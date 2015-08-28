function sqlite_test_bind_wrapping

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );

    
    %% Create a database with some records
    fprintf( 'Creating in-memory database...\n' );
    mksqlite( 'open', ':memory:' ); % "in-memory"-database

    %          |First name |Last name    |City         |Random data
    mydata = { ...
               'Gunther',  'Meyer',      'Munich',     []; ...
               'Holger',   'Michelmann', 'Garbsen',    rand( 1, 10 ); ...
               'Knuth',    'Almeroth',   'Wehnsen',    'coworker' ...
             }; 

    % create table
    mksqlite( 'CREATE TABLE demo (Col_1, Col_2, Col_3, Data)' );
    
    %
    mksqlite( 'typedBLOBs', 1 );     % switch "typed BLOBs" on (dimension and numeric type will be stored)
    mksqlite( 'param_wrapping', 1 ); % enable more parameters then needed to fire multiple commands

    % create records
    % cell matrix must be transposed, so that mydata{:} delivers a sequence 
    % of recordsets
    mksqlite( 'INSERT INTO demo VALUES (?,?,?,?)', mydata' );
    
    
    %% Test how mksqlite handles incomplete bind parameters
    fprintf( 'Testing how mksqlite handles incomplete bind parameters... ' );
    errcount = 0;
    try
        % call with less data should fail when "parameter wrapping" is on
        mksqlite( 'INSERT INTO demo VALUES (?,?,?,?)', mydata{1:end-1} );
    catch
        errcount = errcount + 1;
    end

    try
        % parameters for at least one record must be given, 
        % when "parameter wrapping" is on
        mksqlite( 'INSERT INTO demo VALUES (?,?,?,?)', mydata{1:3} );
    catch
        errcount = errcount + 1;
    end
    
    if errcount == 2
        fprintf( 'succeeded.\n' );
    else
        fprintf( 'failed.\n' );
    end
    
    clear errcount

    
    % ------------------------------------------------------------------

    
    %% Read back all records
    fprintf( 'Restore BLOB records...\n\n' )

    query = mksqlite( 'SELECT * FROM demo' );

    fprintf( '---> Empty array: ' ), ...
             query(1).Data

    fprintf( '---> 10 random numbers between 0 and 1: ' ), ...
             query(2).Data

    fprintf( '---> Text: ' ), ...
             query(3).Data


    mksqlite( 'close' );
