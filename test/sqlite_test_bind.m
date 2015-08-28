function sqlite_test_bind

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

    % create records
    % uses "cell expansion" for command shortening!
    for i = 1:size( mydata, 1 )
        mksqlite( 'INSERT INTO demo VALUES (?,?,?,?)', mydata{i,:} );
    end

    %% Take a screenshot (figure) as RGB-matrix...
    h = figure;
    set( h, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
    x = linspace( 0, 2*pi, 20 );
    plot( x, sin(x), 'r-', 'linewidth', 2 );
    legend on
    F = getframe(h);
    delete(h);
    data = F.cdata;

    % ... write back screenshot information as BLOB with additional 
    % array size parameters
    mksqlite( 'INSERT INTO demo VALUES (?,?,?,?)', ...
              size( data, 1 ), size( data, 2 ), size( data, 3 ), data );



    
    % ------------------------------------------------------------------

    
    %% Read back all records
    fprintf( 'Restore BLOB records...\n\n' )

    query = mksqlite( 'SELECT * FROM demo' );

    fprintf( '---> Empty array: ' ), ...
             query(1).Data

    fprintf( '---> 10 random numbers between 0 and 1: ' ), ...
             typecast( query(2).Data, 'double' )

    fprintf( '---> Text: ' ), ...
             cast( query(3).Data, 'char' )

    fprintf( '---> Image: (see figure) \n\n' )

    % Reshape vector from 4th row as multidimensional array (RGB matrix) back:
    img = reshape( query(4).Data, query(4).Col_1, query(4).Col_2, query(4).Col_3 );

    % display image got from database
    h = image( img );
    axis off
    set( gcf, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
    drawnow

    % bring figure on top
    try
        warning( 'off', 'MATLAB:HandleGraphics:ObsoletedProperty:JavaFrame' );
        jh = get( h, 'JavaFrame' );
        jh.fFigureClient.getWindow.setAlwaysOnTop( true );
        jh.fFigureClient.getWindow.setVisible( true );
    catch
        % ignore errors
    end

    mksqlite( 'close' );
