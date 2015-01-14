function sqlite_test_bind

  clear all
  close all
  clc
  mksqlite('version mex');
  
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Create database with some records  %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  fprintf( 'Creating in-memory database...\n' );
  mksqlite( 'open', ':memory:' ); % "in-memory"-database
  
  %          |First name |Last name    |City         |Random data
  mydata = { ...
             'Gunther',  'Meyer',      'München',    []; ...
             'Holger',   'Michelmann', 'Garbsen',    rand( 1, 10 ); ...
             'Knuth',    'Almeroth',   'Wehnsen',    'Arbeitskollege' ...
           }; 
  
  % Create table
  mksqlite( 'create table demo (Col_1, Col_2, Col_3, Data)' );
  
  % Create records
  for i = 1:size( mydata, 1 )
    mksqlite( 'insert into demo values (?,?,?,?)', mydata{i,:} );
  end

  % Take screenshot (figure) as RGB-matrix...
  h = figure;
  set( h, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
  x = linspace(0,2*pi,20);
  plot( x, sin(x), 'r-', 'linewidth', 2 );
  legend on
  F = getframe(h);
  delete(h);
  data = F.cdata;
  
  % ... write back as BLOB
  mksqlite( 'insert into demo values (?,?,?,?)', ...
            size( data, 1 ), size( data, 2 ), size( data, 3 ), data );
  
          
          
  %%%%%%%%%%%%%%%%%
  % Read database %
  %%%%%%%%%%%%%%%%%
  
  fprintf( 'Restore BLOB records...\n\n' )
  
  query = mksqlite( 'select * from demo' );
  
  fprintf( '---> Empty array: ' ), ...
           query(1).Data
         
  fprintf( '---> 10 random numbers between 0 and 1: ' ), ...
           typecast( query(2).Data, 'double' )
         
  fprintf( '---> Text: ' ), ...
           cast( query(3).Data, 'char' )
         
  fprintf( '---> Image: (see figure) \n\n' )
  
  % Reshape vector as multidimensional array (RGB matrix):
  img = reshape( query(4).Data, query(4).Col_1, query(4).Col_2, query(4).Col_3 );
  
  h = image( img );
  axis off
  set( gcf, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
  drawnow
  
  try
    warning( 'off', 'MATLAB:HandleGraphics:ObsoletedProperty:JavaFrame' );
    jh = get( h, 'JavaFrame' );
    jh.fFigureClient.getWindow.setAlwaysOnTop( true );
    jh.fFigureClient.getWindow.setVisible( true );
  catch
  end
  
  mksqlite( 'close' );
end