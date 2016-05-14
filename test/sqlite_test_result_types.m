function sqlite_test_result_types

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );

    %% Create new on-disc database

    database  = 'my_testdb';    % name of the database file
    table     = 'test_table';   % name of the table

    NumOfSamples = 10;  % amount of records to create

    ruler    = [repmat( '-', 1, 60 ), '\n'];  % ruler

    
    % delete existing database file if any
    try
        if exist( database, 'file' )
            delete (database);
        end
    catch
        error( 'Unable to delete database' );
    end

    % Open (create) database
    mksqlite('open', database);
    mksqlite('PRAGMA synchronous = OFF');
    
    % result types may be:
    % 0: array of structs
    % 1: struct of arrays
    % 2: (cell) matrix
    mksqlite('result_type', 0);  % needless, since is default

    % create table
    % take a look at the boolean field (you would surely never
    % name a column like that) and see later, how mksqlite handles
    % invalid MATLAB field name characters.
    mksqlite( ['CREATE TABLE ' table        , ...
               '  ( Entry         CHAR(32), ' , ...
               '    BigFloat      DOUBLE, '   , ...
               '    SmallFloat    FLOAT, '    , ...
               '    Value         INT, '      , ...
               '    Chars         TINYINT, '  , ...
               '    "0/1-Boolean" BIT, '      , ... 
               '    ManyChars     CHAR(255) ) '] );

    fprintf( ruler )
    fprintf( 'Creating %d records in one single transaction\n', NumOfSamples );
    
    ManyChars = repmat( '1234567890', 1, 20 );
    
    %% Create datasets in one transaction
    tic;
    mksqlite('begin');

    for idx = 1:NumOfSamples
        mksqlite( ['INSERT INTO ' table, ...
                   ' (Entry, BigFloat, ManyChars) ', ...
                   '  VALUES(?,?,?)'], ...
                   sprintf('Entry_%d', idx), idx, ManyChars );
    end

    mksqlite('commit');
    toc
    fprintf ('done.\n');

    fprintf ('Query amount of records\n')
    res = mksqlite(['select count(*) as count from ' table]);
    fprintf ('select count(*) returned %d\n', res.count);


    fprintf( ruler )

    %% Read all records as array of structs (default)
    tic;
    mksqlite( 'result_type', 0 );  % array of structs
    
    % Introducing two further return values:
    % 1.: res_count is the numer of records (rows) returned
    % 2.: col_names is a cell array containing the (original) column 
    %     names. Keep in mind, that MATLAB struct fields have naming 
    %     restrictions and may thus differ from original column names.
    %
    % note: each of following queries fetch the column "value" twice
    %       to show how mksqlite handles duplicate fields
    [res, res_count, col_names] = mksqlite( ['SELECT *,value FROM ' table] )
    a = toc;
    fprintf( 'ready, %f seconds = %d records per second\n\n', a, int32(NumOfSamples/a) );

    %% Read all records as struct of arrays
    tic;
    mksqlite( 'result_type', 1 );  % struct of arrays
    [res, res_count, col_names] = mksqlite(['SELECT *,value FROM ' table])
    a = toc;
    fprintf ('ready, %f seconds = %d records per second\n\n', a, int32(NumOfSamples/a));

    %% Read all records as cell array/matrix
    tic;
    mksqlite( 'result_type', 2 );  % (cell) matrix
    [res, res_count, col_names] = mksqlite(['SELECT *,value FROM ' table])
    a = toc;
    fprintf( 'ready, %f seconds = %d records per second\n', a, int32(NumOfSamples/a) );

    fprintf('done.\n');

    %% Close database
    mksqlite('close');
