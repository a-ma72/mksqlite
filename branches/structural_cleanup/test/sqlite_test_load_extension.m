function sqlite_test_load_extension ()
  % Create in-memory database
  dbid = mksqlite( 'open', '' ); 
  
  % SpatiaLite V3.0.1
  % (ref: http://www.gaia-gis.it/gaia-sins/windows-bin-x86-prev/spatialite-3.0.1-DLL-win-x86.zip)
  
  % Enable SQLite extensions
  mksqlite( dbid, 'enable extension', 1 );
  
  % Bind SpatiaLite
  q = mksqlite( dbid, 'select load_extension("spatialite.dll")' );
  
  if ~isstruct(q)
    error( 'Error while binding SpatiaLite DLL' );
  end
  
  fprintf( 'SpatiaLite version:' );
  mksqlite( dbid, 'select spatialite_version() as Version' )
  
  mksqlite( 0, 'close' ); % Close all databases
end
