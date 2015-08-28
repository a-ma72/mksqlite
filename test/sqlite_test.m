function sqlite_test

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );

    database = 'my_testdb';   % database file name
    table    = 'test_table';  % sql table name
    
    ruler    = [repmat( '-', 1, 60 ), '\n'];  % ruler


    NumOfSamples = 100000;    % number of records in dataset

    % delete database file, if it still exists
    try
        delete( database );
    catch
        error( 'Unable to delete database' );
    end

    % open (creates a new) database
    mksqlite( 'open', database );
    % with synchronous OFF, SQLite continues without syncing 
    % as soon as it has handed data off to the operating system
    mksqlite( 'PRAGMA synchronous = OFF' );

    % create table
    fprintf( 'Create new on-disc database\n' );
    mksqlite( ['CREATE TABLE ' table        , ...
               '  ( Entry       CHAR(32), ' , ...
               '    BigFloat    DOUBLE, '   , ...
               '    SmallFloat  FLOAT, '    , ...
               '    Value       INT, '      , ...
               '    Chars       TINYINT, '  , ...
               '    Boolean     BIT, '      , ... 
               '    ManyChars   CHAR(255) ) '] );

    fprintf( ruler );

    %% Speed test: create records in one single translation
    fprintf( 'Create %d records in one single transaction\n', NumOfSamples );
    ManyChars = repmat( '1234567890', 1, 20 );

    tic;
    mksqlite( 'begin' );  % start transaction creation

        for idx = 1:NumOfSamples
          mksqlite( ['INSERT INTO ', table                     , ...
                     ' (Entry, BigFloat, ManyChars) '          , ...
                     '  VALUES('                               , ...
                                  sprintf( '"Entry_%d"', idx ) , ...
                                  ','                          , ...
                                  num2str(idx)                 , ...
                                  ','                          , ...
                                  '"', ManyChars, '"'          , ...
                     '         )'                              ] );
        end
        
    mksqlite( 'commit' );  % finalize transaction
    fprintf( 'done.\n' );
    toc

    %% Some sql statistics:
    
    % Counting records in table
    fprintf( 'Query amount of records\n' )
    res = mksqlite( ['SELECT COUNT(*) AS count FROM ' table] );
    fprintf( 'SELECT COUNT(*) returned %d\n', res.count );

    % Cumulating selected record fields
    fprintf( 'Cumulate all values between 10 and 75\n' );
    res = mksqlite( ['SELECT SUM(BigFloat) AS cumsum FROM ' table, ...
                     ' WHERE BigFloat BETWEEN 10 AND 75'] );
    fprintf( 'Sum is %d\n', res.cumsum );

    % Speed test: fetching all records
    fprintf( 'Read all records as array of structs\n' );
    tic;
    res = mksqlite( ['SELECT * FROM ' table] );
    a = toc;
    fprintf( 'ready, %f seconds = %d records per second\n\n\n', ...
             a, int32(NumOfSamples/a) );

    % done with on-disc database tests
    mksqlite('close');
    
    % -----------------------------------------------------------------
    
    %% create an in-memory database now and copy all records from on-disc dataset into it
    fprintf( 'Create new in-memory database\n' );
    fprintf( ruler );
    
    % Create an in-memory database ( mksqlite('open', '') does the same job )
    mksqlite( 'open', ':memory:' );

    % Attach previous on-disc database
    mksqlite( ['ATTACH DATABASE "', database '" AS original'] );

    % Copy on-disc database into on-memory database
    fprintf( 'copy database contents in one transaction\n' );
    
    mksqlite( 'begin' );  % begin transaction
    
        % duplicate each table via sql schema
        tables = mksqlite( 'SELECT name FROM original.sqlite_master WHERE type = "table" ' );
        for idx = 1:length( tables )
            mksqlite( ['CREATE TABLE "' tables(idx).name '" ', ...
                       'AS SELECT * FROM original."', tables(idx).name '"'] );
        end

        % duplicate all indexing tables via schema
        tables = mksqlite( 'SELECT sql FROM original.sqlite_master WHERE type = "index" ');
        for idx = 1:length( tables )
            mksqlite( tables(idx).sql );
        end
    
    mksqlite('commit');  % finalize transaction
    
    % detach original (on-disc) database
    mksqlite( 'DETACH original' );
    fprintf( 'Copying done.\n' );

    %% Some sql statistics again:
    
    % counting records in table
    fprintf( 'Query record count\n' )
    res = mksqlite( ['SELECT COUNT(*) AS count FROM ' table] );
    fprintf( 'SELECT COUNT(*) returned %d\n', res.count);

    % cumulating selected record fields
    fprintf( 'Cumulate all values between 10 and 75\n' );
    res = mksqlite( ['SELECT SUM(BigFloat) AS cumsum FROM ' table, ...
                     ' WHERE BigFloat BETWEEN 10 AND 75'] );
    fprintf( 'Sum is %d\n', res.cumsum );
    
    % speed test: fetching all records
    fprintf( 'Read all records as array of structs\n' );
    tic;
    res = mksqlite( ['SELECT * FROM ' table] );
    a = toc;
    fprintf( 'ready, %f seconds = %d records per second\n', ...
             a, int32(NumOfSamples/a) );

    %% Close database
    mksqlite('close');

    fprintf( 'done.\n' );
