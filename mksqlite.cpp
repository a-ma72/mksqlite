/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

/* not really used yet, since this is only one module */
#define MAIN_MODULE

#include "config.h"                 // Defaults
#include "global.hpp"               // Global definitions and stati
#include "serialize.hpp"            // serialization of MATLAB variables (undocumented feature)
#include "compressor.hpp"           // some compressing algorithms
#include "typed_blobs.hpp"          // packing into typed blobs with variable type storage
#include "utils.hpp"                // some utilities
#include "sqlite/sqlite3.h"         // SQLite amalgamation
#include "sql_user_functions.hpp"   // SQL user functions
#include "locale.hpp"               // (Error-)Messages


// static assertion (compile time), ensures int32_t and mwSize as 4 byte data representation
static char SA_UIN32[ (sizeof(uint32_t)==4 && sizeof(mwSize)==4) ? 1 : -1 ]; 

/* Static assertion: Ensure backward compatibility */
static char SA_TBH_BASE[ sizeof( TypedBLOBHeaderV1 ) == 36 ? 1 : -1 ];

/* declare the MEX Entry function as pure C */
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );

// For SETERR usage:
const char* SQL_ERR = "SQL_ERR";                // if attached to g_finalize_msg, function returns with least SQL error message
const char* SQL_ERR_CLOSE = "SQL_ERR_CLOSE";    // same as SQL_ERR, additionally the responsible db will be closed

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* forward declarations */


/*
 * Table of used database ids.
 */
static sqlite3* g_dbs[CONFIG_MAX_NUM_OF_DBS] = { 0 };

/*
 * close left over databases.
 */
static void close_DBs( void )
{
    /*
     * Is there any database left?
     */
    bool dbsClosed = false;
    bool failed = false;
    for( int i = 0; i < CONFIG_MAX_NUM_OF_DBS; i++ )
    {
        /*
         * close it
         */
        if( g_dbs[i] )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[i] ) )
            {
                g_dbs[i] = 0;
                dbsClosed = true;
            } 
            else 
            {
                failed = true;
            }
        }
    }
    
    if( dbsClosed )
    {
        /*
         * Set the language to english if something
         * goes wrong before the language could been set
         */
        if( Language < 0 )
        {
            Language = 0;
        }
        /*
         * and inform the user
         */
        mexWarnMsgTxt( getMsg( MSG_CLOSINGFILES ) );
    }
    
    sqlite3_shutdown();
    blosc_destroy();
    
    if( failed )
    {
        mexErrMsgTxt( getMsg( MSG_ERRCLOSEDBS ) );
    }
}


/*
 * Get the last SQLite Error Code as an Error Identifier
 */
