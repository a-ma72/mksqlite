# First set Matlab_ROOT_DIR environment variable to your installed matlab path,
# such as 'export Matlab_ROOT_DIR=/usr/local/MATLAB/R2017b'!

# Building makefiles:
# mkdir build
# cd build
# cmake -G "Visual Studio 14 2015 Win64" ..

set( CMAKE_POSITION_INDEPENDENT_CODE ON )


cmake_minimum_required( VERSION 3.2 )
set( CMAKE_CONFIGURATION_TYPES Release Debug )
project( mksqlite )

set( CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/build )

set( MATLAB_FIND_DEBUG 1 )
find_package( Matlab REQUIRED COMPONENTS MX_LIBRARY )

IF( MATLAB_FOUND )
    message( STATUS "MATLAB Found, MATLAB MEX will be compiled." )
    message( STATUS ${Matlab_LIBRARIES} )
ELSE( MATLAB_FOUND )
    MESSAGE( "MATLAB not found...nothing will be built." )
ENDIF( MATLAB_FOUND )

add_definitions( -DMATLAB_MEX_FILE )

# set up matlab libraries
INCLUDE_DIRECTORIES( ${Matlab_INCLUDE_DIRS} c-blosc )

matlab_add_mex( NAME mksqlite SRC mksqlite.cpp )
target_link_libraries( mksqlite ${Matlab_LIBRARIES} md5 sqlite blosc_static )

add_subdirectory( c-blosc )
add_subdirectory( sqlite )
add_subdirectory( md5 )

# install to /mex by default
add_custom_command( TARGET mksqlite POST_BUILD 
                    COMMAND ${CMAKE_COMMAND} -E make_directory               ${CMAKE_INSTALL_PREFIX}/mex/$<CONFIG>/
                    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mksqlite> ${CMAKE_INSTALL_PREFIX}/mex/$<CONFIG>/ )
