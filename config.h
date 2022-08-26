/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      config.h
 *  @brief     Global configuration settings and defaults
 *  @details   Configuration file for settings and defaults
 *  @authors   Martin Kortmann <mail@kortmann.de>,
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.13
 *  @date      2008-2022
 *  @copyright Distributed under BSD-2
 *  @pre       
 *  @warning   
 *  @bug       
 */
 
#pragma once

#include "git_revision.h"

/// Common configurations
#define BOOL_TRUE  1
#define BOOL_FALSE 0
#define ON         1 
#define OFF        0

#define MKSQLITE_CONFIG_USE_UUID               ON          ///< true=use uuid extension
#define MKSQLITE_CONFIG_USE_HEAP_CHECK         OFF         ///< false=standard allocators, true=usage of heap_check.hpp
#define MKSQLITE_CONFIG_USE_LOGGING            OFF         ///< default SQL busy timeout in milliseconds (1000)
#define MKSQLITE_CONFIG_NULL_AS_NAN            OFF         ///< use NaN instead of NULL values by default
#define MKSQLITE_CONFIG_BUSYTIMEOUT            1000        ///< default SQL busy timeout in milliseconds (1000)

/// compression level: Using compression on typed blobs when > 0
#define MKSQLITE_CONFIG_COMPRESSION_LEVEL      0           ///< no compression by default
#define MKSQLITE_CONFIG_COMPRESSION_TYPE       "blosclz"   ///< "blosclz", "lz4", "lz4hc", "snappy", "zlib", "zstd", "qlin16", "qlog16"

/// Flag: check compressed against original data
#define MKSQLITE_CONFIG_COMPRESSION_CHECK      OFF         ///< check is on by default

/// Convert UTF-8 to ascii, otherwise set slCharacterEncoding('UTF-8')
#define MKSQLITE_CONFIG_CONVERT_UTF8           ON          ///< use UTF8 encoding by default


/// MATLAB specific configurations
#if defined( MATLAB_MEX_FILE )

    /**
     * \brief result types
     * \sa STR_RESULT_TYPES in locale.hpp
     */
    enum RESULT_TYPES {
        RESULT_TYPE_ARRAYOFSTRUCTS, ///< 0-Array of structs
        RESULT_TYPE_STRUCTOFARRAYS, ///< 1-Struct of arrays
        RESULT_TYPE_MATRIX,         ///< 2-Matrix/cell array
    
        /// Limit for bound checking only
        RESULT_TYPE_MAX_ID = RESULT_TYPE_MATRIX
    };

    #define MKSQLITE_CONFIG_VERSION_STRING           MKSQLITE_VERSION_MAJOR "." MKSQLITE_VERSION_MINOR    /**< mksqlite version string */
    
    #define MKSQLITE_CONFIG_MAX_NUM_OF_DBS           100                          ///< maximum number of databases, simultaneous open
    #define MKSQLITE_CONFIG_CHECK_4_UNIQUE_FIELDS    ON                          ///< ensure unique fields in query return structure by default

    /// Allow streaming to convert MATLAB variables into byte streams
    #define MKSQLITE_CONFIG_STREAMING                OFF                         ///< streaming is disabled by default

    /// SQLite itself limits BLOBs to 1MB, mksqlite limits to INT32_MAX
    #define MKSQLITE_CONFIG_MAX_BLOB_SIZE            ((mwSize)INT32_MAX)         ///< max. size in bytes of a blob

    /// Early bind mxSerialize and mxDeserialize
    /// ON: mksqlite has to be linked with MATLAB lib, OFF: dynamic calls to MATLAB functions
    #ifndef MKSQLITE_CONFIG_EARLY_BIND_SERIALIZE
    #define MKSQLITE_CONFIG_EARLY_BIND_SERIALIZE     OFF                         ///< early binding if off by default
    #endif

    /// Data organisation of query results
    #define MKSQLITE_CONFIG_RESULT_TYPE              RESULT_TYPE_ARRAYOFSTRUCTS  ///< return array of structs by default

    /// Wrap parameters
    #define MKSQLITE_CONFIG_PARAM_WRAPPING           OFF                         ///< paramter wrapping is off by default

    /// Use blosc library
    #define MKSQLITE_CONFIG_USE_BLOSC                ON                          ///< 
#endif
