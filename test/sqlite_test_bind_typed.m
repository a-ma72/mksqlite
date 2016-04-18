function sqlite_test_bind_typed
  
    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );
    

    %% Create an in-memory database
    mksqlite( 'open', ':memory:' );
    mksqlite( 'param_wrapping', 0 );

    % mksqlite stores numeric arrays always as an array of bytes (uint8),
    % since SQLite only supports native byte array (BLOB). When you fetch 
    % a BLOB you will always get your result as byte array of type uint8.
    % With typed BLOBs mksqlite stores additionally informations (dimensions and 
    % numeric type), so it can be returned in the same format as it was before. 
    % No casting or reshaping will be necessary.
    
    mksqlite( 'typedBLOBs', 1 ); 

    %          |First name |Last name    |City         |Random data
    mydata = { ...
               'Gunther',  'Meyer',      'Munich',     []; ...
               'Holger',   'Michelmann', 'Garbsen',    rand( 1, 10 ); ...
               'Knuth',    'Almeroth',   'Wehnsen',    'coworker' ...
             }; 

    % create table
    mksqlite( 'CREATE TABLE demo (Col_1, Col_2, Col_3, Data)' );

    % create records
    % uses MATLAB "cell expansion" for command 
    % shortening: mydata{i,:} expands to 4 arguments
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

    %% Store using typed BLOB feature
    mksqlite( 'INSERT INTO demo (Data) VALUES (?)', data );

    try
        % enable advanced typed BLOBs. So now structures cell arrays 
        % and complex numbers can be stored as BLOB in the database.
        mksqlite( 'typedBLOBs', 2 );
    catch
        % If your MATLAB version doesn't support streaming (serialization)
        % of variables, mksqlite will report an error. mksqlite uses mode 1
        % then. You can read the current mode with:
        %   mode = mksqlite( 'typedBLOBs' );
        % Discarding the fail, because this will be checked next...
    end

    % If your MATLAB version doesn't support streaming, then skip writing
    % structures and complex numbers (nested variables)...
    % (mksqlite( 'typedBLOBs' ) will be 1 then)
    if mksqlite( 'typedBLOBs' ) == 2
        data_complex = struct;
        data_complex.String = 'Name';
        data_complex.Complex = 3+4i;
        data_complex.Cell = { 1:10, 'Text', 1+2i };

        mksqlite( 'INSERT INTO demo (Data) VALUES (?)', data_complex );
    end 


    %% Now read back values
    clc
    fprintf( 'Get BLOB with the original data types from the database...\n\n' )

    query = mksqlite( 'SELECT * FROM demo' );

    fprintf( '---> Empty array: ' ), ...
             query(1).Data

    fprintf( '---> 10 random numbers between 0 and 1: ' ), ...
             query(2).Data

    fprintf( '---> Text: ' ), ...
             query(3).Data

    fprintf('---> Image: (see figure) \n\n')

    % Due to typed BLOBs the image data haven't to be reshaped or casted.
    img = query(4).Data;

    h = image( img );
    axis off
    set( gcf, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
    drawnow

    % bring recent figure to top
    try
        warning( 'off', 'MATLAB:HandleGraphics:ObsoletedProperty:JavaFrame' );
        jh = get( h, 'JavaFrame' );
        jh.fFigureClient.getWindow.setAlwaysOnTop( true );
        jh.fFigureClient.getWindow.setVisible( true );
    catch
    end

    % if streaming is supported, fetch the nested variable
    if mksqlite( 'typedBLOBs' ) == 2
        fprintf( '---> Nested variable: ' );
        query(5).Data
    else
        fprintf( ['\nThis MATLAB version doesn''t support serialization.\n', ...
                  'The test of handling nested variables will be skipped.\n'] );
    end

    mksqlite( 'close' );
