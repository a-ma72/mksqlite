/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */
 
 /* Global configuration settings and defaults */

#define CONFIG_MAX_NUM_OF_DBS         5       /* maximum number of databases */
#define CONFIG_NULL_AS_NAN            false   /* use NAN instead of NULL values */
#define CONFIG_BUSYTIMEOUT            1000    /* default SQL busy timeout in seconds */
#define CONFIG_CHECK_4_UNIQUE_FIELDS  true    /* ensure unique fields in query return structure */

/* compression level: Using compression on typed blobs when > 0 */
#define CONFIG_COMPRESSION_LEVEL      0       /* no compression by default */
#define CONFIG_COMPRESSION_TYPE       NULL    /* "blosc", "blosclz", "qlin16", "qlog16" or NULL (for default)

/* Flag: check compressed against original data */
#define CONFIG_COMPRESSION_CHECK      true    /* 

/* Convert UTF-8 to ascii, otherwise set slCharacterEncoding('UTF-8') */
#define CONFIG_CONVERT_UTF8           true    /* use UTF8 encoding */

// SQLite itself limits BLOBs to 1MB, mksqlite limits to INT32_MAX
#define CONFIG_MKSQLITE_MAX_BLOB_SIZE ((mwSize)INT32_MAX)

/* Early bind mxSerialize and mxDeserialize */
/* 0: mksqlite tries to use , 1: mksqlite has to be linked with MATLAB lib */
#define CONFIG_EARLY_BIND_SERIALIZE   0       
