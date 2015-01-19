function sqlite_test_blosc

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
    
    database = 'demo.db';
    
    % delete existing on-disc database if any
    if exist( database, 'file' )
        delete( database );
    end

    % create a new database file
    mksqlite( 'open', database );

    % create table
    mksqlite( ['CREATE TABLE demo '   , ...
               ' ( id PRIMARY KEY, '  , ...
               '   type, '            , ...  % indicating which compressor was used
               '   data, '            , ...  % packed random data
               '   pack_ratio, '      , ...  % compression ratio
               '   pack_time, '       , ...  % compression time
               '   unpack_time '     , ...  % decompression time
               ' )'] );
             
    compressors = { 'lz4', 'lz4hc', 'blosclz' };  % lossless compressors

    typed_blob_mode   = 2; % Use typed BLOBs with compression feature
    compression_level = 9; % level, range from 0=off to 9=max

    % You're not limited in mixing compressed and uncompressed data in the data base!
    mksqlite( 'typedBLOBs', typed_blob_mode ); % switch typed BLOBs mode
    mksqlite( 'compression_check', 1 ); % set check for compressed data on

    % generating recordset
    fprintf( 'Please wait, while generating 500 entries...\n' );
    for n = 1:500

        data = cumsum( randn( 10000, 1) );  % some random data

        % set randomly one compression
        type = randi(size(compressors)-1) + 1;
        compressor = compressors{type};
        mksqlite( 'compression', compressor, compression_level ); 

        % insert random data
        mksqlite( ['INSERT INTO demo ', ...
                   ' (id, type, data) ', ...
                   ' VALUES (?,?,?)'], ...
                   n, type, data );

        % note that BDC...() SQL user functions depend on current
        % compression settings!
        mksqlite( ['UPDATE demo SET ', ...
                   '  pack_time   = BDCPackTime(data), '   , ...
                   '  unpack_time = BDCUnpackTime(data), ' , ...
                   '  pack_ratio  = BDCRatio(data) '   , ...
                   ' WHERE id = ?'], n );

        % Display progress state
        if n > 1
            fprintf( '%c', ones(3,1)*8 );
        end

        fprintf( '%03d', n );
    end
    
    fprintf( '\n\n\n' );
    
    
    % display some statistics about blosc compressors
    for i = 1:3
        fprintf( 'Statistics for %s with max. compression rate:\n', compressors{i} );
        mksqlite( ['SELECT ', ...
                   '  MIN(pack_time),   AVG(pack_time),   MAX(pack_time), ', ...
                   '  MIN(unpack_time), AVG(unpack_time), MAX(unpack_time), ', ...
                   '  MIN(pack_ratio),  AVG(pack_ratio),  MAX(pack_ratio) ', ...
                   '  FROM demo WHERE type = ?'], i )
    end

    %  close database
    mksqlite( 0, 'close' );
