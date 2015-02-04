function sqlite_test_big_int ()

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
    
    format long

    % create in-memory database
    mksqlite( 'open', '' );

    % create simple table with one field of integer type
    mksqlite( 'CREATE TABLE bigint (x INT)' );

    % mksqlite switches automatically to 64-bit integer return type, 
    % if the value can't be lostless represented by double type
    for i = 1:2
        if i == 1
            % small integer value as bigint
            fprintf( 'Small value as bigint\n' );
            value_orig = uint8( hex2dec( ['CC'; 'CC'; 'CC'; 'CC'; '00'; '00'; '00'; '00'] ) );
            value_orig = typecast( value_orig', 'int64' );
        else
            % huge integer value as bigint
            fprintf( 'Huge value as bigint\n' );
            value_orig = uint8( hex2dec( ['CC'; 'CC'; 'CC'; 'CC'; 'CC'; 'CC'; 'CC'; '7C'] ) );
            value_orig = typecast( value_orig', 'int64' );
        end

        % output value contents to be stored in database
        fprintf( 'MATLAB original:\n\tdata type: %s, and value: %ld\n', ...
                 class( value_orig ), value_orig );
                 
        % BTW: 
        % 'uint64' type is not supported by SQLite and would thus run
        % into an error.
        mksqlite( 'INSERT INTO bigint VALUES (?)', value_orig );

        % refetch value from database
        value_fetched = mksqlite( 'SELECT x, PRINTF("%d",x) AS x_dec FROM bigint' );

        % output data, how it is represented by SQL
        fprintf( 'After fetching from database:\n\tdata type: %s, and value: %s', ...
                 class( value_fetched.x ), value_fetched.x_dec );

        fprintf( '\n\n' );
        
        % empty database
        mksqlite( 'DELETE FROM bigint' ); 
    end

    mksqlite( 'close' );
