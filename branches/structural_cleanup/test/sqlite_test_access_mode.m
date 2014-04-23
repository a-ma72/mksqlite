function sqlite_test_access_mode ()
  clear all
  mksqlite( 'version mex' ); % Information bei erstem Funktionsaufruf verwerfen
  clc
  
  db_name = 'sql_test_access2.db';
  
  if exist( db_name, 'file' )
    delete( db_name );
  end
  
  try
    % Ohne das Recht die Datenbank erzeugen zu dürfen, misslingt dieser
    % Befehl:
    s = 'mksqlite( ''open'', db_name, ''RO'' );';
    fprintf( [s,'\n'] );
    eval( s ); % Datenbank mit Lese-/Schreibrecht erzeugen (Single-Threaded)
  catch
    fprintf( 'Catch block: Datenbank konnte mit Read-Only Zugriffsrecht nicht erzeugt werden. Test erfolgreich\n' );
  end
  
  % mksqlite( 'open', db_name ) öffnet mit Defaultwerten und entspricht
  % einem Aufruf wie:
  % mksqlite( 'open', db_name, 'RWC', 'Single' )
  
  % Datenbank mit Inhalt erzeugen
  mksqlite( 'open', db_name ); % Datenbank erzeugen und mit Lese-/Schreibrecht öffnen (Single-Threaded)
  
  mksqlite( 'create table data (Col_1)' );
  mksqlite( 'insert into data (Col_1) values( "A String")' );
  mksqlite( 0, 'close' );
  fprintf( 'Datenbank mit Inhalt (%s) wurde erzeugt\n', db_name );

  % Da die Datenbank jetzt existiert, kann sie auch im Read-Only Modus
  % geöffnet werden:
  fprintf( 'Datenbank im Read-Only Modus öffnen\n' );
  s = 'mksqlite( ''open'', db_name, ''RO'' );'; % Datenbank mit Nur-Leserecht öffnen (Single-Threaded)
  fprintf( [s,'\n'] );
  eval( s );
  
  % Schreibend kann jedoch nicht zugegriffen werden:
  try
    s = 'mksqlite( ''insert into data (Col_1) values( "A String")'' );';
    fprintf( [s,'\n'] );
    eval( s );
  catch
    fprintf( 'Catch block: Schreibzugriff war nicht möglich, Test erfolgreich\n' );
  end
  
  fprintf( 'Öffne Datenbank in Multithreading Modi...\n');
  mksqlite( 0, 'close' ); % Alle Datenbanken schließen
  mksqlite( 'open', db_name, 'RW', 'Multi' ); % Datenbank im Multithread-Modus öffnen
  mksqlite( 0, 'close' ); % Alle Datenbanken schließen
  mksqlite( 'open', db_name, 'RW', 'Serial' ); % Datenbank im Multithread-Modus (Serialized) öffnen
  mksqlite( 0, 'close' ); % Alle Datenbanken schließen
  
  fprintf( 'Tests abgeschlossen\n' );
end