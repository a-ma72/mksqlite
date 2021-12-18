project( sqlite )

if( SQLITE_ENABLE_MATH_FUNCTIONS )
	add_definitions( -DSQLITE_ENABLE_MATH_FUNCTIONS )
endif()

#add_compiler_option( -fPIC )
if( MKSQLITE_CONFIG_USE_UUID )
	message( STATUS "Using UUID extension" )
	add_library( sqlite STATIC sqlite3.c shell.c uuid.c )
else()
	add_library( sqlite STATIC sqlite3.c shell.c )
endif()
