/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      mksqlite.cpp
 *  @brief     Sql class and mksqlite class, main routine
 *  @details
 *  @author    Martin Kortmann <mail@kortmann.de>
 *  @author    Andreas Martin  <andi.martin@gmx.net>
 *  @version   2.0
 *  @date      2008-2014
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

/* following define is not really used yet, since this is only one module */
#define MAIN_MODULE

#include "config.h"                 // Defaults
#include "global.hpp"               // Global definitions and stati
#include "serialize.hpp"            // Serialization of MATLAB variables (undocumented feature)
#include "number_compressor.hpp"    // Some compressing algorithms
#include "typed_blobs.hpp"          // Packing into typed blobs with variable type storage
#include "utils.hpp"                // Utilities 
#include "sql_interface.hpp"        // SQLite interface
#include "locale.hpp"               // (Error-)Messages
#include <vector>

#define STRMATCH(strA,strB)       ( (strA) && (strB) && ( 0 == _strcmpi( (strA), (strB) ) ) )
#define FINALIZE_STR( message )   mexErrMsgTxt( message )
#define FINALIZE( identifier )    FINALIZE_STR( ::getLocaleMsg( identifier ) )

#define DBID_NEW_OR_ALL -1

// static assertion (compile time), ensures int32_t and mwSize as 4 byte data representation
static char SA_UIN32[ (sizeof(uint32_t)==4 && sizeof(mwSize)==4) ? 1 : -1 ]; 

/* Static assertion: Ensure backward compatibility */
static char SA_TBH_BASE[ sizeof( TypedBLOBHeaderV1 ) == 36 ? 1 : -1 ];

/* declare the MEX Entry function as pure C */
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* SQLite engine */



static struct Sql
{
    enum { COUNT_DB = CONFIG_MAX_NUM_OF_DBS };
    
    SQL m_db[COUNT_DB];     /* SQLite Interface */
    int m_dbid;             /* database id, base 0 */
    
    Sql(): m_dbid(0) {};
    
    ~Sql()
    {
        (void)closeAllDbs();
        sqlite3_shutdown();
    }
    
    // check if database id is in valid range
    bool isValidId( int newId )
    {
        return newId >= 0 && newId < COUNT_DB;
    }
    
    void switchTo( int newId )
    {
        assert( isValidId( newId ) );
        m_dbid = newId;
    }
    
    SQL& current()
    {
        return m_db[m_dbid];
    }
    
    void printStati()
    {
        for( int i = 0; i < COUNT_DB; i++ )
        {
            mexPrintf( "DB Handle %d: %s\n", i, m_db[i].isOpen() ? "OPEN" : "CLOSED" );
        }
    }
    
    // returns the first next free id slot (base 0). Database must be closed
    int getNextFreeId()
    {
        /*
         * If there isn't an database id, then try to get one
         */
        for( int i = 0; i < COUNT_DB; i++ )
        {
            if( !m_db[i].isOpen() )
            {
                return i;
            }
        }
        
        return -1;  // no free slot available
    }
    
    bool closeAllDbs()
    {
        bool anyOpen = false;

        for( int i = 0; i < COUNT_DB; i++ )
        {
            if( m_db[i].isOpen() )
            {
                anyOpen = true;
                m_db[i].closeDb();
            }
        }
        
        return anyOpen;
    }
    
} Sql;



void mex_module_deinit()
{
    if( Sql.closeAllDbs() )
    {
        /*
         * inform the user, databases have been closed
         */
        mexWarnMsgTxt( ::getLocaleMsg( MSG_CLOSINGFILES ) );
    }
    
    blosc_destroy();
}


void mex_module_init()
{
    mxArray *plhs[3] = {0};

    if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
    {
        g_compression_type = BLOSC_DEFAULT_ID;
        blosc_init();
        mexAtExit( mex_module_deinit );
        typed_blobs_init();

        mexPrintf( ::getLocaleMsg( MSG_HELLO ), 
                   sqlite3_libversion() );
        mexPrintf( "Platform: %s, %s\n\n", 
                   TBH_platform, 
                   TBH_endian[0] == 'L' ? "little endian" : "big endian" );

        g_is_initialized = true;
    }
    else
    {
        FINALIZE( MSG_ERRPLATFORMDETECT );
    }
    
    if( 0 == mexCallMATLAB( 1, plhs, 0, NULL, "namelengthmax" ) )
    {
        g_namelengthmax = (int)mxGetScalar( plhs[0] );
    }
}







class Mksqlite
{
    int               m_nlhs;       // count of left hand side arguments
    int               m_narg;       // count of right hand side arguments
    mxArray**         m_plhs;       // pointer to current left hand side argument
    const mxArray**   m_parg;       // pointer to current right hand side argument
    char*             m_command;    // SQL command. Allocated and freed by this class
    const char*       m_query;      // m_command, or a translation from m_command
    int               m_dbid;       // database id for next command (-1: argument missing, 0: use first free slot, 1..:database id)
    Err               m_err;        // recent error
    