static const char* trans_err_to_ident( sqlite3 *db )
{
    static char dummy[32];

    int errorcode = sqlite3_errcode( db );
    
    switch( errorcode )
     {    
        case SQLITE_OK:         return( "SQLITE:OK" );
        case SQLITE_ERROR:      return( "SQLITE:ERROR" );
        case SQLITE_INTERNAL:   return( "SQLITE:INTERNAL" );
        case SQLITE_PERM:       return( "SQLITE:PERM" );
        case SQLITE_ABORT:      return( "SQLITE:ABORT" );
        case SQLITE_BUSY:       return( "SQLITE:BUSY" );
        case SQLITE_LOCKED:     return( "SQLITE:LOCKED" );
        case SQLITE_NOMEM:      return( "SQLITE:NOMEM" );
        case SQLITE_READONLY:   return( "SQLITE:READONLY" );
        case SQLITE_INTERRUPT:  return( "SQLITE:INTERRUPT" );
        case SQLITE_IOERR:      return( "SQLITE:IOERR" );
        case SQLITE_CORRUPT:    return( "SQLITE:CORRUPT" );
        case SQLITE_NOTFOUND:   return( "SQLITE:NOTFOUND" );
        case SQLITE_FULL:       return( "SQLITE:FULL" );
        case SQLITE_CANTOPEN:   return( "SQLITE:CANTOPEN" );
        case SQLITE_PROTOCOL:   return( "SQLITE:PROTOCOL" );
        case SQLITE_EMPTY:      return( "SQLITE:EMPTY" );
        case SQLITE_SCHEMA:     return( "SQLITE:SCHEMA" );
        case SQLITE_TOOBIG:     return( "SQLITE:TOOBIG" );
        case SQLITE_CONSTRAINT: return( "SQLITE:CONSTRAINT" );
        case SQLITE_MISMATCH:   return( "SQLITE:MISMATCH" );
        case SQLITE_MISUSE:     return( "SQLITE:MISUSE" );
        case SQLITE_NOLFS:      return( "SQLITE:NOLFS" );
        case SQLITE_AUTH:       return( "SQLITE:AUTH" );
        case SQLITE_FORMAT:     return( "SQLITE:FORMAT" );
        case SQLITE_RANGE:      return( "SQLITE:RANGE" );
        case SQLITE_NOTADB:     return( "SQLITE:NOTADB" );
        case SQLITE_ROW:        return( "SQLITE:ROW" );
        case SQLITE_DONE:       return( "SQLITE:DONE" );

        default:
            _snprintf( dummy, sizeof( dummy ), "SQLITE:%d", errorcode );
            return dummy;
     }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* the main routine */

/*
 * This ist the Entry Function of this Mex-DLL
 */
void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] )
{
    mxArray* pArgs       = NULL;  // Cell array of parameters behind command, in case of an SQL command only
    sqlite3_stmt *st     = NULL;  // SQL statement (sqlite bridge)
    char *command        = NULL;  // the SQL command (superseeded by query)
    g_finalize_msg       = NULL;  // pointer to actual error message
    int FirstArg         = 0;
    
    /*
     * Get the current Language
     */
    if( getLanguage() == -1 )
    {
#ifdef _WIN32        
        switch( PRIMARYLANGID( GetUserDefaultLangID() ) )
        {
            case LANG_GERMAN:
                setLanguage(1);
                break;
                
            default:
                setLanguage(0);
        }
#else
        setLanguage(0);
#endif
    }
    
    /*
     * Print Version Information, initializing, ...
     */
    if( !g_is_initialized )
    {
        mxArray *plhs[3] = {0};
        
        if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
        {
            //assert( old_version::check_compatibility() );

            mexPrintf( getMsg( MSG_HELLO ), sqlite3_libversion() );
            
            mxGetString( plhs[0], TBH_platform, TBH_PLATFORM_MAXLEN );
            mxGetString( plhs[2], TBH_endian, 2 );

            mexPrintf( "Platform: %s, %s\n\n", TBH_platform, TBH_endian[0] == 'L' ? "little endian" : "big endian" );
            
            destroy_array( plhs[0] );
            destroy_array( plhs[1] );
            destroy_array( plhs[2] );
            
            mexAtExit( close_DBs );
            sqlite3_initialize();
            blosc_init();
            g_compression_type = BLOSC_DEFAULT_ID;
            g_is_initialized = true;
        }
        else
        {
            mexErrMsgTxt( getMsg( MSG_ERRPLATFORMDETECT ) );
        }
    }
    
    int db_id = 0;
    int CommandPos = 0;
    int NumArgs = nrhs;
    
    /*
     * Check if the first argument is a number, then we have to use
     * this number as an database id.
     */
    if( nrhs >= 1 && mxIsNumeric( prhs[0] ) )
    {
        db_id = get_integer( prhs[0] );
        if( db_id < 0 || db_id > CONFIG_MAX_NUM_OF_DBS )
        {
            FINALIZE( getMsg( MSG_INVALIDDBHANDLE ) );
        }
        db_id--;
        CommandPos++;
        NumArgs--;
    }

    /*
     * no argument -> fail
     */
    if( NumArgs < 1 )
    {
        mexPrintf( "%s", getMsg( MSG_USAGE ) );
        FINALIZE( getMsg( MSG_INVALIDARG ) );
    }
    
    /*
     * The next (or first if no db number available) is the command,
     * it has to be a string.
     * This fails also, if the first arg is a db-id and there is no 
     * further argument
     */
    if( !mxIsChar( prhs[CommandPos] ) )
    {
        mexPrintf( "%s", getMsg( MSG_USAGE ) );
        FINALIZE( getMsg( MSG_INVALIDARG ) );
    }
    
    /*
     * Get the command string
     */
    command = get_string( prhs[CommandPos] );
    
    /*
     * Adjust the Argument pointer and counter
     */
    FirstArg = CommandPos + 1;
    NumArgs--;
    
    if( !strcmp( command, "version mex" ) )
    {
        if( nlhs == 0 )
        {
            mexPrintf( "mksqlite Version %s\n", MKSQLITE_VERSION_STRING );
        } 
        else
        {
            plhs[0] = mxCreateString( MKSQLITE_VERSION_STRING );
        }
    } 
    else if( !strcmp( command, "version sql" ) )
    {
        if( nlhs == 0 )
        {
            mexPrintf( "SQLite Version %s\n", SQLITE_VERSION_STRING );
        } 
        else 
        {
            plhs[0] = mxCreateString( SQLITE_VERSION_STRING );
        }
    } 
    else if( !strcmp( command, "open" ) )
    {
        int flags = 0;
        /*
         * open a database. There has to be one string argument,
         * the database filename
         */
        if( NumArgs < 1 || !mxIsChar( prhs[FirstArg] ) )
        {
            FINALIZE( getMsg( MSG_NOOPENARG ) );
        }
        
        // No Memoryleak 'command not freed' when get_string fails
        // Matlab Help:
        // "If your application called mxCalloc or one of the 
        // mxCreate* routines to allocate memory, mexErrMsgTxt 
        // automatically frees the allocated memory."
        char* dbname = get_string( prhs[FirstArg] );

        /*
         * Is there an database ID? The close the database with the same id 
         */
        if( db_id > 0 && g_dbs[db_id] )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", getMsg( MSG_ERRCANTCLOSE ) );
                FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
            }
        }

        /*
         * If there isn't an database id, then try to get one
         */
        if( db_id < 0 )
        {
            for( int i = 0; i < CONFIG_MAX_NUM_OF_DBS; i++ )
            {
                if( g_dbs[i] == 0 )
                {
                    db_id = i;
                    break;
                }
            }
        }
        /*
         * no database id? sorry, database id table full
         */
        if( db_id < 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            
            free_ptr( dbname );  // Needless due to mexErrMsgTxt(), but clean
            FINALIZE( getMsg( MSG_NOFREESLOT ) );
        }
        
        
        /*
         * Open mode (optional)
         */
        if( NumArgs >= 2 )
        {
            char* iomode = get_string( prhs[FirstArg+1] );
            
            if( 0 == _strcmpi( iomode, "ro" ) )
            {
                flags |= SQLITE_OPEN_READONLY;
            } 
            else if( 0 == _strcmpi( iomode, "rw" ) )
            {
                flags |= SQLITE_OPEN_READWRITE;
            } 
            else if( 0 == _strcmpi( iomode, "rwc" ) )
            {
                flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            } 
            else 
            {
                free_ptr( iomode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( getMsg( MSG_ERRUNKOPENMODE ) );
            }
            free_ptr( iomode ); 
        } 
        else 
        {
            flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        }
        
        
        /*
         * Threading mode (optional)
         */
        if( NumArgs >= 3 )
        {
            char* threadmode = get_string( prhs[FirstArg+2] );
            
            if( 0 == _strcmpi( threadmode, "single" ) )
            {
                /* default */
            } 
            else if( 0 == _strcmpi( threadmode, "multi" ) )
            {
                flags |= SQLITE_OPEN_NOMUTEX;
            } 
            else if( 0 == _strcmpi( threadmode, "serial" ) )
            {
                flags |= SQLITE_OPEN_FULLMUTEX;
            } 
            else 
            {
                free_ptr( threadmode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( getMsg( MSG_ERRUNKTHREADMODE ) );
            }
            free_ptr( threadmode );
        } 
        
       
        /*
         * Open the database
         * (g_dbs[db_id] is assigned by sqlite3_open_v2(), even if an error
         * occures
         */
        int rc = sqlite3_open_v2( dbname, &g_dbs[db_id], flags, NULL );
        
        if( SQLITE_OK != rc )
        {
            /*
             * Database could not opened
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            
            free_ptr( dbname );   // Needless due to mexErrMsgTxt(), but clean
            mexPrintf( "%s\n", getMsg( MSG_CANTOPEN ) );
            FINALIZE( SQL_ERR_CLOSE );
        }
        
        /*
         * Set Default Busytimeout
         */
        rc = sqlite3_busy_timeout( g_dbs[db_id], CONFIG_BUSYTIMEOUT );
        if( SQLITE_OK != rc )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            free_ptr( dbname );   // Needless due to mexErrMsgTxt(), but clean
            mexPrintf( "%s\n", getMsg( MSG_BUSYTIMEOUTFAIL ) );
            FINALIZE( SQL_ERR_CLOSE );
        }
        
        // attach new SQL commands to opened database
        sqlite3_create_function( g_dbs[db_id], "pow", 2, SQLITE_UTF8, NULL, pow_func, NULL, NULL );     // power function (math)
        sqlite3_create_function( g_dbs[db_id], "regex", 2, SQLITE_UTF8, NULL, regex_func, NULL, NULL ); // regular expressions (MATCH mode)
        sqlite3_create_function( g_dbs[db_id], "regex", 3, SQLITE_UTF8, NULL, regex_func, NULL, NULL ); // regular expressions (REPLACE mode)
        sqlite3_create_function( g_dbs[db_id], "bdcratio", 1, SQLITE_UTF8, NULL, BDC_ratio_func, NULL, NULL );     // compression ratio (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcpacktime", 1, SQLITE_UTF8, NULL, BDC_pack_time_func, NULL, NULL );     // compression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcunpacktime", 1, SQLITE_UTF8, NULL, BDC_unpack_time_func, NULL, NULL );     // decompression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "md5", 1, SQLITE_UTF8, NULL, MD5_func, NULL, NULL );     // Message-Digest (RSA)
        
        /*
         * return value will be the used database id
         */
        plhs[0] = mxCreateDoubleScalar( (double)db_id + 1 );
        free_ptr( dbname );
    }
    else if( !strcmp( command, "close" ) )
    {
        /*
         * close a database
         */

        /*
         * There should be no Argument to close
         */
        FINALIZE_IF( NumArgs > 0, getMsg( MSG_INVALIDARG ) );
        
        /*
         * if the database id is < 0 than close all open databases
         */
        if( db_id < 0 )
        {
            for( int i = 0; i < CONFIG_MAX_NUM_OF_DBS; i++ )
            {
                if( g_dbs[i] )
                {
                    if( SQLITE_OK == sqlite3_close( g_dbs[i] ) )
                    {
                        g_dbs[i] = 0;
                    } 
                    else 
                    {
                        mexPrintf( "%s\n", getMsg( MSG_ERRCANTCLOSE ) );
                        FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
                    }
                }
            }
        }
        else
        {
            /*
             * If the database is open, then close it. Otherwise
             * inform the user
             */
            FINALIZE_IF( !g_dbs[db_id], getMsg( MSG_DBNOTOPEN ) );

            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", getMsg( MSG_ERRCANTCLOSE ) );
                FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
            }
        }
    }
    else if( !strcmp( command, "enable extension" ) )
    {
        /*
         * There should be one Argument to enable extension
         */
        FINALIZE_IF( NumArgs < 1, getMsg( MSG_MISSINGARG ) );
        FINALIZE_IF( NumArgs > 1, getMsg( MSG_UNEXPECTEDARG ) );
        FINALIZE_IF( !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
        FINALIZE_IF( !g_dbs[db_id], getMsg( MSG_DBNOTOPEN ) );
        
        int flagOnOff = get_integer( prhs[FirstArg] );
        
        if( SQLITE_OK == sqlite3_enable_load_extension( g_dbs[db_id], flagOnOff ) )
        {
            if( flagOnOff )
            {
                mexPrintf( "%s\n", getMsg( MSG_EXTENSION_EN ) );
            } 
            else 
            {
                mexPrintf( "%s\n", getMsg( MSG_EXTENSION_DIS ) );
            }
        } 
        else 
        {
            mexPrintf( "%s\n", getMsg( MSG_EXTENSION_FAIL ) );
        }
    }
    else if( !strcmp( command, "status" ) )
    {
        /*
         * There should be no Argument to status
         */
        FINALIZE_IF( NumArgs > 0, getMsg( MSG_INVALIDARG ) );
        
        for( int i = 0; i < CONFIG_MAX_NUM_OF_DBS; i++ )
        {
            mexPrintf( "DB Handle %d: %s\n", i, g_dbs[i] ? "OPEN" : "CLOSED" );
        }
    }
    else if( !_strcmpi( command, "setbusytimeout" ) )
    {
        /*
         * There should be one Argument, the Timeout in ms
         */
        FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
        FINALIZE_IF( !g_dbs[db_id], getMsg( MSG_DBNOTOPEN ) );

        /*
         * Set Busytimeout
         */
        int TimeoutValue = get_integer( prhs[FirstArg] );

        int rc = sqlite3_busy_timeout( g_dbs[db_id], TimeoutValue );
        if( SQLITE_OK != rc )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            mexPrintf( "%s\n", getMsg( MSG_BUSYTIMEOUTFAIL ) );
            FINALIZE( SQL_ERR_CLOSE );
        }
    }
    else if( !_strcmpi( command, "check4uniquefields" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_check4uniquefields );
        }
        else 
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
            g_check4uniquefields = ( get_integer( prhs[FirstArg] ) ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "convertUTF8" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_convertUTF8 );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
            g_convertUTF8 = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "typedBLOBs" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)typed_blobs_mode_get() );
        }
        else
        {
            int iValue;
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
            iValue = get_integer( prhs[FirstArg] );
            FINALIZE_IF( iValue < /*0*/ TYBLOB_NO || iValue > /*2*/ TYBLOB_BYSTREAM, getMsg( MSG_INVALIDARG ) );
            if( TYBLOB_BYSTREAM == iValue && !can_serialize() )
            {
                FINALIZE( getMsg( MSG_ERRNOTSUPPORTED ) );
            }
            typed_blobs_mode_set( (typed_blobs_e)iValue );
        }
    }
    else if( !_strcmpi( command, "NULLasNaN" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_NULLasNaN );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
            g_NULLasNaN = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "compression" ) )
    {
        if( NumArgs == 0 )
        {
            mxArray* cell = mxCreateCellMatrix( 2, 1 );
            mxArray* compressor = mxCreateString( g_compression_type ? g_compression_type : "" );
            mxArray* level = mxCreateDoubleScalar( (double)g_compression_level );
            mxSetCell( cell, 0, compressor );
            mxSetCell( cell, 1, level );
            plhs[0] = cell;
        }
        else
        {
            int new_compression_level = 0;
            const char* new_compressor = NULL;
            
            FINALIZE_IF(   NumArgs != 2 
                        || !mxIsChar( prhs[FirstArg] )
                        || !mxIsNumeric( prhs[FirstArg+1] ), getMsg( MSG_INVALIDARG ) );
            
            new_compressor = get_string( prhs[FirstArg] );
            new_compression_level = ( get_integer( prhs[FirstArg+1] ) );
            
            FINALIZE_IF( new_compression_level < 0 || new_compression_level > 9, getMsg( MSG_INVALIDARG ) );

            if( 0 == _strcmpi( new_compressor, BLOSC_LZ4_ID ) )
            {
                g_compression_type = BLOSC_LZ4_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, BLOSC_LZ4HC_ID ) )
            {
                g_compression_type = BLOSC_LZ4HC_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, BLOSC_DEFAULT_ID ) )
            {
                g_compression_type = BLOSC_DEFAULT_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, QLIN16_ID ) )
            {
                g_compression_type = QLIN16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else if( 0 == _strcmpi( new_compressor, QLOG16_ID ) )
            {
                g_compression_type = QLOG16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else {
                free_ptr( new_compressor );
                FINALIZE( getMsg( MSG_INVALIDARG ) );
            }
            
            free_ptr( new_compressor );
            g_compression_level = new_compression_level;
        }
    }
    else if( !_strcmpi( command, "compression_check" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_compression_check );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), getMsg( MSG_INVALIDARG ) );
            g_compression_check = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else
    {
        /*
         * database id < 0? That's an error...
         */
        if( db_id < 0 )
        {
            FINALIZE( getMsg( MSG_INVALIDDBHANDLE ) );
        }
        
        /*
         * database not open? -> error
         */
        FINALIZE_IF( !g_dbs[db_id], getMsg( MSG_DBNOTOPEN ) );
        
        /*
         * Every unknown command is treated as an sql query string
         */
        const char* query = command;

        /*
         * emulate the "show tables" sql query
         */
        if( !_strcmpi( query, "show tables" ) )
        {
            query = "SELECT name as tablename FROM sqlite_master "
                    "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                    "UNION ALL "
                    "SELECT name as tablename FROM sqlite_temp_master "
                    "WHERE type IN ('table','view') "
                    "ORDER BY 1";
        }

        /*
         * complete the query
         */
        // TODO: What does this test? sqlite3_complete() returns 1 if the string is complete and valid...
        FINALIZE_IF( sqlite3_complete( query ) != 0, getMsg( MSG_INVQUERY ) );
        
        /*
         * and prepare it
         * if anything is wrong with the query, than complain about it.
         */
        FINALIZE_IF( SQLITE_OK != sqlite3_prepare_v2( g_dbs[db_id], query, -1, &st, 0 ), SQL_ERR );
        
        /*
         * Parameter binding 
         */
        int bind_names_count = sqlite3_bind_parameter_count( st );
        
        // the number of arguments may not exceed the number of placeholders
        // in the sql statement
        FINALIZE_IF( NumArgs > bind_names_count, getMsg( MSG_UNEXPECTEDARG ) );

        // collect input arguments in a cell array
        if(    !NumArgs                                    // results in an empty cell array
            || !mxIsCell( prhs[FirstArg] )                 // mxIsCell() not called when NumArgs==0 !
            || typed_blobs_mode_check(TYBLOB_BYSTREAM) )   // if serialized items are allowed, no encapsulated parameters are
        {
            // Collect parameter list in one cell array
            pArgs = mxCreateCellMatrix( NumArgs, 1 );
            for( int i = 0; pArgs && i < NumArgs; i++ )
            {
                // deep copy into cell array
                mxSetCell( pArgs, i, mxDuplicateArray( prhs[FirstArg+i] ) );
            }
        }
        else
        {
            // Parameters may be (and are) packed in only one single 
            // cell array
            FINALIZE_IF( NumArgs > 1, getMsg( MSG_UNEXPECTEDARG ) );

            // Make a deep copy
            pArgs = mxDuplicateArray( prhs[FirstArg] );
            
            FINALIZE_IF( (int)mxGetNumberOfElements( pArgs ) > bind_names_count, getMsg( MSG_UNEXPECTEDARG ) );
        }
        
        // if parameters needed for parameter binding, 
        // at least one parameter has to be passed 
        FINALIZE_IF( !pArgs, getMsg( MSG_MISSINGARGL ) );

        for( int iParam = 0; iParam < bind_names_count; iParam++ )
        {
            // item must not be destroyed! (Matlab would crash)
            const mxArray *pItem = NULL;
            
            if( iParam + 1 <= (int)mxGetNumberOfElements( pArgs ) )
            {
                pItem = mxGetCell( pArgs, iParam );
            }
                

            if( !pItem || mxIsEmpty( pItem ) )
            {
                if( SQLITE_OK != sqlite3_bind_null( st, iParam+1 ) )
                {
                    FINALIZE( SQL_ERR );
                }
                continue;
            }

            bool      bStreamable       = typed_blobs_mode_check(TYBLOB_BYSTREAM);
            size_t    szElement         = mxGetElementSize( pItem );      // size of one element in bytes
            size_t    cntElements       = mxGetNumberOfElements( pItem ); // number of elements in cell array
            mxClassID clsid             = mxGetClassID( pItem );
            int       iTypeComplexity   = Compressor::get_type_complexity( pItem, bStreamable );
            char*     str_value         = NULL;

            switch( iTypeComplexity )
            {
              case Compressor::TC_COMPLEX:
                // structs, cells, complex data (SQLite typed ByteStream BLOB)
                // can only be stored as undocumented byte stream
                if( !bStreamable )
                {
                    FINALIZE( getMsg( MSG_INVALIDARG ) );
                }
                /* otherwise fallthrough */
              case Compressor::TC_SIMPLE_ARRAY:
                // multidimensional non-complex numeric or char arrays
                // will be stored as vector(!).
                // Caution: Array dimensions are lost!
                /* otherwise fallthrough */
              case Compressor::TC_SIMPLE_VECTOR:
                // non-complex numeric vectors (SQLite BLOB)
                if( typed_blobs_mode_check(TYBLOB_NO) )
                {
                    // matrix arguments are omitted as blobs, except string arguments
                    // data is attached as is, no header information here
                    const void* blob = mxGetData( pItem );
                    // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, 
                                                        (int)(cntElements * szElement),
                                                        SQLITE_TRANSIENT ) )
                    {
                        FINALIZE( SQL_ERR );
                    }
                } 
                else 
                {
                    void*  blob          = NULL;
                    size_t blob_size     = 0;
                    double process_time  = 0.0;
                    double ratio         = 0.0;
                    
                    /* blob_pack() modifies g_finalize_msg */
                    FINALIZE_IF( !blob_pack( pItem, &blob, &blob_size, &process_time, &ratio ), g_finalize_msg );
                    
                    // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, (int)blob_size, sqlite3_free ) )
                    {
                        FINALIZE( SQL_ERR );
                    }  
                }
                break;
              case Compressor::TC_SIMPLE:
                // 1-byte non-complex scalar, char or simple string (SQLite simple types)
                switch( clsid )
                {
                    case mxLOGICAL_CLASS:
                    case mxINT8_CLASS:
                    case mxUINT8_CLASS:
                    case mxINT16_CLASS:
                    case mxINT32_CLASS:
                    case mxUINT16_CLASS:
                    case mxUINT32_CLASS:
                        // scalar integer value
                        if( SQLITE_OK != sqlite3_bind_int( st, iParam+1, (int)mxGetScalar( pItem ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxDOUBLE_CLASS:
                    case mxSINGLE_CLASS:
                        // scalar floating point value
                        if( SQLITE_OK != sqlite3_bind_double( st, iParam+1, mxGetScalar( pItem ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxCHAR_CLASS:
                    {
                        // string argument
                        char* str_value = mxArrayToString( pItem );
                        if( str_value && g_convertUTF8 )
                        {
                            int len = latin2utf( (unsigned char*)str_value, NULL ); // get the size only
                            unsigned char *temp = (unsigned char*)mxCalloc( len, sizeof(char) ); // allocate memory
                            if( temp )
                            {
                                latin2utf( (unsigned char*)str_value, temp );
                                free_ptr( str_value );
                                str_value = (char*)temp;
                            }
                            else
                            {
                                free_ptr( str_value );  // Needless due to mexErrMsgTxt(), but clean
                                FINALIZE( getMsg( MSG_ERRMEMORY ) );
                            }
                        }
                        // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
                        if( SQLITE_OK != sqlite3_bind_text( st, iParam+1, str_value, -1, SQLITE_TRANSIENT ) )
                        {
                            free_ptr( str_value );  // Needless due to mexErrMsgTxt(), but clean
                            FINALIZE( SQL_ERR );
                        }
                        free_ptr( str_value );
                        break;
                    }
                } // end switch
                break;
              case Compressor::TC_EMPTY:
                // Empty parameters are omitted as NULL by sqlite
                continue;
              default:
                // all other (unsuppored types)
                FINALIZE( getMsg( MSG_INVALIDARG ) );
                break;
            }
        }

        /*
         * Any results?
         */
        int ncol = sqlite3_column_count( st );
        if( ncol > 0 )
        {
            char **fieldnames = new char *[ncol];   /* Column names */
            Values* allrows = 0;                    /* All query results */
            Values* lastrow = 0;                    /* pointer to the last result row */
            int rowcount = 0;                       /* number of result rows */
            
            /*
             * Get the column names of the result set
             */
            for( int iCol = 0; iCol < ncol; iCol++ )
            {
                const char *cname = sqlite3_column_name( st, iCol );
                
                fieldnames[iCol] = new char [strlen(cname) +1];
                strcpy( fieldnames[iCol], cname );
                /*
                 * replace invalid chars by '_', so we can build
                 * valid MATLAB structs
                 */
                char *mk_c = fieldnames[iCol];
                while( *mk_c )
                {
//                    if ((*mk_c == ' ') || (*mk_c == '*') || (*mk_c == '?') || !isprint(*mk_c))
                    if( !isalnum( *mk_c ) )
                        *mk_c = '_';
                    mk_c++;
                }
                
                if( fieldnames[iCol][0] == '_' )
                {
                    fieldnames[iCol][0] = 'X';  // fieldnames must not start with an underscore
                                                // TODO: Any other ideas?
                }
            }
            /*
             * Check for duplicate colum names
             */
            if( g_check4uniquefields )
            {
                for( int iCol = 0; iCol < (ncol -1); iCol++ )
                {
                    for( int jCol = iCol+1; jCol < ncol; jCol++ )
                    {
                        if( !strcmp( fieldnames[iCol], fieldnames[jCol] ) )
                        {
                            /*
                             * Change the duplicate colum name to be unique
                             * by adding _x to it. x counts from 1 to 99
                             */
                            size_t fnamelen = strlen( fieldnames[jCol] ) + 4;
                            char *newcolumnname = new char [fnamelen];
                            int k;
                            
                            for( k = 1; k < 100; k++ )
                            {
                                /*
                                 * Build new name
                                 */
                                _snprintf( newcolumnname, fnamelen, "%s_%d", fieldnames[jCol], k );

                                /*
                                 * is it already unique? */
                                bool unique = true;
                                for( int lCol = 0; lCol < ncol; lCol++ )
                                {
                                    if( !strcmp( fieldnames[lCol], newcolumnname ) )
                                    {
                                        unique = false;
                                        break;
                                    }
                                }
                                if( unique ) break;
                            }
                            if( k == 100 )
                            {
                                mexWarnMsgTxt( getMsg( MSG_MSGUNIQUEWARN ) );
                            }
                            else
                            {
                                /*
                                 * New unique Identifier found, assign it
                                 */
                                delete [] fieldnames[jCol];
                                fieldnames[jCol] = newcolumnname;
                            }
                        }
                    }
                }
            }
            /*
             * get the result rows from the engine
             *
             * We cannot get the number of result lines, so we must
             * read them in a loop and save them into an temporary list.
             * Later, we can transfer this List into an MATLAB array of structs.
             * This way, we must allocate enough memory for two result sets,
             * but we save time by allocating the MATLAB Array at once.
             */
            for(;;)
            {
                /*
                 * Advance to the next row
                 */
                int step_res = sqlite3_step( st );

                if( step_res == SQLITE_ERROR )
                {
                    g_finalize_msg = SQL_ERR;
                }

                /*
                 * no row left? break out of the loop
                 */
                if( step_res != SQLITE_ROW ) break;

                /*
                 * get new memory for the result
                 */
                Values* RecordValues = new Values( ncol );
                
                Value *v = RecordValues->m_Values;
                for( int jCol = 0; jCol < ncol; jCol++, v++ )
                {
                     int fieldtype = sqlite3_column_type( st, jCol );

                     v->m_Type = fieldtype;
                     v->m_Size = 0;
                     
                     switch( fieldtype )
                     {
                         case SQLITE_NULL:      v->m_NumericValue = g_NaN;                                                       break;
                         case SQLITE_INTEGER:   v->m_NumericValue = (double)sqlite3_column_int64( st, jCol );                    break;
                         case SQLITE_FLOAT:     v->m_NumericValue = (double)sqlite3_column_double( st, jCol );                   break;
                         case SQLITE_TEXT:      v->m_StringValue  = strnewdup( (const char*)sqlite3_column_text( st, jCol ) );   break;
                         case SQLITE_BLOB:      
                            {
                                v->m_Size = sqlite3_column_bytes( st, jCol );
                                if( v->m_Size > 0 )
                                {
                                    v->m_StringValue = new char[v->m_Size];
                                    memcpy( v->m_StringValue, sqlite3_column_blob( st, jCol ), v->m_Size );
                                }
                                else
                                {
                                    v->m_Size = 0;
                                }
                            }
                            break;
                         default:
                            FINALIZE( getMsg( MSG_UNKNWNDBTYPE ) );
                     }
                }
                /*
                 * and add this row to the list of all result rows
                 */
                if( !lastrow )
                {
                    allrows = lastrow = RecordValues;
                }
                else
                {
                    lastrow->m_NextValues = RecordValues;
                    lastrow = lastrow->m_NextValues;
                }
                /*
                 * we have one more...
                 */
                rowcount++;
            }
            
            /*
             * end the sql engine
             */
            sqlite3_finalize( st );
            st = NULL;

            /*
             * got nothing? return an empty result to MATLAB
             */
            if( rowcount == 0 || ! allrows )
            {
                plhs[0] = mxCreateDoubleMatrix( 0, 0, mxREAL );
                FINALIZE_IF( NULL == plhs[0], getMsg( MSG_CANTCREATEOUTPUT ) );
            }
            else
            {
                /*
                 * Allocate an array of MATLAB structs to return as result
                 */
                int ndims[2];
                int index;
                
                ndims[0] = rowcount;
                ndims[1] = 1;
                
                plhs[0] = mxCreateStructArray( 2, ndims, ncol, (const char**)fieldnames );
                FINALIZE_IF( NULL == plhs[0], getMsg( MSG_CANTCREATEOUTPUT ) );
                
                /*
                 * transfer the result rows from the temporary list into the result array
                 */
                lastrow = allrows;
                index = 0;
                while( lastrow )
                {
                    Value* recordvalue = lastrow->m_Values;
                    
                    for( int fieldnr = 0; fieldnr < ncol; fieldnr++, recordvalue++ )
                    {
                        if( recordvalue -> m_Type == SQLITE_TEXT )
                        {
                            mxArray* c = mxCreateString( recordvalue->m_StringValue );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, c );
                        }
                        else if( recordvalue->m_Type == SQLITE_NULL && !g_NULLasNaN )
                        {
                            mxArray* out_double = mxCreateDoubleMatrix( 0, 0, mxREAL );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                        }
                        else if( recordvalue->m_Type == SQLITE_BLOB )
                        {
                            if( recordvalue->m_Size > 0 )
                            {
                                if( typed_blobs_mode_check(TYBLOB_NO) )
                                {
                                    mwSize NumDims[2]={1,1};
                                    NumDims[1] = recordvalue->m_Size;
                                    mxArray* out_uchar8 = mxCreateNumericArray( 2, NumDims, mxUINT8_CLASS, mxREAL );
                                    unsigned char *v = (unsigned char *) mxGetData( out_uchar8 );
                                   
                                    memcpy( v, recordvalue->m_StringValue, recordvalue->m_Size );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, out_uchar8 );
                                } 
                                else 
                                {
                                    void*    blob         = (void*)recordvalue->m_StringValue;
                                    size_t   blob_size    = (size_t)recordvalue->m_Size;
                                    mxArray* pFieldItem;
                                    double   process_time = 0.0;
                                    double   ratio = 0.0;
                                    
                                    /* blob_unpack() modifies g_finalize_msg */
                                    FINALIZE_IF( !blob_unpack( blob, blob_size, &pFieldItem, &process_time, &ratio ), g_finalize_msg );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, pFieldItem );
                                }
                            } 
                            else 
                            {
                                // empty BLOB
                                mxArray* out_double = mxCreateDoubleMatrix( 0, 0, mxREAL );
                                mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                            }
                        } 
                        else 
                        {
                            mxArray* out_double = mxCreateDoubleScalar( recordvalue->m_NumericValue );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                        }
                    }
                    allrows = lastrow;
                    lastrow = lastrow->m_NextValues;
                    delete allrows;
                    index++;
                }
            }
            
            for( int iCol=0; iCol<ncol; iCol++ )
            {
                delete [] fieldnames[iCol];
            }
            delete [] fieldnames;
        }
        else
        {
            /*
             * no result, cleanup the sqlite engine
             */
            int res = sqlite3_step( st );
            sqlite3_finalize( st );
            st = NULL;

            plhs[0] = mxCreateDoubleMatrix( 0, 0, mxREAL );
            FINALIZE_IF( NULL == plhs[0], getMsg( MSG_CANTCREATEOUTPUT ) );
            FINALIZE_IF( SQLITE_DONE != res, SQL_ERR );
        }
    }
        
finalize:        
    if( st )
    {
        sqlite3_clear_bindings( st );
        sqlite3_finalize( st );
    }
    
    free_ptr( command );  
    destroy_array( pArgs );

    // mexErrMsg*() functions automatically free all 
    // allocated memory by mxCalloc() ans mxCreate*() functions.

    if( g_finalize_msg == SQL_ERR || g_finalize_msg == SQL_ERR_CLOSE )
    {
        const char *msg_id = trans_err_to_ident( g_dbs[db_id] );
        char msg_err[1024];
        
        _snprintf( msg_err, 1024, "SQLite: \"%s\"", sqlite3_errmsg( g_dbs[db_id] ) );
        
        if( g_finalize_msg == SQL_ERR_CLOSE )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", getMsg( MSG_ERRCANTCLOSE ) );
            }
        }
        
        mexErrMsgIdAndTxt( msg_id, msg_err );
    }
    
    if( g_finalize_msg )
    {
        mexErrMsgTxt( g_finalize_msg );
    }
}




