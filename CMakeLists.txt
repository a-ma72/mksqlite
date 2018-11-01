# First set Matlab_ROOT_DIR environment variable to your installed matlab path,
# such as 'export Matlab_ROOT_DIR=/usr/local/MATLAB/R2017b'!

# Building makefiles:
# mkdir build
# cd build
# cmake -G "Visual Studio 14 2015 Win64" ..

# Superseed options from other modules
macro(SET_OPTION option value)
  set(${option} ${value} CACHE INTERNAL "" FORCE)
endmacro()

cmake_minimum_required( VERSION 3.2 )
set( CMAKE_CONFIGURATION_TYPES Release Debug )
project( mksqlite )

option( MKSQLITE_CONFIG_USE_HEAP_CHECK "Internal heap checking" OFF )
option( MKSQLITE_CONFIG_NULL_AS_NAN "Use NaN instead of NULL values by default" OFF )
option( MKSQLITE_CONFIG_COMPRESSION_CHECK "Always check compressed data (slower)" OFF )
option( MKSQLITE_CONFIG_CONVERT_UTF8 "Text interchange with MATLAB in UTF8 char format" ON )
set( MKSQLITE_CONFIG_MAX_NUM_OF_DBS 10 CACHE STRING "Maximum number of databases opened at once" )
set( MKSQLITE_CONFIG_BUSYTIMEOUT 1000 CACHE STRING "Default SQL busy timeout in milliseconds (1000)" )

if( EXISTS ${CMAKE_SOURCE_DIR}/c-blosc )
    option( MKSQLITE_CONFIG_USE_BLOSC "Build BLOSC" ON )
    SET_OPTION( BUILD_BENCHMARKS OFF )
    SET_OPTION( BUILD_SHARED     OFF )
    SET_OPTION( BUILD_STATIC     ON  )
    SET_OPTION( BUILD_TESTS      OFF )
else()
    set( MKSQLITE_CONFIG_USE_BLOSC OFF CACHE INTERNAL "" FORCE )
endif()

set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set( CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/build )

set( MATLAB_FIND_DEBUG 1 )
find_package( Matlab REQUIRED COMPONENTS MX_LIBRARY )

if( MATLAB_FOUND )
    message( STATUS "MATLAB Found, MATLAB MEX will be compiled." )
    # message( STATUS ${Matlab_LIBRARIES} )
else()
    message( FATAL_ERROR "MATLAB not found...nothing will be built." )
endif()

add_definitions( -DMATLAB_MEX_FILE )

# set up matlab libraries
include_directories( ${Matlab_INCLUDE_DIRS} c-blosc )

# Parse mksqlite Version
if( WIN32 )
    set( CMD_PREFIX CMD /c )
endif()
execute_process( COMMAND ${CMD_PREFIX} git describe --tags
                 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                 OUTPUT_VARIABLE MKSQLITE_VERSION_STRING
                 ERROR_QUIET
                 OUTPUT_STRIP_TRAILING_WHITESPACE )
if( NOT ${MKSQLITE_VERSION_STRING} STREQUAL "" )
    string( REGEX REPLACE "([0-9]+)\.([0-9]+).*" "\\1" MKSQLITE_VERSION_MAJOR  ${MKSQLITE_VERSION_STRING} )
    string( REGEX REPLACE "([0-9]+)\.([0-9]+).*" "\\2" MKSQLITE_VERSION_MINOR  ${MKSQLITE_VERSION_STRING} )
    configure_file( ${CMAKE_SOURCE_DIR}/git_revision.h.in ${CMAKE_SOURCE_DIR}/git_revision.h )
else()
    message( "Didn't got revision information, using previous information!" )
endif()
configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

set( src md5 sqlite )

add_subdirectory( sqlite )
add_subdirectory( md5 )

if( MKSQLITE_CONFIG_USE_BLOSC )
    add_subdirectory( c-blosc )
    include_directories( c-blosc )
    set( src ${src} blosc_static )
endif()

matlab_add_mex( NAME mksqlite SRC mksqlite.cpp )
target_link_libraries( mksqlite ${Matlab_LIBRARIES} ${src} )
get_filename_component( Matlab_MX_LIBRARY_PATH ${Matlab_MX_LIBRARY} DIRECTORY )
target_link_libraries( mksqlite ${Matlab_MX_LIBRARY_PATH}/libut${CMAKE_LINK_LIBRARY_SUFFIX} )

# install to /bin and /share by default
if( 1 )
    install( TARGETS mksqlite RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )
    install( FILES LICENSE README.MD DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/mksqlite )
    if( MKSQLITE_CONFIG_USE_BLOSC )
        install( DIRECTORY c-blosc/LICENSES DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/mksqlite/blosc )
    endif()
endif()

# CPack

# include( InstallRequiredSystemLibraries )

set( CPACK_GENERATOR TGZ ZIP )
set( CPACK_SOURCE_GENERATOR TGZ ZIP )
set( CPACK_PACKAGE_VERSION_MAJOR ${MKSQLITE_VERSION_MAJOR} )
set( CPACK_PACKAGE_VERSION_MINOR ${MKSQLITE_VERSION_MINOR} )
set( CPACK_PACKAGE_VERSION_PATCH 0 )
set( CPACK_PACKAGE_VERSION ${MKSQLITE_VERSION_STRING} )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.MD" )
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "A MATLAB(R) Mex-DLL to SQLite Databases" )
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" )
#set( CPACK_SOURCE_IGNORE_FILES "/build.*;/c-blosc.*;.*~;\\\\.git.*;\\\\.DS_Store" )
#set( CPACK_STRIP_FILES 0 )
#set( CPACK_SOURCE_STRIP_FILES 0 )

include( CPack )