    /* inhibit assignment, default and copy ctors */
    Mksqlite();
    Mksqlite( const Mksqlite& );
    Mksqlite& operator=( const Mksqlite& );
    
public:
    Mksqlite( int nlhs, mxArray** plhs, int nrhs, const mxArray** prhs )
    : m_nlhs( nlhs ), m_plhs( plhs ), 
      m_narg( nrhs ), m_parg( prhs ),
      m_command(NULL), m_query(NULL), m_dbid(-1)
    {
        /*
         * no argument -> fail
         */
        if( nrhs < 1 )
        {
            mexPrintf( "%s", ::getLocaleMsg( MSG_USAGE ) );
            m_err.set( MSG_INVALIDARG );
        }
    }
    
    
    ~Mksqlite()
    {
        if( m_command )
        {
            ::utils_free_ptr( m_command );
        }
    }
    

    bool errPending()
    {
        return m_err.isPending();
    }
    
    
    void errClear()
    {
        m_err.clear();
    }
    
    
    // Aborts the running function with an error message. Allocated memory (by 
    // MATLAB allocation functions) is freed automatically
    void returnWithError()
    {
        assert( errPending() );
        
        mexErrMsgTxt( m_err.get() );
    }
    
    
    // Read an integer parameter at current argument read position, and
    // write to refValue (as 0 or 1 if abBoolInt is set to true)
    bool argGetNextInteger( int& refValue, bool asBoolInt = false )
    {
        if( errPending() ) return false;

        if( m_narg < 1 ) 
        {
            m_err.set( MSG_MISSINGARG );
            return false;
        }
        else if( !mxIsNumeric( m_parg[0] ) )
        {
            m_err.set( MSG_NUMARGEXPCT );
            return false;
        }
        
        refValue = ValueMex( m_parg[0] ).GetInt();
        if( asBoolInt )
        {
            refValue = ( refValue != 0 );
        }

        m_parg++;
        m_narg--;

        return true;
    }

    
    // Reads the next argument if it is numeric and interprets as dbid.
    // m_dbid is set either to the read dbid, or if parameter is missing,
    // the first dbid is used. If user entered an id == 0, the next free slot 
    // will be choosen.
    bool argTryReadValidDbid()
    {
        if( errPending() ) return false;

        int new_dbid;
        
        /*
         * Check if the first argument is a number (base 1), then we have to use
         * this number as the new database id.
         */
        if( argGetNextInteger( new_dbid, /*asBoolInt*/ false ) )
        {
            if( !Sql.isValidId( new_dbid-1 ) && new_dbid != 0 )
            {
                m_err.set( MSG_INVALIDDBHANDLE );
                return false;
            }
            else
            {
                m_dbid = new_dbid;
            }
        }
        
        // argGetNextInteger() may fail, if no dbid was given. 
        // But it's legal!
        m_err.clear();
        return true;
    }
    
    
    // read the command from current argument position, always a string and thus 
    // asserted.
    bool argReadCommand()
    {
        if( errPending() ) return false;
        
        /*
         * The next (or first if no db number available) is the m_command,
         * it has to be a string.
         * This fails also, if the first arg is a db-id and there is no 
         * further argument
         */
        if( !m_narg || !mxIsChar( m_parg[0] ) )
        {
            mexPrintf( "%s", ::getLocaleMsg( MSG_USAGE ) );
            m_err.set( MSG_INVALIDARG );
            return false;
        }
        else
        {
            /*
             * Get the m_command string
             */
            m_command = ValueMex( m_parg[0] ).GetString();
            m_parg++;
            m_narg--;
            
        }
        
        return true;
    }
    
    
    // test current command as flag parameter with it's new value.
    // strMatchFlagName holds the name of the flag to test.
    bool cmdTryHandleFlag( const char* strMatchFlagName, int& refFlag )
    {
        if( errPending() || !STRMATCH( m_command, strMatchFlagName ) ) 
        {
            return false;
        }

        int iOldValue = refFlag;

        if( m_narg > 1 ) 
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }

        if( m_narg > 0 && !argGetNextInteger( refFlag, /*asBoolInt*/ true ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        // always return old flag value
        m_plhs[0] = mxCreateDoubleScalar( (double)iOldValue );

        return true;
    }
    

    // test current command as version query to sqlite or mksqlite version numbers
    // strCmdMatchVerMex and strCmdMatchVerSql hold the mksqlite command names
    bool cmdTryHandleVersion( const char* strCmdMatchVerMex, const char* strCmdMatchVerSql )
    {
        if( errPending() ) return false;
        
        if( STRMATCH( m_command, strCmdMatchVerMex ) )
        {
            if( m_narg > 0 ) 
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }
            else
            {
                if( m_nlhs == 0 )
                {
                    mexPrintf( "mksqlite Version %s\n", MKSQLITE_VERSION_STRING );
                } 
                else
                {
                    m_plhs[0] = mxCreateString( MKSQLITE_VERSION_STRING );
                }
            }
            return true;
        } 
        else if( STRMATCH( m_command, strCmdMatchVerSql ) )
        {
            if( m_narg > 0 ) 
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }
            else
            {
                if( m_nlhs == 0 )
                {
                    mexPrintf( "SQLite Version %s\n", SQLITE_VERSION_STRING );
                } 
                else 
                {
                    m_plhs[0] = mxCreateString( SQLITE_VERSION_STRING );
                }
            }
            return true;
        }
        
