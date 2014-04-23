function sqlite_test_load_extension ()
  % Dummy Datenbank im Speicher erzeugen
  dbid = mksqlite( 'open', '' ); 
  
  % SpatiaLite V3.0.1
  % (ref: http://www.gaia-gis.it/gaia-sins/windows-bin-x86-prev/spatialite-3.0.1-DLL-win-x86.zip)
  
  % SQLite Extensions zulassen
  mksqlite( dbid, 'enable extension', 1 );
  
  % SpatiaLite einbinden
  q = mksqlite( dbid, 'select load_extension("spatialite.dll")' );
  
  if ~isstruct(q)
    error( 'Fehler beim Einbinden der DLL SpatiaLite' );
  end
  
  fprintf( 'SpatiaLite Version:' );
  mksqlite( dbid, 'select spatialite_version() as Version' )
  
  mksqlite( 0, 'close' ); % Datenbank schlieﬂen
end