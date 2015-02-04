function sqlite_test_load_extension
    
    if length(dbstack) ~= 1
        return
    end
    
    % Create in-memory database
    dbid = mksqlite( 'open', '' ); 

    % SpatiaLite V3.0.1
    % (ref: http://www.gaia-gis.it/gaia-sins/windows-bin-x86-prev/spatialite-3.0.1-DLL-win-x86.zip)

    % Enable SQLite extensions
    mksqlite( dbid, 'enable extension', 1 );

    % Bind SpatiaLite
    q = mksqlite( dbid, 'SELECT load_extension("libspatialite-2.dll")' );

    if ~isstruct(q)
      error( 'Error while binding SpatiaLite DLL' );
    end

    fprintf( 'SpatiaLite version:' );
    mksqlite( dbid, 'SELECT spatialite_version() AS Version' )

    mksqlite( 'close' ); % Close database

