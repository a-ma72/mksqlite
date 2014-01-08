function sqlite_test_bind_typed ()
  
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Datenbank und Inhalt erzeugen  %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  mksqlite( 'open', ':memory:' ); % "in-memory"-Datenbank
  mksqlite( 'typedBLOBs', 1 ); % Typisierung der BLOBs einschalten

  %          |Vorname   |Name         |Ort          |Testdaten
  mydata = { ...
             'Gunther', 'Meyer',      'München',    []; ...
             'Holger',  'Michelmann', 'Garbsen',    rand( 1, 10 ); ...
             'Knuth',   'Almeroth',   'Wehnsen',    'Arbeitskollege' ...
           }; 
  
  % Tabelle anlegen
  mksqlite( 'create table demo (Col_1, Col_2, Col_3, Data)' );
  
  % Datenfelder erzeugen
  for i = 1:size( mydata, 1 )
    mksqlite( 'insert into demo values (?,?,?,?)', mydata{i,:} );
  end

  % Bildschirmausschnitt (figure) als RGB-Matrix erzeugen...
  h = figure;
  set( h, 'units', 'normalized', 'position', [0.5,0.5,0.2,0.2] );
  x = linspace(0,2*pi,20);
  plot( x, sin(x), 'r-', 'linewidth', 2 );
  legend on
  F = getframe(h);
  delete(h);
  data = F.cdata;
  
  % ... und als BLOB in die Datenbank schreiben
  mksqlite( 'insert into demo values (?,?,?,?)', ...
            size( data, 1 ), size( data, 2 ), size( data, 3 ), data );
  
          
          
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%        
  % Daten aus der Datenbank wieder auslesen %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%        
  
  clc
  fprintf( 'BLOB Datentypen mit ursprünglichen Datentypen aus der Datenbank holen...\n\n' )
  
  query = mksqlite( 'select * from demo' );
  
  fprintf( '---> Leeres Array: ' ), ...
           query(1).Data
         
  fprintf( '---> 10 Zufallszahlen zwischen 0 und 1: ' ), ...
           typecast( query(2).Data, 'double' )
         
  fprintf( '---> Text: ' ), ...
           cast( query(3).Data, 'char' )
         
  fprintf('---> Image: (s.Figure) \n\n')
  
  % Vektor in mehrdimensionale RGB Matrix zurückwandeln:
  img = query(4).Data;
  
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