        return false;
    }
    
    
    // try to interpret current command as blob mode setting
    // strCmdMatchName hold the mksqlite command name
    bool cmdTryHandleTypedBlob( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }

        int old_mode = typed_blobs_mode_get();
        int new_mode = old_mode;

        if( m_narg > 1 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }

        if( m_narg > 0 && !argGetNextInteger( new_mode ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }

        if( new_mode != old_mode )
        {
            if( new_mode < 0 || new_mode > TYBLOB_MAX_ID )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }

            typed_blobs_mode_set( (typed_blobs_e)new_mode );
        
            if( g_streaming && typed_blobs_mode_check( TYBLOB_NO ) )
            {
                mexPrintf( ::getLocaleMsg( MSG_STREAMINGNEEDTYBLOBS ) );
                g_streaming = false;
            }
        }

        // always return the old value
        m_plhs[0] = mxCreateDoubleScalar( (double)old_mode );

        return true;
    }
    
    
    // try to interpret current command as setting for sqlite extension enable
    // strCmdMatchName holds the mksqlite command name
    bool cmdTryHandleEnableExtension( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }

        //assert( isValidDbid( m_dbid ) );
        
        /*
         * There should be one Argument to "enable extension"
         */
        if( m_narg > 1 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        int flagOnOff;
        if( !argGetNextInteger( flagOnOff, /*asBoolInt*/ true ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        
        Sql.current().clearErr();

        if( !Sql.current().setEnableLoadExtension( flagOnOff ) )
        {
            m_err.set( Sql.current().getErr() );
            return false;
        }

        mexPrintf( "%s\n", ::getLocaleMsg( flagOnOff ? MSG_EXTENSION_EN : MSG_EXTENSION_DIS ) );
        
        return true;
    }
    
    
    // try to interpret current command as compression setting.
    // strCmdMatchName holds the mksqlite command name
    bool cmdTryHandleCompression( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }
        
        // always return the old settings
        if(1)
        {
            mxArray* cell       = mxCreateCellMatrix( 2, 1 );
            mxArray* compressor = mxCreateString( g_compression_type ? g_compression_type : "" );
            mxArray* level      = mxCreateDoubleScalar( (double)g_compression_level );
            
            mxSetCell( cell, 0, compressor );
            mxSetCell( cell, 1, level );
            
            m_plhs[0] = cell;
        }

        if(1)
        {
            int new_compression_level = 0;
            char* new_compressor = NULL;

            if( m_narg < 2 ) 
            {
                m_err.set( MSG_MISSINGARG );
                return false;
            }
            else if( m_narg > 2 ) 
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }

            if( !mxIsChar( m_parg[0] ) || !mxIsNumeric( m_parg[1] ) )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }
            
            new_compressor        = ValueMex( m_parg[0] ).GetString();
            new_compression_level = ValueMex( m_parg[1] ).GetInt();

            if( new_compression_level < 0 || new_compression_level > 9 )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }

            if( STRMATCH( new_compressor, BLOSC_LZ4_ID ) )
            {
                g_compression_type = BLOSC_LZ4_ID;
            } 
            else if( STRMATCH( new_compressor, BLOSC_LZ4HC_ID ) )
            {
                g_compression_type = BLOSC_LZ4HC_ID;
            } 
            else if( STRMATCH( new_compressor, BLOSC_DEFAULT_ID ) )
            {
                g_compression_type = BLOSC_DEFAULT_ID;
            } 
            else if( STRMATCH( new_compressor, QLIN16_ID ) )
            {
                g_compression_type = QLIN16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else if( STRMATCH( new_compressor, QLOG16_ID ) )
            {
                g_compression_type = QLOG16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else 
            {
                m_err.set( MSG_INVALIDARG );
            }

            ::utils_free_ptr( new_compressor );
            
            if( !errPending() )
            {
                g_compression_level = new_compression_level;
            }
        }
        return true;
    }
    
    
    // try to interpret current command as status command.
    // strCmdMatchName holds the mksqlite command name.
    bool cmdTryHandleStatus( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        /*
         * There should be no Argument to status
         */
        if( m_narg > 0 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        Sql.printStati();
        
        return true;
    }
    
    
    // try to interpret current command as streaming switch.
    // strCmdMatchName holds the mksqlite command name.
    bool cmdTryHandleStreaming( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        /*
         *  Check max number of arguments
         */
        if( m_narg > 1 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        /*
         *  try to read flag
         */
        int flagOnOff = g_streaming;
        if( m_narg && !argGetNextInteger( flagOnOff, /*asBoolInt*/ true ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        if( flagOnOff && !have_serialize() )
        {
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_STREAMINGNOTSUPPORTED ) );
            flagOnOff = 0;
        }
        
        if( flagOnOff && typed_blobs_mode_check( TYBLOB_NO ) )
        {
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_STREAMINGNEEDTYBLOBS ) );
            flagOnOff = 0;
        }
        
        // always return current status
        m_plhs[0] = mxCreateDoubleScalar( (double)g_streaming );
        
        // store new value
        g_streaming = flagOnOff;
        
        return true;
    }
    
    
    // try to interpret current command as result type.
    // strCmdMatchName holds the mksqlite command name.
    bool cmdTryHandleResultType( const char* strCmdMatchName )
    {
        int new_result_type;
        int old_result_type = g_result_type;
        
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        /*
         * There should be one integer argument
         */
        if( m_narg > 1 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        if( !m_narg )
        {
            /*
             * Print current result type
             */
            mexPrintf( "%s\"%s\"\n", ::getLocaleMsg( MSG_RESULTTYPE ) );
            m_err.set( Sql.current().getErr() );
            return false;
        }
        
        if( m_narg && !argGetNextInteger( new_result_type, /*asBoolInt*/ false  ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        if( new_result_type != old_result_type )
        {
            if( new_result_type < 0 || new_result_type > RESULT_TYPE_MAX_ID )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }
        
            g_result_type = new_result_type;
        }
        
        // always return the old value
        m_plhs[0] = mxCreateDoubleScalar( (double)old_result_type );

        return true;
    }
    
    
    // try to interpret current command for busy timeout switch.
    // strCmdMatchName holds the mksqlite command name.
    bool cmdTryHandleSetBusyTimeout( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }
        
        //assert( isValidDbid( m_dbid ) );
        
        int iTimeout;
        
        /*
         * There should be one Argument, the Timeout in ms
         */
        if( m_narg > 1 ) 
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        if( !Sql.current().isOpen() )
        {
            m_err.set( MSG_DBNOTOPEN );
            return false;
        }
        
        Sql.current().clearErr();
        
        if( !m_narg && !Sql.current().getBusyTimeout( iTimeout ) )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
            m_err.set( Sql.current().getErr() );
            return false;
        }
        
        if( m_narg && !argGetNextInteger( iTimeout, /*asBoolInt*/ false  ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        Sql.current().clearErr();
        
        if( !Sql.current().setBusyTimeout( iTimeout ) )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
            m_err.set( Sql.current().getErr() );
            return false;
        }
        
        // always return old timeout value
        m_plhs[0] = mxCreateDoubleScalar( (double)iTimeout );

        return true;
    }
    
    
    /* 
     * try to interpret current argument as command or switch:
     *
     * version mex
     * version sql
     * check4uniquefields
     * convertUTF8
     * typedBLOBs
     * NULLasNaN
     * compression
     * compression_check
     * show tables
     * enable extension
     * status
     * setbusytimeout
     */
    bool cmdTryHandleNonSqlStatement()
    {
        if(    cmdTryHandleFlag( "check4uniquefields", g_check4uniquefields )
            || cmdTryHandleFlag( "convertUTF8", g_convertUTF8 )
            || cmdTryHandleFlag( "NULLasNaN", g_NULLasNaN )
            || cmdTryHandleFlag( "compression_check", g_compression_check )
            || cmdTryHandleFlag( "wrap_parameters", g_wrap_parameters )
            || cmdTryHandleStatus( "status" )
            || cmdTryHandleVersion( "version mex", "version sql" )
            || cmdTryHandleStreaming( "streaming" )
            || cmdTryHandleTypedBlob( "typedBLOBs" )
            || cmdTryHandleResultType( "result_type" )
            || cmdTryHandleCompression( "compression" )
            || cmdTryHandleSetBusyTimeout( "setbusytimeout" )
            || cmdTryHandleEnableExtension( "enable extension" ) )
        {
           return true;
        }
        else if ( STRMATCH( m_command, "show tables" ) ) 
        {
            m_query = "SELECT name as tablename FROM sqlite_master "
                      "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                      "UNION ALL "
                      "SELECT name as tablename FROM sqlite_temp_master "
                      "WHERE type IN ('table','view') "
                      "ORDER BY 1;";
            
            return false;
        }

        return false;
    }
    

    
    enum command_e { FAILED = -1, DONE, OPEN, CLOSE, QUERY };
    
    // read the command from the current argument position and test for
    // switches or other commands.
    // Switches were dispatched, other commands remain unhandled here
    command_e cmdAnalyseCommand()
    {
        if( STRMATCH( m_command, "open" ) )     return OPEN;
        if( STRMATCH( m_command, "close" ) )    return CLOSE;
        if( cmdTryHandleNonSqlStatement() )     return DONE;
        if( errPending() )                      return FAILED;
        
        return QUERY;
    }

    
    // handle the open command. If the read dbid is -1 a new slot (dbid) 
    // will be used, otherwise the given dbid or, if no dbid was given, the 
    // recent dbid is used.
    bool cmdHandleOpen()
    {
        int openFlags = 0;

        if( errPending() ) return false;
        
        /*
         * open a database. There has to be one string argument,
         * the database filename
         */
        if( !m_narg || !mxIsChar( m_parg[0] ) )
        {
            m_err.set( MSG_NOOPENARG );
            return false;
        }
        
        char* dbname = ValueMex( m_parg[0] ).GetString();
        m_parg++;
        m_narg--;

        // user missed entering a database id, so select the default (1)
        if( m_dbid < 0 )
        {
            m_dbid = 1;
        }
        
        // find a free database slot, if user entered 0
        if( !m_dbid )
        {
            int new_dbid = Sql.getNextFreeId();

            if( !Sql.isValidId( new_dbid ) )
            {
                // no free slot available
                m_err.set( MSG_NOFREESLOT );
                return false;
            }
            
            m_dbid = new_dbid + 1;  // Base 1
        }
        
        Sql.switchTo( m_dbid-1 );
        
        if( !Sql.current().closeDb() )
        {
            m_err.set( Sql.current().getErr() );
        }
        
        /*
         * Open mode (optional)
         */
        if( m_narg > 0 && !errPending() )
        {
            char* iomode = ValueMex( m_parg[0] ).GetString();
            
            m_parg++;
            m_narg--;
            
            if( STRMATCH( iomode, "ro" ) )
            {
                openFlags |= SQLITE_OPEN_READONLY;
            } 
            else if( STRMATCH( iomode, "rw" ) )
            {
                openFlags |= SQLITE_OPEN_READWRITE;
            } 
            else if( STRMATCH( iomode, "rwc" ) )
            {
                openFlags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            } 
            else 
            {
                m_err.set( MSG_ERRUNKOPENMODE );
            }
            
            ::utils_free_ptr( iomode ); 
        } 
        else 
        {
            openFlags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        }
        
        
        /*
         * Threading mode (optional)
         */
        if( m_narg > 0 && !errPending() )
        {
            char* threadmode = ValueMex( m_parg[0] ).GetString();
            
            m_parg++;
            m_narg--;
            
            if( STRMATCH( threadmode, "single" ) )
            {
                /* default */
            } 
            else if( STRMATCH( threadmode, "multi" ) )
            {
                openFlags |= SQLITE_OPEN_NOMUTEX;
            } 
            else if( STRMATCH( threadmode, "serial" ) )
            {
                openFlags |= SQLITE_OPEN_FULLMUTEX;
            } 
            else 
            {
                m_err.set( MSG_ERRUNKTHREADMODE );
            }
            
            ::utils_free_ptr( threadmode );
        } 
        
        if( !errPending() )
        {
            if( !Sql.current().openDb( dbname, openFlags ) )
            {
                m_err.set( Sql.current().getErr() );
            }
        }
        
        /*
         * Set Default Busytimeout
         */
        if( !errPending() )
        {
            if( !Sql.current().setBusyTimeout( CONFIG_BUSYTIMEOUT ) )
            {
                mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
                m_err.set( Sql.current().getErr() );
            }
        }
        
        
        /*
         * always return value will be the used database id
         */
        m_plhs[0] = mxCreateDoubleScalar( (double)m_dbid );

        ::utils_free_ptr( dbname );
        
        return !errPending();
    }
    

   /*
    * close a database.
    * handles the close command. If a dbid of 0 is given, all open dbs
    * will be closed.
    */
    bool cmdHandleClose()
    {
        if( errPending() ) return false;

        /*
         * There should be no Argument to close
         */
        if( m_narg > 0 )
        {
            m_err.set( MSG_INVALIDARG );
            return false;
        }

        if( m_dbid < 0 )
        {
            m_dbid = 1;
        }
        
        /*
         * if the database id is 0 than close all open databases
         */
        if( !m_dbid )
        {
            Sql.closeAllDbs();
        }
        else
        {
            /*
             * If the database is open, then close it. Otherwise
             * inform the user
             */
            
            Sql.switchTo( m_dbid-1 );

            if( !Sql.current().closeDb() )
            {
                m_err.set( Sql.current().getErr() );
            }
        }
        
        return errPending();
    }
    
    
    mxArray* createItemFromValueSQL( const ValueSQL& value )
    {
        mxArray* item = NULL;

        switch( value.m_typeID )
        {
          case SQLITE_NULL:
            item = mxCreateDoubleMatrix( 0, 0, mxREAL );
            break;

          case SQLITE_INTEGER:
            item = mxCreateNumericMatrix( 1, 1, mxINT64_CLASS, mxREAL );

            if(item)
            {
                *(long long*)mxGetData( item ) = value.m_integer;
            }
            break;

          case SQLITE_FLOAT:
            item = mxCreateDoubleScalar( value.m_float );
            break;

          case SQLITE_TEXT:
            item = mxCreateString( value.m_text );
            break;

          case SQLITE_BLOB:
          {
            ValueMex blob(value.m_blob);
            size_t blob_size = blob.ByData();

            if( blob_size > 0 )
            {
                if( typed_blobs_mode_check(TYBLOB_NO) )
                {
                    item = mxCreateNumericMatrix( (mwSize)blob_size, 1, mxUINT8_CLASS, mxREAL );

                    if(item)
                    {
                        memcpy( ValueMex(item).Data(), blob.Data(), blob_size );
                    }
                } 
                else 
                {
                    const void* blob         = ValueMex( value.m_blob ).Data();
                    double      process_time = 0.0;
                    double      ratio = 0.0;
                    int         err_id;

                    /* blob_unpack() modifies g_finalize_msg */
                    err_id = blob_unpack( blob, blob_size, can_serialize(), &item, &process_time, &ratio );

                    if( MSG_NOERROR != err_id )
                    {
                        m_err.set( err_id );
                    }
                }
            } 
            else 
            {
                // empty BLOB
                item = mxCreateDoubleMatrix( 0, 0, mxREAL );
            }

            break;
          } /* end case SQLITE_BLOB */

          default:
            assert(false);
            break;

        } /* end switch */
        
        return item;
    }
    
    
    mxArray* createResultColNameMatrix( const ValueSQLCols& cols )
    {
        mxArray* colNames = mxCreateCellMatrix( (int)cols.size(), (int)cols.size() ? 1 : 0 );
        
        // check if all results are of type floating point
        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            mxArray* colName = mxCreateString( cols[i].m_name.c_str() );

            if( !colNames || !colName )
            {
                utils_destroy_array( colName );
                utils_destroy_array( colNames );

                m_err.set( MSG_ERRMEMORY );
            }
            else
            {
                mxDestroyArray( mxGetCell( colNames, i ) );
                mxSetCell( colNames, i, colName );
            }
        }
        
        return colNames;
    }
    
    
    mxArray* createResultAsArrayOfStructs( ValueSQLCols& cols )
    {
        /*
         * Allocate an array of MATLAB structs to return as result
         */
        mxArray* result = mxCreateStructMatrix( (int)cols[0].size(), 1, 0, NULL );

        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            int j;

            if( !result || -1 == ( j = mxAddField( result, cols[i].m_name.c_str() ) ) )
            {
                m_err.set( MSG_ERRMEMORY );
            }

            for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
            {
                mxArray* item = createItemFromValueSQL( cols[i][row] );

                if( !item )
                {
                    m_err.set( MSG_ERRMEMORY );
                }
                else
                {
                    // destroy previous item
                    mxDestroyArray( mxGetFieldByNumber( result, row, j ) );
                    // and replace with new one
                    mxSetFieldByNumber( result, row, j, item );

                    cols[i].Destroy(row);
                    item = NULL;  // Do not destroy! (Occupied by MATLAB struct now)
                }
            } /* end for (rows) */
        } /* end for (cols) */
        
        return result;
    }
    
    
    mxArray* createResultAsStructOfArrays( ValueSQLCols& cols )
    {
        /*
         * Allocate a MATLAB struct of arrays to return as result
         */
        mxArray* result = mxCreateStructMatrix( 1, 1, 0, NULL );

        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            mxArray* column = NULL;
            int j;

            column = cols[i].m_isAnyType ?
                     mxCreateCellMatrix( (int)cols[0].size(), 1 ) :
                     mxCreateDoubleMatrix( (int)cols[0].size(), 1, mxREAL );

            if( !result || !column || -1 == ( j = mxAddField( result, cols[i].m_name.c_str() ) ) )
            {
                m_err.set( MSG_ERRMEMORY );
                utils_destroy_array( column );
            }

            if( !cols[i].m_isAnyType )
            {
                for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
                {
                    assert( cols[i][row].m_typeID == SQLITE_FLOAT );
                    mxGetPr(column)[row] = cols[i][row].m_float;
                } /* end for (rows) */
            }
            else
            {
                for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
                {
                    mxArray* item = createItemFromValueSQL( cols[i][row] );

                    if( !item )
                    {
                        m_err.set( MSG_ERRMEMORY );
                        utils_destroy_array( column );
                    }
                    else
                    {
                        // destroy previous item
                        mxDestroyArray( mxGetCell( column, row ) );
                        // and replace with new one
                        mxSetCell( column, row, item );

                        cols[i].Destroy(row);
                        item = NULL;  // Do not destroy! (Occupied by MATLAB cell array now)
                    }
                } /* end for (rows) */
            } /* end if */

            if( !errPending() )
            {
                mxSetFieldByNumber( result, 0, j, column );
                column = NULL;  // Do not destroy! (Occupied by MATLAB struct now)
            }
        } /* end for (cols) */
        
        return result;
    }
    
    
    mxArray* createResultAsMatrix( ValueSQLCols& cols )
    {
        bool allFloat = true;
        
        // check if all columns contain numeric values
        for( int i = 0; i < (int)cols.size(); i++ )
        {
            if( cols[i].m_isAnyType )
            {
                allFloat = false;
                break;
            }
        }
        
        /*
         * Allocate a MATLAB matrix or cell array to return as result
         */
        mxArray* result = allFloat ?
                 mxCreateDoubleMatrix( (int)cols[0].size(), (int)cols.size(), mxREAL ) :
                 mxCreateCellMatrix( (int)cols[0].size(), (int)cols.size() );

        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            if( !result )
            {
                m_err.set( MSG_ERRMEMORY );
                utils_destroy_array( result );
            }

            for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
            {
                if( allFloat )
                {
                    assert( cols[i][row].m_typeID == SQLITE_FLOAT );
                    double dVal = cols[i][row].m_float;

                    mxGetPr(result)[i * (int)cols[0].size() + row] = dVal;
                }
                else
                {
                    mxArray* item = createItemFromValueSQL( cols[i][row] );

                    if( !item )
                    {
                        m_err.set( MSG_ERRMEMORY );
                    }
                    else
                    {
                        // destroy previous item
                        mxDestroyArray( mxGetCell( result, i * (int)cols[i].size() + row ) );
                        // and replace with new one
                        mxSetCell( result, i * (int)cols[i].size() + row, item );

                        cols[i].Destroy(row);
                        item = NULL;  // Do not destroy! (Occupied by MATLAB cell array now)
                    }
                }
            } /* end for (rows) */
        } /* end for (cols) */
                 
        return result;
    }
    
    
    
    // handles ramaining SQL query
    bool cmdHandleSQLStatement()
    {
        if( errPending() ) return false;
        
        // if user missed entering a database id, or selected id 0
        // use default database at slot 1
        if( m_dbid <= 0 )
        {
            m_dbid = 1;  // Base 1
        }
        
        /*** Selecting database ***/

        assert( Sql.isValidId( m_dbid-1 ) );

        Sql.switchTo( m_dbid-1 );

        if( !Sql.current().isOpen() )
        {
            m_err.set( MSG_DBNOTOPEN );
            return false;
        }

        /*** prepare query, append semicolon ***/
        
        // m_query can be already set i.e. in case of command 'show tables'
        if( !m_query )
        {
            // Append a semicolon
            char* new_command = NULL;
            
            new_command = (char*)MEM_ALLOC( (int)strlen( m_command ) + 2, 1 );
            
            if( !new_command )
            {
                m_err.set( MSG_ERRMEMORY );
                return false;
            }
            
            sprintf( new_command, "%s;", m_command );
            ::utils_free_ptr( m_command );
            m_command = new_command;
            
            m_query = m_command;
        }
        
        /*** prepare statement ***/

        if( !Sql.current().setQuery( m_query ) )
        {
            m_err.set( Sql.current().getErr() );
            return false;
        }

        /*** Progress parameters for subsequent queries ***/

        ValueSQLCols    cols;
        const mxArray** nextBindParam   = m_parg;
        int             countBindParam  = m_narg;
        int             argsNeeded      = Sql.current().getParameterCount();

        // Check if a lonely cell argument is passed
        if( countBindParam == 1 && ValueMex(*nextBindParam).IsCell() )
        {
            // Only allowed, when streming is off
            if( g_streaming )
            {
                m_err.set( MSG_SINGLECELLNOTALLOWED );
                return false;
            }
            
            // redirect cell elements as bind arguments
            countBindParam = (int)ValueMex(*nextBindParam).NumElements();
            nextBindParam = (const mxArray**)ValueMex(*nextBindParam).Data();
        }

        if( g_wrap_parameters )
        {
            // exceeding argument list allowed to ommit multiple queries
            
            int count       = argsNeeded ? ( countBindParam / argsNeeded ) : 1;
            int remain      = argsNeeded ? ( countBindParam % argsNeeded ) : 0;

            // no parameters may left
            if( remain )
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }
        
            // all placeholders must be fullfilled
            if( !count )
            {
                m_err.set( MSG_MISSINGARG );
                return false;
            }
        }
        else
        {
            // the number of arguments may not exceed the number of placeholders
            // in the sql statement
            if( countBindParam > argsNeeded )
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }

            // number of arguments must match now
            if( countBindParam != argsNeeded )
            {
                m_err.set( MSG_MISSINGARGL );
                return false;
            }
        }

        // loop over parameters

        for(;;)
        {
            // reset SQL statement
            Sql.current().reset();

            /*** Bind parameters ***/
        
            // bind each argument to SQL statements placeholders 
            for( int iParam = 0; !errPending() && iParam < argsNeeded; iParam++, countBindParam-- )
            {
                if( !Sql.current().bindParameter( iParam+1, *nextBindParam++, can_serialize() ) )
                {
                    m_err.set( Sql.current().getErr() );
                    return false;
                }
            }

            /*** fetch results and store results for output ***/

            if( !Sql.current().fetch( cols ) )
            {
                m_err.set( Sql.current().getErr() );
                return false;
            }

            if( countBindParam <= 0 )
            {
                break;
            }
        }

        /*
         * end the sql engine
         */
        Sql.current().finalize();

        /*** Prepare results to return ***/
        
        // Get a cell matrix of original column names and analyze if
        mxArray* colNames = createResultColNameMatrix( cols );
        
        if( errPending() )
        {
            utils_destroy_array( colNames );
            return false;
        }
        
        // check if result is empty
        if( !cols.size() )
        {
            /*
             * got nothing? return an empty result to MATLAB
             */
            mxArray* result = mxCreateDoubleMatrix( 0, 0, mxREAL );
            if( !result )
            {
                m_err.set( MSG_CANTCREATEOUTPUT );
            }
            else
            {
                m_plhs[0] = result;
            }
        }
        else
        {
            mxArray* result = NULL;
            
            switch( g_result_type )
            {
                case RESULT_TYPE_ARRAYOFSTRUCTS:
                    result = createResultAsArrayOfStructs( cols );
                    break;
                
                case RESULT_TYPE_STRUCTOFARRAYS:
                    result = createResultAsStructOfArrays( cols );
                    break;
                
                case RESULT_TYPE_MATRIX:
                    result = createResultAsMatrix( cols );
                    break;
                
                default:
                    assert( false );
                    break;
            }

            if( !result )
            {
                m_err.set( MSG_CANTCREATEOUTPUT );
            }
            else 
            {
                m_plhs[0] = result;
            }
        }

        if( !errPending() )
        {
            // If more than 1 return parameter, output the row count 
            if( m_nlhs > 1 )
            {
                int row_count = cols.size() > 0 ? (int)cols[0].size() : 0;
                m_plhs[1] = mxCreateDoubleScalar( (double)row_count );
                assert( NULL != m_plhs[1] );
            }
                
            // If more than 2 return parameters, output the real column names
            if( m_nlhs > 2 )
            {
                m_plhs[2] = colNames;
                colNames  = NULL;
            }
        }

        utils_destroy_array( colNames );

        return !errPending();
        
    } /* end cmdHandleSQLStatement() */

};






    
    
    

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* the main routine */

