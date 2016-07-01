function sqlite_test_access_mode

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
  
    
    % name of test database
    db_name = 'sql_test_access2.db';

    % delete database file, if exists
    if exist( db_name, 'file' )
        delete( db_name );
    end

    %% Without write access the following command will fail...
    try
        s = 'mksqlite( ''open'', db_name, ''RO'' );';
        fprintf( [s,'\n'] );
        eval( s ); % Try to create database with read-only-access (single-thread)
        error( 'Test failed!' )
    catch
        % test succeeds, if we ran into this block
        fprintf( 'Catch block: Unable to create database with read-only-access, test succeeded!\n' );
    end

    % mksqlite( 'open', db_name ) opens the database with default access rights
    % and is the same as:
    % mksqlite( 'open', db_name, 'RWC', 'Single' )

    %% Create database with some records
    mksqlite( 'open', db_name ); % Open with read/write-access (single-thread)

    % create a table with a single column and insert one record
    mksqlite( 'CREATE TABLE data (Col_1)' );
    mksqlite( 'insert into data (Col_1) values( "A String")' );
    mksqlite( 'close' );
    fprintf( 'Database with one record (%s) has been created\n', db_name );

    %% Now since the database is existing, we're able to open it with read-only access:
    fprintf( 'Open database with read-only access:\n' );
    s = 'mksqlite( ''open'', db_name, ''RO'' );'; % Open read-only (single-thread)
    fprintf( [s,'\n'] );
    eval( s );

    %% Write access to the database should not be possible:
    try
        s = 'mksqlite( ''insert into data (Col_1) values( "A String")'' );';
        fprintf( [s,'\n'] );
        eval( s );
        error( 'Test failed!' )
    catch
        % test succeeds, if we ran into this block
        fprintf( 'Catch block: Write access denied, test succeeded!\n' );
    end

    %% Now open database in multithreading mode (without further tests
    fprintf( 'Open database in multithreading modes...\n');
    mksqlite( 0, 'close' ); % Close all open databases
    mksqlite( 'open', db_name, 'RW', 'Multi' ); % Open in multithread-mode (File must exist! 'RWC' otherwise)
    mksqlite( 0, 'close' ); % Close all open databases
    mksqlite( 'open', db_name, 'RW', 'Serial' ); % Open in multithread-mode (serialized)
    mksqlite( 0, 'close' ); % Close all open databases

    % No error should have occured
    fprintf( 'Tests successfully done.\n' );
