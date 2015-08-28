function sqlite_test_md5_and_packaging

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
    
    
    mksqlite( 'open', '' );
  
    %% MD5 Test Suite
    % (ref: http://tools.ietf.org/html/rfc1321)

    % Compare md5 hashing values with the reference values, listed from the above source
    clc
    fprintf( 'MD5 Test Suite:\n\n' );
    q = mksqlite( 'select MD5(?) as MD5', 'a' );
    fprintf( '"a":\n%s\n%s\n\n', lower( q.MD5 ), '0cc175b9c0f1b6a831c399e269772661' );
    q = mksqlite( 'select MD5(?) as MD5', 'abc' );
    fprintf( '"abc":\n%s\n%s\n\n', lower( q.MD5 ), '900150983cd24fb0d6963f7d28e17f72' );
    q = mksqlite( 'select MD5(?) as MD5', 'message digest' );
    fprintf( '"message digest":\n%s\n%s\n\n', lower( q.MD5 ), 'f96b697d7cb7938d525a2f31aaf161d0' );
    q = mksqlite( 'select MD5(?) as MD5', 'abcdefghijklmnopqrstuvwxyz' );
    fprintf( '"abcdefghijklmnopqrstuvwxyz":\n%s\n%s\n\n', lower( q.MD5 ), 'c3fcd3d76192e4007dfb496cca67e13b' );
    q = mksqlite( 'select MD5(?) as MD5', 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789' );
    fprintf( '"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789":\n%s\n%s\n\n', lower( q.MD5 ), 'd174ab98d277d9f5a5611c2c9f419d9f' );
    q = mksqlite( 'select MD5(?) as MD5', '12345678901234567890123456789012345678901234567890123456789012345678901234567890' );
    fprintf( '"12345678901234567890123456789012345678901234567890123456789012345678901234567890":\n%s\n%s\n\n', lower( q.MD5 ), '57edf4a22be3c955ac49da2e2107b67a' );


    %% Packaging time for random numbers

    fprintf( 'BLOSC test suite:\n\n' );

    compressor = 'blosclz';   % blosclz, lz4, lz4hc, qlin16, qlog16

    % Using typed BLOBs with compression feature
    mksqlite( 'typedBLOBs', 2 );
    mksqlite( 'compression_check', 1 );

    %% 1.000.000 random numbers

    data = randn( 1e6, 1 );

    for level = 9:-1:0
        % set compressor and compression level
        mksqlite( 'compression', compressor, level );

        % measuring
        q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                       '       BDCUnpackTime(?) AS t_unpack, ', ...
                       '       BDCRatio(?) AS ratio, ', ...
                       '       ? as data'], ...
                       data, data, data, data );

        % display results
        fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
                  level, q.t_pack, q.t_unpack, q.ratio*100 );
    end

    %% Same random numbers, cumulated

    data = cumsum( data );

    for level = 9:-1:0
        % set compressor and compression level
        mksqlite( 'compression', compressor, level );

        % measuring
        q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                       '       BDCUnpackTime(?) AS t_unpack, ', ...
                       '       BDCRatio(?) AS ratio, ', ...
                       '       ? as data'], ...
                       data, data, data, data );

        % display results
        fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
                  level, q.t_pack, q.t_unpack, q.ratio*100 );
    end

    %% Same random numbers, half constant 0

    data( 1:500000 ) = 0;

    for level = 9:-1:0
        % set compressor and compression level
        mksqlite( 'compression', compressor, level );

        % measuring
        q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                       '       BDCUnpackTime(?) AS t_unpack, ', ...
                       '       BDCRatio(?) AS ratio, ', ...
                       '       ? as data'], ...
                       data, data, data, data );

        % display results
        fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
                  level, q.t_pack, q.t_unpack, q.ratio*100 );
    end


    %% QLIN16/QLOG16 test suite

    fprintf( '\nLossy compression "QLIN16":\n(Unique resolution over entire value range)\n' );

    level = 1; % Always 1, ratio is always constant 1:4
    data = cumsum( randn( 1e6, 1 ) );

    %% Measuring QLIN16
    
    % set compressor and compression level
    mksqlite( 'compression', 'QLIN16', level );
    q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                   '       BDCUnpackTime(?) AS t_unpack, ', ...
                   '       BDCRatio(?) AS ratio, ', ...
                   '       ? as data'], ...
                   data, data, data, data );

    %% Display results
    fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
              level, q.t_pack, q.t_unpack, q.ratio*100 );

    figure
    plot( data, 'r-', 'linewidth', 2, 'displayname', 'Original' ), hold all
    plot( q.data, 'k-', 'displayname', 'Copy' )
    grid
    legend( 'show' )
    title( 'QLIN16' );

    fprintf( '\nLossy compression "QLOG16":\n(Lesser resolution for bigger values)\n' );

    level = 1; % Always 1, ratio is always constant 1:4
    data = data - min(data); % Only positive numbers allowed (and 0, NaN, +Inf und -Inf)
    
    %% Measuring QLOG16

    % set compressor and compression level
    mksqlite( 'compression', 'QLOG16', level );
    q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                   '       BDCUnpackTime(?) AS t_unpack, ', ...
                   '       BDCRatio(?) AS ratio, ', ...
                   '       ? as data'], ...
                   data, data, data, data );

    %% Display results
    fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
              level, q.t_pack, q.t_unpack, q.ratio*100 );

    figure
    plot( data, 'r-', 'linewidth', 2, 'displayname', 'Original' ), hold all
    plot( q.data, 'k-', 'displayname', 'Copy' )
    grid
    legend( 'show' )
    title( 'QLOG16' );

    %% Packaging time for screenshot data
    
    % Screenshot (figure) as (RGB-matrix)
    fprintf( '\nScreenshot data:\n' );

    h = figure;
    set( h, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
    x = linspace(0,2*pi,20);
    plot( x, sin(x), 'r-', 'linewidth', 2 );
    legend on
    F = getframe(h);
    delete(h);
    data = F.cdata;

    %% Measuring

    level = 9;
    mksqlite( 'compression', compressor, level );
    
    q = mksqlite( ['SELECT BDCPackTime(?) AS t_pack, ', ...
                   '       BDCUnpackTime(?) AS t_unpack, ', ...
                   '       BDCRatio(?) AS ratio'], ...
                   data, data, data );

    %% Display results
    fprintf( 'Level %d: pack(%gs), unpack(%gs), ratio(%g%%)\n', ...
              level, q.t_pack, q.t_unpack, q.ratio*100 );

    %% Close database
    mksqlite( 'close' );
