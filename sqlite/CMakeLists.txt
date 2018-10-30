project( sqlite )

#add_compiler_option( -fPIC )
add_library( sqlite STATIC sqlite3.c shell.c )
