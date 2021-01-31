function sqlite_test_rows_changed()

  mksqlite('open', ':memory:');
  mksqlite('result_type', 2);
  mksqlite('CREATE TABLE tbl (k INTEGER NOT NULL PRIMARY KEY, v INTEGER, t TEXT DEFAULT "empty")' );
  mksqlite('CREATE TRIGGER chng_insert AFTER INSERT ON tbl BEGIN INSERT INTO chng (inserts) VALUES(new.k); END');
  mksqlite('CREATE TRIGGER chng_update AFTER UPDATE ON tbl BEGIN INSERT INTO chng (updates) VALUES(new.k); END');
  mksqlite('CREATE TABLE chng (inserts, updates)');

  clc

  % 3 Datensaetze anlegen
  mksqlite('INSERT INTO tbl VALUES (42, 420, "init:0"), (43, 430, "init:0"), (44, 440, "init:0")')
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( 'INSERT: %d\n', ch );
  show_modified();

  % 1) 1x Replace, 42 (primary key!) bereits vorhanden
  % Replace ersetzt im Gegensatz zu Update immer den kompletten Datensatz,
  % auch wenn die Felder nicht alle angegeben sind!
  mksqlite('INSERT OR REPLACE INTO tbl (k,v) VALUES (42, 421)');
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( '1) INSERT OR REPLACE: %d\n', ch );
  show_modified();

  % 2) 1x Insert, 45 noch nicht vorhanden
  mksqlite('INSERT OR REPLACE INTO tbl VALUES (45, 452, "init:3")')
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( '2) INSERT OR REPLACE: %d\n', ch );
  show_modified();

  % 3) Mix: 2x Replace und 2x Insert
  mksqlite('INSERT OR REPLACE INTO tbl (k,v) VALUES (42, 423), (43, 433), (46, 463), (47,473)')
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( '3) INSERT OR REPLACE: %d\n', ch );
  show_modified();

  % 4) 3x Update 
  mksqlite('UPDATE tbl SET v=k*10+4 WHERE k in (43, 45, 46)');
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( '4) UPDATE (no change): %d\n', ch );
  show_modified();

  % 5) Update (alle)
  mksqlite('UPDATE tbl SET v=k*10+5, t="init:5"');
  ch = mksqlite('SELECT changes() AS changes');
  fprintf( '5) UPDATE (no change): %d\n', ch );
  show_modified();

  mksqlite('close');

end

function show_modified
  mksqlite('SELECT * FROM tbl')
  x = mksqlite('SELECT * FROM chng');
  assert( iscell(x) );
  inserts = [x{:,1}];
  updates = [x{:,2}];
  if ~isempty( inserts )
    inserts = join( sprintfc( '%d', inserts ), ',');
    fprintf( 'Inserts: %s\n', inserts{1} );
  end
  if ~isempty( updates )
    updates = join( sprintfc( '%d', updates ), ',');
    fprintf( 'Updates: %s\n', updates{1} );
  end
  mksqlite('DELETE FROM chng');
  fprintf( '==============================================\n' );
end