// Time measuring functions
// Windows
#ifdef _WIN32
#include <windows.h>

double get_wall_time()
{
    LARGE_INTEGER time,freq;
    
    if( !QueryPerformanceFrequency( &freq ) )
    {
        // Handle error
        return 0;
    }
    
    if( !QueryPerformanceCounter( &time ) )
    {
        // Handle error
        return 0;
    }
    
    return (double)time.QuadPart / freq.QuadPart;
}


double get_cpu_time()
{
    FILETIME a,b,c,d;
    if( GetProcessTimes( GetCurrentProcess(), &a, &b, &c, &d ) != 0 )
    {
        //  Returns total user time.
        //  Can be tweaked to include kernel times as well.
        return
            (double)( d.dwLowDateTime |
            ( (unsigned long long)d.dwHighDateTime << 32 ) ) * 0.0000001;
    } 
    else 
    {
        //  Handle error
        return 0;
    }
}

//  Posix/Linux
#else
#include <time.h>
#include <sys/time.h>
double get_wall_time()
{
    struct timeval time;
    
    if( gettimeofday( &time, NULL ) )
    {
        //  Handle error
        return 0;
    }
    
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

double get_cpu_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}
#endif


// create a compressed typed blob from a Matlab item (deep copy)
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_size, double *pdProcess_time, double* pdRatio )
{
    assert( NULL != ppBlob && NULL != pBlob_size && NULL != pdProcess_time && NULL != pdRatio );
    *ppBlob         = NULL;
    *pBlob_size     = 0;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    bool bStreamable = typed_blobs_mode_check(TYBLOB_BYSTREAM);
    Compressor bag( bStreamable );
    
    /* 
     * create a typed blob. Header information is generated
     * according to value and type of the matrix and the machine
     */
    // setCompressor() always returns true, since parameters had been checked already
    (void)bag.setCompressor( g_compression_type, g_compression_level );
    FINALIZE_IF( !bag.attachStreamableCopy( const_cast<mxArray*>( pItem ) ), g_finalize_msg );

    if( g_compression_level )
    {
        double start_time = get_wall_time();
        
        bag.pack(); // allocates m_cdata
        
        *pdProcess_time = get_wall_time() - start_time;
        
        // did the compressor ommits compressed data?
        if( bag.m_cdata_size > 0 )
        {
            size_t blob_size_uncompressed;
            
            *pBlob_size = 
                TypedBLOBHeaderV2::dataOffset( bag.m_nDims ) +
                bag.m_cdata_size;
            
            blob_size_uncompressed =
                TypedBLOBHeaderV1::dataOffset( bag.m_nDims ) +
                bag.m_rdata_size;
            
            // calculate the compression ratio
            if( blob_size_uncompressed > 0 )
            {
                *pdRatio = (double)*pBlob_size / (double)blob_size_uncompressed;
            }
            else
            {
                *pdRatio = 0.0;
            }
            
            if( *pBlob_size >= blob_size_uncompressed )
            {
                bag.m_cdata_size = 0; // Switch zu uncompressed blob, it's not worth the efford
            }
        }

        // is there compressed data to store in the blob?
        if( bag.m_cdata_size >  0 )
        {
            TypedBLOBHeaderV2* tbh2 = NULL;
            
            // discard data if it exeeds max allowd size by sqlite
            if( *pBlob_size > CONFIG_MKSQLITE_MAX_BLOB_SIZE )
            {
                FINALIZE( getMsg( MSG_BLOBTOOBIG ) );
            }

            // allocate space for a typed blob containing compressed data
            tbh2 = (TypedBLOBHeaderV2*)sqlite3_malloc( (int)*pBlob_size );
            if( NULL == tbh2 )
            {
                FINALIZE( getMsg( MSG_ERRMEMORY ) );
            }

            // blob typing...
            tbh2->init( bag.m_pItem );
            tbh2->setCompressor( bag.m_strCompressorType );

            // ...and copy compressed data
            // TODO: Do byteswapping here if big endian? 
            // (Most platforms use little endian)
            memcpy( (char*)tbh2->getData(), bag.m_cdata, bag.m_cdata_size );

            // check if compressed data equals to original?
            if( g_compression_check )
            {
                int check_id = 0xdeadbeef;
                size_t rdata_size   = bag.m_rdata_size;
                void* rdata         = mxMalloc( rdata_size + 2 * sizeof( check_id ) );
                char* pData         = (char*)rdata;
                int ok              = 0;

                if( NULL == rdata )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( getMsg( MSG_ERRMEMORY ) );
                }

                // mark data bounds to check for range violation
                memcpy( pData, &check_id, sizeof( check_id ) );
                pData += sizeof( check_id );
                memcpy( pData + rdata_size, &check_id, sizeof( check_id ) );
                
                // unpacking data to pData
                bag.m_rdata = pData;
                
                // inflate compressed data again
                if( !bag.unpack() )
                {
                    sqlite3_free( tbh2 );

                    free_ptr( rdata );  // Needless due to mexErrMsgTxt(), but clean
                    FINALIZE( getMsg( MSG_ERRCOMPRESSION ) );
                }

                // check if data is equal and bounds are not violated
                ok = (    rdata_size == bag.m_rdata_size 
                       && 0 == memcmp( bag.m_rdata, pData, rdata_size ) 
                       && 0 == memcmp( rdata,               &check_id, sizeof( check_id ) )
                       && 0 == memcmp( pData + rdata_size, &check_id, sizeof( check_id ) ) );

                free_ptr( rdata );

                if( !ok )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( getMsg( MSG_ERRCOMPRESSION ) );
                }
            }

            // store the typed blob with compressed data as return parameter
            *ppBlob = (void*)tbh2;

        }
    }

    // if compressed data exceeds uncompressed size, it will be stored as
    // uncompressed typed blob
    if( !*ppBlob )
    {
        TypedBLOBHeaderV1* tbh1 = NULL;

        /* Without compression, raw data is copied into blob structure as is */
        *pBlob_size = TypedBLOBHeaderV1::dataOffset( bag.m_nDims ) +
                       bag.m_rdata_size;

        FINALIZE_IF( *pBlob_size > CONFIG_MKSQLITE_MAX_BLOB_SIZE, getMsg( MSG_BLOBTOOBIG ) );

        tbh1 = (TypedBLOBHeaderV1*)sqlite3_malloc( (int)*pBlob_size );
        if( NULL == tbh1 )
        {
            FINALIZE( getMsg( MSG_ERRMEMORY ) );
        }

        // blob typing...
        tbh1->init( bag.m_pItem );

        // and copy uncompresed data
        // TODO: Do byteswapping here if big endian? 
        // (Most platforms use little endian)
        memcpy( tbh1->getData(), bag.m_rdata, bag.m_rdata_size );

        *ppBlob = (void*)tbh1;
    }
    
    // mark data type as "unknown", means that it's a serialized item as byte stream
    if( bag.m_bIsByteStream )
    {
        ((TypedBLOBHeaderV1*)*ppBlob)->m_clsid = mxUNKNOWN_CLASS;
    }
    
