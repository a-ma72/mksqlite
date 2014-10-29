/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */
 
 /* Global configuration settings and defaults */

#pragma once

#define BOOL_TRUE  1
#define BOOL_FALSE 0

/* result type names defined in locale.hpp */
enum {
    RESULT_TYPE_ARRAYOFSTRUCTS, // Array of structs
    RESULT_TYPE_STRUCTOFARRAYS, // Struct of arrays
    RESULT_TYPE_MATRIX,         // Matrix/cell array
    
    // Limit for bound checking only
    RESULT_TYPE_MAX_ID = RESULT_TYPE_MATRIX
};

#define CONFIG_MAX_NUM_OF_DBS         5             /* maximum number of databases */
#define CONFIG_NULL_AS_NAN            BOOL_FALSE    /* use NAN instead of NULL values */
#define CONFIG_BUSYTIMEOUT            1000          /* default SQL busy timeout in seconds */
#define CONFIG_CHECK_4_UNIQUE_FIELDS  BOOL_TRUE     /* ensure unique fields in query return structure */

/* compression level: Using compression on typed blobs when > 0 */
#define CONFIG_COMPRESSION_LEVEL      0             /* no compression by default */
#define CONFIG_COMPRESSION_TYPE       NULL          /* "blosc", "blosclz", "qlin16", "qlog16" or NULL (for default)

/* Flag: check compressed against original data */
#define CONFIG_COMPRESSION_CHECK      BOOL_TRUE     /* check is on by default */

/* Convert UTF-8 to ascii, otherwise set slCharacterEncoding('UTF-8') */
#define CONFIG_CONVERT_UTF8           BOOL_TRUE     /* use UTF8 encoding by default */

/* Allow streaming to convert MATLAB variables into byte streams */
#define CONFIG_STREAMING              BOOL_FALSE    /* streaming disabled by default */

// SQLite itself limits BLOBs to 1MB, mksqlite limits to INT32_MAX
#define CONFIG_MKSQLITE_MAX_BLOB_SIZE ((mwSize)INT32_MAX)

/* Early bind mxSerialize and mxDeserialize */
/* BOOL_TRUE: mksqlite has to be linked with MATLAB lib, BOOL_FALSE: dynamic calls to MATLAB functions */
#ifndef CONFIG_EARLY_BIND_SERIALIZE
#define CONFIG_EARLY_BIND_SERIALIZE   BOOL_FALSE
#endif

/* Data organisation of query results */
#define CONFIG_RESULT_TYPE            RESULT_TYPE_ARRAYOFSTRUCTS

/* Wrap parameters */
#define CONFIG_WRAP_PARAMETERS        BOOL_FALSE