/*
 * This ist the Entry Function of this Mex-DLL
 */
void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] )
{
    /*
     * Get the current Language
     */
    if( getLocale() == -1 )
    {
#ifdef _WIN32        
        switch( PRIMARYLANGID( GetUserDefaultLangID() ) )
        {
            case LANG_GERMAN:
                setLocale(1);
                break;
                
            default:
                setLocale(0);
        }
#else
        setLocale(0);
#endif
    }
    
    /*
     * Print Version Information, initializing, ...
     */
    if( !g_is_initialized )
    {
        mex_module_init();
    }
    
    Mksqlite mksqlite( nlhs, plhs, nrhs, prhs );
    
    Sql.current().clearErr();
    
    // read first numeric argument if given and use as dbid, then read command string
    if( !mksqlite.argTryReadValidDbid() || !mksqlite.argReadCommand() )
    {
        mksqlite.returnWithError();
    }
    
    // analyse command string (switch, command, or query)
    switch( mksqlite.cmdAnalyseCommand() )
    {
      case Mksqlite::OPEN:
        (void)mksqlite.cmdHandleOpen();
        break;
        
      case Mksqlite::CLOSE:
        (void)mksqlite.cmdHandleClose();
        break;
        
      case Mksqlite::QUERY:
        (void)mksqlite.cmdHandleSQLStatement();
        break;
        
      case Mksqlite::FAILED:
        break;
        
      case Mksqlite::DONE:
        break;
        
      default:
        assert( false );
    }
    
    
    if( mksqlite.errPending() )
    {
        mksqlite.returnWithError();
    }
    
    return;
}