finalize:
  
    // cleanup
    // m_rdata points to m_pItem data structure and may not be freed!
    bag.m_rdata = NULL;
    free_ptr( bag.m_cdata );
    destroy_array( bag.m_pItem );
    
    return NULL != *ppBlob;
}


// uncompress a typed blob and return its matlab item
int blob_unpack( const void* pBlob, size_t blob_size, mxArray** ppItem, double* pdProcess_time, double* pdRatio )
{
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    assert( NULL != ppItem && NULL != pdProcess_time && NULL != pdRatio );
    *ppItem         = NULL;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    tbhv1_t* tbh1 = (tbhv1_t*)pBlob;
    tbhv2_t* tbh2 = (tbhv2_t*)pBlob;
    
    bool bStreamable = typed_blobs_mode_check(TYBLOB_BYSTREAM);
    Compressor bag( bStreamable );

    /* test valid platform */
    if( !tbh1->validPlatform() )
    {
        mexWarnMsgIdAndTxt( "MATLAB:MKSQLITE:BlobDiffArch", getMsg( MSG_WARNDIFFARCH ) );
        // TODO: warning, error or automatic conversion..?
        // since mostly platforms (except SunOS) use LE encoding
        // and unicode is not supported here, there is IMHO no need 
        // for conversions...
        
        // Warning can switched off via:
        // warning( 'off', 'MATLAB:MKSQLITE:BlobDiffArch' );
    }

    /* check for valid header */
    if( !tbh1->validMagic() )
    {
        FINALIZE( getMsg( MSG_UNSUPPTBH ) );
    }

    FINALIZE_IF( !tbh1->validClsid(), getMsg( MSG_UNSUPPVARTYPE ) );
    
    // serialized item marked as "unknown" is a byte stream
    bag.m_bIsByteStream = ( tbh1->m_clsid == mxUNKNOWN_CLASS );

    switch( tbh1->m_ver )
    {
      // typed blob with uncompressed data
      case sizeof( tbhv1_t ):
      {
          // space allocation (check later)
          bag.attachItem( tbh1->createNumericArray( /* doCopyData */ true ) );
          break;
      }

      // typed blob with compressed data
      case sizeof( tbhv2_t ):
      {
          // space allocation (check later)
          bag.attachItem( tbh2->createNumericArray( /* doCopyData */ false ) );
          
          bag.m_cdata        = tbh2->getData();
          bag.m_cdata_size   = blob_size - tbh2->dataOffset();
          
          // space allocated?
          if( bag.m_pItem )
          {
              if( !tbh2->validCompression() )
              {
                  FINALIZE( getMsg( MSG_UNKCOMPRESSOR ) );
              }

              bag.setCompressor( tbh2->m_compression );

              double start_time = get_wall_time();
              
              if( !bag.unpack() )
              {
                  FINALIZE( getMsg( MSG_ERRCOMPRESSION ) );
              } 
              else 
              {
                  *pdProcess_time = get_wall_time() - start_time;
                  
                  if( bag.m_rdata_size > 0 )
                  {
                      *pdRatio = (double)bag.m_cdata_size / (double)bag.m_rdata_size;
                  }
                  else
                  {
                      *pdRatio = 0.0;
                  }
                  
                  // TODO: Do byteswapping here if needed, depend on endian?
              }
          }
          break;
      }

      default:
          FINALIZE( getMsg( MSG_UNSUPPTBH ) );
    }

    FINALIZE_IF( NULL == bag.m_pItem, getMsg( MSG_ERRMEMORY ) );
    
    if( bag.m_bIsByteStream  )
    {
        mxArray* pDeStreamed = NULL;
        
        if( !deserialize( bag.m_pItem, pDeStreamed ) )
        {
            FINALIZE( getMsg( MSG_ERRMEMORY ) );
        }
        
        destroy_array( bag.m_pItem );
        bag.attachItem( pDeStreamed );
    }

    *ppItem = bag.m_pItem;
    bag.m_pItem = NULL;
      
finalize:
    
    // cleanup
    // rdata and cdata points to memory allocated outside
    bag.m_rdata = NULL;
    bag.m_cdata = NULL;

    // bag.m_pItem is non-NULL when an error occured only
    destroy_array( bag.m_pItem );

    return NULL != *ppItem;
}
