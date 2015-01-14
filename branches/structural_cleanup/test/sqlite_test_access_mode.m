function sqlite_test_access_mode
  clear all
  mksqlite( 'version mex' ); % Discard startup information
  clc
  
  db_name = 'sql_test_access2.db';
  
  if exist( db_name, 'file' )
    delete( db_name );
  end
  
  try
    % Without write access the following command will fail...
    s = 'mksqlite( ''open'', db_name, ''RO'' );';
    fprintf( [s,'\n'] );
    eval( s ); % Try to create database with read-only-access (single-thread)
  catch
    fprintf( 'Catch block: Unable to create database with read-only-access, test succeeded!\n' );
  end
  
  % mksqlite( 'open', db_name ) opens the database with default access rights
  % and is the same as:
  % mksqlite( 'open', db_name, 'RWC', 'Single' )
  
  % Create database with some records
  mksqlite( 'open', db_name ); % Open with read/write-access (single-thread)
  
  mksqlite( 'create table data (Col_1)' );
  mksqlite( 'insert into data (Col_1) values( "A String")' );
  mksqlite( 0, 'close' );
  fprintf( 'Database with one record (%s) has been created\n', db_name );

  % Now since the database is existing, we're able to open it with read-only access:
  fprintf( 'Open database with read-only access:\n' );
  s = 'mksqlite( ''open'', db_name, ''RO'' );'; % Open read-only (single-thread)
  fprintf( [s,'\n'] );
  eval( s );
  
  % Write into database is not possible:
  try
    s = 'mksqlite( ''insert into data (Col_1) values( "A String")'' );';
    fprintf( [s,'\n'] );
    eval( s );
  catch
    fprintf( 'Catch block: Write access denied, test succeeded!\n' );
  end
  
  fprintf( 'Open database in multithreading mode...\n');
  mksqlite( 0, 'close' ); % Close all open databases
  mksqlite( 'open', db_name, 'RW', 'Multi' ); % Open in multithread-mode
  mksqlite( 0, 'close' ); % Close all open databases
  mksqlite( 'open', db_name, 'RW', 'Serial' ); % Open in multithread-mode (serialized)
  mksqlite( 0, 'close' ); % Close all open databases
  
  fprintf( 'Tests done.\n' );
end