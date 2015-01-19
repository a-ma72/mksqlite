function sqlite_test_bind_typed_compressed

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
    
    database = 'demo.db';

    mksqlite( 'open', database );
    mksqlite( 'drop table if exists demo' );

    % Create table
    mksqlite( ['CREATE TABLE demo '   , ...
               ' ( id PRIMARY KEY, '  , ...
               '   type, '            , ...
               '   data, '            , ...
               '   size, '            , ...
               '   level, '           , ...
               '   pack_ratio, '      , ...
               '   pack_time, '       , ...
               '   unpack_time, '     , ...
               '   md5_hash '         , ...
               ' )'] );


    fprintf( 'Please wait, while generating 500 entries...\n' );
    for n = 1:500
        compression_level = randi(10)-1;  % randomly choose compression rate...
        use_typed_blobs   = randi(2)-1;   % and whether or not compression is used

        if ~use_typed_blobs
          compression_level = 0;  % no compression
        end

        data = [];

        % randomly generate 3 types of data:
        % 1: NxN matrix with N is 1..10 with random numbers
        % 2: vector with length 10000..20000 with random numbers
        % 3: like type 2, but as cumulated sum

        while isempty( data )
          type = randi(3);
          switch type
            case 1
              if use_typed_blobs % Arrays assume typed blobs
                data = randn( 1 + randi( 9 ) );
              end
            case 2
              data = randn( 1e4 + randi(1e4), 1);
            case 3
              data = cumsum( randn( 1e4 + randi(1e4), 1) );
          end
        end

        nElements = numel( data );

        % You're not limited in mixing compressed and uncompressed data in the data base!
        mksqlite( 'typedBLOBs', use_typed_blobs ); % Using typed BLOBs
        
        % Set compression for typed BLOBs
        mksqlite( 'compression', 'blosclz', compression_level ); 

        % insert data
        mksqlite( ['INSERT INTO demo ', ...
                   '(id, type, data, size, level) values (?,?,?,?,?)'], ...
                   n, type, data, nElements, compression_level );

        % note that BDC...() SQL user functions depend on current
        % compression settings!
        mksqlite( ['UPDATE demo SET ', ...
                   '  pack_time   = BDCPackTime(data), '   , ...
                   '  unpack_time = BDCUnpackTime(data), ' , ...
                   '  pack_ratio  = BDCRatio(data), '      , ...
                   '  md5_hash    = MD5(data) '            , ...
                   ' WHERE id = ?'], n );
                 
        if length(dbstack) == 1
            clc, fprintf( '%d\n', n );
        end
    end

    % Compression of "real" random numbers is generally poor:
    query = mksqlite( ['SELECT type, size, level, pack_ratio, ', ...
                       'pack_time, unpack_time FROM demo ', ...
                       'WHERE type<3 AND level>0'] );
                     
    % show histogram 
    figure, hist( [query.pack_ratio]', 50 )

    % "natural" series, as in a measurement are slightly  
    % better to compress
    min_level = 0;
    query = mksqlite( ['SELECT type, size, level, pack_ratio, ', ...
                       'pack_time, unpack_time FROM demo ', ...
                       'WHERE type=3 AND level>?'], min_level );

    figure, hist( [query.pack_ratio]', 50 )
    
    % by the way: "lossy" compressors (QLIN16 and QLOG16) have a 
    % constant compression rate of 25%

    % close database
    mksqlite( 'close' );
