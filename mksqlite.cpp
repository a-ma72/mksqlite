/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      mksqlite.cpp
 *  @brief     Main routine (\ref mexFunction())
 *  @details   class implementations (SQLstack and Mksqlite)
 *  @authors   Martin Kortmann <mail@kortmann.de>, 
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.0
 *  @date      2008-2015
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

/* following define is not really used yet, since this is only one module */
/// @cond
#define MAIN_MODULE
/// @endcond

//#include "config.h"                 // Defaults
//#include "global.hpp"               // Global definitions and stati
//#include "serialize.hpp"            // Serialization of MATLAB variables (undocumented feature)
//#include "number_compressor.hpp"    // Some compressing algorithms
//#include "typed_blobs.hpp"          // Packing into typed blobs with variable type storage
//#include "utils.hpp"                // Utilities 
#include "sql_interface.hpp"        // SQLite interface
//#include "locale.hpp"               // (Error-)Messages
//#include <vector>

/// Returns 0 if \p strA and \p strB are equal (ignoring case)
#define STRMATCH(strA,strB)       ( (strA) && (strB) && ( 0 == _strcmpi( (strA), (strB) ) ) )
/// Terminates the function immediately with an error message
#define FINALIZE_STR( message )   mexErrMsgTxt( message )
/// Terminates the function immediately with an error message
#define FINALIZE( identifier )    FINALIZE_STR( ::getLocaleMsg( identifier ) )

/// @cond

// design time assertion ensures int32_t and mwSize as 4 byte data representation
HC_COMP_ASSERT( sizeof(uint32_t)==4 && sizeof(mwSize)==4 );
// Static assertion: Ensure backward compatibility
HC_COMP_ASSERT( sizeof( TypedBLOBHeaderV1 ) == 36 );

/// @endcond

/// MEX Entry function declared the as pure C
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
 * \brief SQLite interface stack
 *
 * Holds a defined number (COUNT_DB) of SQLite slots (with dbids) to 
 * facilitate usage of multiple parallel databases. Each database can
 * be accessed by its dbid (database ID) as first optional parameter to
 * mksqlite: \n
 * \code 
 * dbid_1 = mksqlite( 1, 'open', 'first.db' );    // open 1st database with dbid=1
 * dbid_2 = mksqlite( 2, 'open', 'second.db' );   // open 2nd database with dbid=2
 * assert( dbid_1 == 1 && dbid_2 == 2 );
 * query = mksqlite( 2, 'SELECT * FROM table' );  // access to 2nd database
 * mksqlite( 0, 'close' );                        // closes all databases
 * \endcode
 */
static struct SQLstack
{
    /// Stack size
    enum { COUNT_DB = CONFIG_MAX_NUM_OF_DBS };
    
    SQLiface m_db[COUNT_DB];     ///< SQLite interface slots
    int      m_dbid;             ///< selected current database id, base 0
    
    
    /// Standard Ctor (first database slot is default)
    SQLstack(): m_dbid(0) 
    {
        sqlite3_initialize();
    };
    
    
    /**
     * \brief Dtor
     *
     * Closes all databases and shuts sqlite engine down.
     */
    ~SQLstack()
    {
        (void)closeAllDbs();
        sqlite3_shutdown();
    }
    
    
    /// Checks if database \p newId is in valid range
    bool isValidId( int newId )
    {
        return newId >= 0 && newId < COUNT_DB;
    }
    
    
    /// Makes \p newId as current database id
    void switchTo( int newId )
    {
        assert( isValidId( newId ) );
        m_dbid = newId;
    }
    
    
    /// Returns the current SQL interface
    SQLiface& current()
    {
        assert( m_dbid >= 0 );
        return m_db[m_dbid];
    }
    
    
    /// Outputs current status for each database slot
    void printStati()
    {
        for( int i = 0; i < COUNT_DB; i++ )
        {
            mexPrintf( "DB Handle %d: %s\n", i+1, m_db[i].isOpen() ? "OPEN" : "CLOSED" );
        }
    }
    
    
    /// Returns the first next free id slot (base 0). Database must be closed
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
    
    
    /// Closes all open databases and returns the number of closed DBs, if any open
    int closeAllDbs()
    {
        int nClosed = 0;

        for( int i = 0; i < COUNT_DB; i++ )
        {
            if( m_db[i].isOpen() )
            {
                m_db[i].closeDb();
                nClosed++;
            }
        }
        
        return nClosed;
    }
    
} SQLstack; ///< Holding the SQLiface slots


/**
 * \brief Module deinitialization
 *
 * Closes all open databases and deinit blosc environment
 */
void mex_module_deinit()
{
    if( SQLstack.closeAllDbs() > 0 )
    {
        /*
         * inform the user, databases have been closed
         */
        mexWarnMsgTxt( ::getLocaleMsg( MSG_CLOSINGFILES ) );
    }
    
    blosc_destroy();
}


/**
 * \brief Module initialization
 *
 * Get platform information and initializes blosc.
 */
void mex_module_init()
{
    static bool init_once = false;  // only one initialization per module
    
    if( !init_once )
    {
        mxArray *plhs[3] = {0};

        if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
        {
            g_compression_type = BLOSC_DEFAULT_ID;
            blosc_init();
            mexAtExit( mex_module_deinit );
            typed_blobs_init();

            mexPrintf( ::getLocaleMsg( MSG_HELLO ), 
                       SQLITE_VERSION );

            mexPrintf( "Platform: %s, %s\n\n", 
                       TBH_platform, 
                       TBH_endian[0] == 'L' ? "little endian" : "big endian" );

            init_once = true;
        }
        else
        {
            FINALIZE( MSG_ERRPLATFORMDETECT );
        }

        if( 0 == mexCallMATLAB( 1, plhs, 0, NULL, "namelengthmax" ) )
        {
            g_namelengthmax = (int)mxGetScalar( plhs[0] );
        }

#if CONFIG_USE_HEAP_CHECK
        mexPrintf( "Heap checking is on, this may slow down execution time dramatically!\n" );
#endif
        
    }
}






/**
 * \brief Main routine class
 *
 */
class Mksqlite
{
    int               m_nlhs;       ///< count of left hand side arguments
    int               m_narg;       ///< count of right hand side arguments
    mxArray**         m_plhs;       ///< pointer to current left hand side argument
    const mxArray**   m_parg;       ///< pointer to current right hand side argument
    char*             m_command;    ///< SQL command. Allocated and freed by this class
    const char*       m_query;      ///< \p m_command, or a translation from \p m_command
    int               m_dbid_req;   ///< requested database id (user input) -1="arg missing", 0="next free slot" or 1..COUNT_DB
    int               m_dbid;       ///< selected database slot (1..COUNT_DB)
    Err               m_err;        ///< recent error
    
    /**
     * \name Inhibit assignment, default and copy ctors
     * @{ */
    Mksqlite();
    Mksqlite( const Mksqlite& );
    Mksqlite& operator=( const Mksqlite& );
    /** @} */
    
public:
    /// Standard ctor
    Mksqlite( int nlhs, mxArray** plhs, int nrhs, const mxArray** prhs )
    : m_nlhs( nlhs ), m_plhs( plhs ), 
      m_narg( nrhs ), m_parg( prhs ),
      m_command(NULL), m_query(NULL), m_dbid_req(-1), m_dbid(1)
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
    
    
    /// Release object
    void Release()
    {
        if( m_command )
        {
            ::utils_free_ptr( m_command );
        }
    }
    
    
    /// Dtor
    ~Mksqlite()
    {
        Release();
    }
    
    /// Returns true, if any error is pending
    bool errPending()
    {
        return m_err.isPending();
    }
    
    
    /// Clear recent error
    void errClear()
    {
        m_err.clear();
    }
    
    
    /**
     * \brief Termine function 
     * 
     * Aborts the running function with an error message. Allocated memory 
     * (by MATLAB allocation functions) is freed automatically
     */
    void returnWithError()
    {
        const char *errId  = NULL;
        const char *errMsg = m_err.get( &errId );
        
        assert( errPending() );
        
        mexErrMsgIdAndTxt( errId ? errId : "MKSQLITE:ANY", errMsg );
    }
    
    
    /**
     * \brief Ensuring current database is open
     *
     * Sets \p m_err to MSG_DBNOTOPEN, if not.
     *
     * \returns true if database is open
     */
    bool ensureDbIsOpen()
    {
        // database must be opened to set busy timeout
        if( !SQLstack.current().isOpen() )
        {
            m_err.set( MSG_DBNOTOPEN );
            return false;
        }
        
        return true;
    }
    
    
    /**
     * \brief Omits a warning if database is given but superfluous
     *
     * \returns true if dbid is undefined
     */
    bool warnOnDefDbid()
    {
        if( m_dbid_req != -1 )
        {
            mexWarnMsgTxt( ::getLocaleMsg( MSG_DBID_SUPFLOUS ) );
            return false;
        }
        
        return true;
    }
    
    
    /**
     * \brief Get next integer from argument list
     *
     * \param[out] refValue Result will be returned in
     * \param[in] asBoolInt If true, \p refValue will be true (1) or false (0) only
     * 
     * Read an integer parameter at current argument read position, and
     * write to \p refValue (as 0 or 1 if \p asBoolInt is set to true)
     */
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

    
    /**
     * \brief Get database ID from argument list
     * 
     * Reads the next argument if it is numeric and a valid dbid.
     * dbid must be in range 0..\p CONFIG_MAX_NUM_OF_DBS, where 0 has the special
     * meaning that the first free slot will later be used.\n
     * If the parameter is missing, \p m_dbid_req will be set to -1. \p m_dbid will
     * be set to either -1 (argument missing) or a valid dbid 1..\p CONFIG_MAX_NUM_OF_DBS
     */
    bool argTryReadValidDbid()
    {
        if( errPending() ) return false;

        /*
         * Check if the first argument is a number (base 1), then we have to use
         * this number as the requested database id. A number of 0 is allowed an leads
         * to find the first free slot.
         */
        if( argGetNextInteger( m_dbid_req, /*asBoolInt*/ false ) )
        {
            if( !SQLstack.isValidId( m_dbid_req-1 ) && m_dbid_req != 0 )
            {
                m_err.set( MSG_INVALIDDBHANDLE );
                return false;
            }
        } else {
            m_dbid_req = -1;  // argument is missing
        }
        
        // find a free database slot, if user entered 0
        if( !m_dbid_req )
        {
            int new_dbid = SQLstack.getNextFreeId();

            if( !SQLstack.isValidId( new_dbid ) )
            {
                // no free slot available
                m_err.set( MSG_NOFREESLOT );
                return false;
            }
            
            m_dbid = new_dbid + 1;  // Base 1
        } else {
            // select database id or default (1) if no one is given 
            m_dbid = ( m_dbid_req < 0 ) ? 1 : m_dbid_req;
        }

        SQLstack.switchTo( m_dbid-1 );  // base 0
        SQLstack.current().clearErr();
        m_err.clear();
        return true;
    }
    
    
    /**
     * \brief Get command from argument list
     * 
     * Read the command from current argument position, always a string and thus 
     * asserted.
     */
    bool argReadCommand()
    {
        if( errPending() ) return false;
        
        /*
         * The next (or first if no db number available) is the m_command,
         * it has to be a string.
         * This fails also, if the first arg is a dbid and there is no 
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
    
    
    /**
     * \brief Handle flag from command
     *
     * \param[in] strMatchFlagName Name of flag to test
     * \param[out] refFlag Flag value if name matched
     * \returns true, if flag could be assigned
     * 
     * Test current command as flag parameter with it's new value.
     * \p strMatchFlagName holds the name of the flag to test.
     */
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
    

    /**
     * \brief Handle version commands
     *
     * \param[in] strCmdMatchVerMex Command name to get mex version
     * \param[in] strCmdMatchVerSql Command name to get SQLite version
     * \returns true on success
     * 
     * Test current command as version query to sqlite or mksqlite version numbers.
     * \p strCmdMatchVerMex and \p strCmdMatchVerSql hold the mksqlite command names.
     * m_plhs[0] will be set to the corresponding version string.
     */
    bool cmdTryHandleVersion( const char* strCmdMatchVerMex, const char* strCmdMatchVerSql )
    {
        if( errPending() ) return false;
        
        if( STRMATCH( m_command, strCmdMatchVerMex ) )
        {
            // Global command, dbid useless
            warnOnDefDbid();

            if( m_narg > 0 ) 
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }
            else
            {
                if( m_nlhs == 0 )
                {
                    mexPrintf( "mksqlite Version %s\n", CONFIG_MKSQLITE_VERSION_STRING );
                } 
                else
                {
                    m_plhs[0] = mxCreateString( CONFIG_MKSQLITE_VERSION_STRING );
                }
            }
            return true;
        } 
        else if( STRMATCH( m_command, strCmdMatchVerSql ) )
        {
            // Global command, dbid useless
            warnOnDefDbid();

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
    
    
    /**
     * \brief Handle typed BLOB settings command 
     * 
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as blob mode setting
     * \p strCmdMatchName hold the mksqlite command name.
     * m_plhs[0] will be set to the old setting.
     */
    bool cmdTryHandleTypedBlob( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }
        
        /*
         * typedBLOBs setting:
         *  0 --> no typed blobs, streaming off
         *  1 --> typed blobs,    streaming off
         *  2 --> typed blobs,    streaming on
         *
         * Streaming is only valid if typed blobs are enabled because 
         * one could not distinguish between byte arrays and a 
         * streamed MATLAB array.
         */
        
        // Global command, dbid useless
        warnOnDefDbid();

        int old_mode = typed_blobs_mode_on();
        
        if( old_mode && g_streaming )
        {
            old_mode = 2;
        }
        
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

        // action only if something changed
        if( new_mode != old_mode )
        {
            if( new_mode < 0 || new_mode > 2 )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }

            typed_blobs_mode_set( new_mode > 0 );
        
            g_streaming = (new_mode == 2);
        }

        // always return the old value
        m_plhs[0] = mxCreateDoubleScalar( (double)old_mode );

        return true;
    }
    
    
    /**
     * \brief Handle command to (en-/dis-)able loading extensions
     *
     * \param[in] strCmdMatchName Command name
     * 
     * Try to interpret current command as setting for sqlite extension enable
     * \p strCmdMatchName holds the mksqlite command name.
     */
    bool cmdTryHandleEnableExtension( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }

        // database must be open to change setting
        if( !ensureDbIsOpen() )
        {
            // ensureDbIsOpen() sets m_err
            return false;
        }
        
        /*
         * There should be one argument to "enable extension"
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
        
        if( !SQLstack.current().setEnableLoadExtension( flagOnOff ) )
        {
            const char* errid = NULL;
            m_err.set( SQLstack.current().getErr(&errid), errid );
            return false;
        }

        mexPrintf( "%s\n", ::getLocaleMsg( flagOnOff ? MSG_EXTENSION_EN : MSG_EXTENSION_DIS ) );
        
        return true;
    }
    
    
    /**
     * \brief Handle compression setting command
     *
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as compression setting.
     * \p strCmdMatchName holds the mksqlite command name.
     * m_plhs[0] will be set to the old setting.
     */
    bool cmdTryHandleCompression( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }
        
        // Global command, dbid useless
        warnOnDefDbid();
        
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
            
            // Get new compressor setting
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
    
    
    /**
     * \brief Handle status command
     *
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as status command.
     * \p strCmdMatchName holds the mksqlite command name.
     */
    bool cmdTryHandleStatus( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        // Global command, dbid useless
        warnOnDefDbid();
        
        /*
         * There should be no argument to status
         */
        if( m_narg > 0 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        SQLstack.printStati();
        
        return true;
    }
    
    
    /**
     * \brief Handle language command
     *
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as status command.
     * \p strCmdMatchName holds the mksqlite command name.
     */
    bool cmdTryHandleLanguage( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        // Global command, dbid useless
        warnOnDefDbid();
        
        /*
         * There should be one numeric argument
         */
        if( m_narg < 1 ) 
        {
            m_err.set( MSG_MISSINGARG );
            return false;
        }
        else if( m_narg > 1 ) 
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }

        if( !mxIsNumeric( m_parg[0] ) )
        {
            m_err.set( MSG_NUMARGEXPCT );
            return false;
        } else {
            int iLang = ValueMex( m_parg[0] ).GetInt();
            
            if( !setLocale( iLang ) )
            {
                m_err.set( MSG_INVALIDARG );
                return false;
            }
        }

        return true;
    }
    
    
    /**
     * \brief Handle streaming setting command
     *
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as streaming switch.
     * \p strCmdMatchName holds the mksqlite command name.
     * m_plhs[0] will be set to the old setting.
     */
    bool cmdTryHandleStreaming( const char* strCmdMatchName )
    {
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        // Global command, dbid useless
        warnOnDefDbid();

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
        
        // Report, if serialization is not possible (reset flag then)
        if( flagOnOff && !have_serialize() )
        {
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_STREAMINGNOTSUPPORTED ) );
            flagOnOff = 0;
        }
        
        // Report, if user tries to use streaming with blobs turned off (reset flag then)
        if( flagOnOff && !typed_blobs_mode_on() )
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
    
    
    /**
     * \brief Handle result type command
     *
     * \param[in] strCmdMatchName Command name
     * \returns true on success
     * 
     * Try to interpret current command as result type.
     * \p strCmdMatchName holds the mksqlite command name.
     * m_plhs[0] will be set to the old setting.
     */
    bool cmdTryHandleResultType( const char* strCmdMatchName )
    {
        int new_result_type;
        int old_result_type = g_result_type;
        
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) ) 
        {
            return false;
        }
        
        // Global command, dbid useless
        warnOnDefDbid();

        /*
         * There should be one integer argument
         */
        if( m_narg > 1 )
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        // No arguments, then report current state
        if( !m_narg )
        {
            const char* errid = NULL;
            /*
             * Print current result type
             */
            mexPrintf( "%s\"%s\"\n", ::getLocaleMsg( MSG_RESULTTYPE ) );
            m_err.set( SQLstack.current().getErr(&errid), errid );
            return false;
        }
        
        // next parameter must be on/off flag
        if( m_narg && !argGetNextInteger( new_result_type, /*asBoolInt*/ false  ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        // action on change only
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
    
    
    /**
     * \brief Handle set busy timeout command
     *
     * \param[in] strCmdMatchName Command name
     * 
     * Try to interpret current command as busy timeout switch.
     * \p strCmdMatchName holds the mksqlite command name.
     * m_plhs[0] will be set to the old setting.
     */
    bool cmdTryHandleSetBusyTimeout( const char* strCmdMatchName )
    {
        int iTimeout;
        
        if( errPending() || !STRMATCH( m_command, strCmdMatchName ) )
        {
            return false;
        }
        
        // database must be open to set busy timeout
        if( !ensureDbIsOpen() )
        {
            // ensureDbIsOpen() sets m_err
            return false;
        }
        
        /*
         * There should be one argument, the Timeout in ms
         */
        if( m_narg > 1 ) 
        {
            m_err.set( MSG_UNEXPECTEDARG );
            return false;
        }
        
        if( !m_narg && !SQLstack.current().getBusyTimeout( iTimeout ) )
        {
            const char* errid = NULL;
            /*
             * Anything wrong? free the database id and inform the user
             */
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
            m_err.set( SQLstack.current().getErr(&errid), errid );
            return false;
        }
        
        if( m_narg && !argGetNextInteger( iTimeout, /*asBoolInt*/ false  ) )
        {
            // argGetNextInteger() sets m_err
            return false;
        }
        
        // Note that negative timeout values are allowed:
        // "Calling this routine with an argument less than or equal 
        // to zero turns off all busy handlers."
        
        if( !SQLstack.current().setBusyTimeout( iTimeout ) )
        {
            const char* errid = NULL;
            /*
             * Anything wrong? free the database id and inform the user
             */
            mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
            m_err.set( SQLstack.current().getErr(&errid), errid );
            return false;
        }
        
        // always return old timeout value
        m_plhs[0] = mxCreateDoubleScalar( (double)iTimeout );

        return true;
    }
    
    
    /**
     * \brief Interpret current argument as command or switch
     *
     * Checking following commands:
     * - version mex
     * - version sql
     * - check4uniquefields
     * - convertUTF8
     * - typedBLOBs
     * - NULLasNaN
     * - param_wrapping
     * - streaming
     * - result_type
     * - compression
     * - compression_check
     * - show tables
     * - enable extension
     * - status
     * - setbusytimeout
     */
    bool cmdTryHandleNonSqlStatement()
    {
        if(    cmdTryHandleFlag( "check4uniquefields", g_check4uniquefields )
            || cmdTryHandleFlag( "convertUTF8", g_convertUTF8 )
            || cmdTryHandleFlag( "NULLasNaN", g_NULLasNaN )
            || cmdTryHandleFlag( "compression_check", g_compression_check )
            || cmdTryHandleFlag( "param_wrapping", g_param_wrapping )
            || cmdTryHandleStatus( "status" )
            || cmdTryHandleLanguage( "lang" )
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
            
            return false;  // dispatch unhandled
        }

        return false;
    }
    

    /// Return values of cmdAnalyseCommand()
    enum command_e { FAILED = -1, DONE, OPEN, CLOSE, QUERY };
    
    /**
     * \brief Analyse command string and process if its neither open, close nor a sql command. 
     *
     * \returns Status (see command_e)
     * 
     * Reads the command from the current argument position and test for
     * switches or other commands.
     * Switches were dispatched, other commands remain unhandled here.
     */
    command_e cmdAnalyseCommand()
    {
        if( STRMATCH( m_command, "open" ) )     return OPEN;
        if( STRMATCH( m_command, "close" ) )    return CLOSE;
        if( cmdTryHandleNonSqlStatement() )     return DONE;
        if( errPending() )                      return FAILED;
        
        return QUERY;
    }

    
    /**
     * \brief Handle open command
     *
     * Handle the open command. If the read dbid is -1 a new slot (dbid) 
     * will be used, otherwise the given dbid or, if no dbid was given, the 
     * recent dbid is used.
     */
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

        // close database if open
        if( !SQLstack.current().closeDb() )
        {
            const char* errid = NULL;
            m_err.set( SQLstack.current().getErr(&errid), errid );
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
            if( !SQLstack.current().openDb( dbname, openFlags ) )
            {
                // adopt sql error message
                const char* errid = NULL;
                m_err.set( SQLstack.current().getErr(&errid), errid );
            }
        }
        
        /*
         * Set default busytimeout
         */
        if( !errPending() )
        {
            const char* errid = NULL;
            
            if( !SQLstack.current().setBusyTimeout( CONFIG_BUSYTIMEOUT ) )
            {
                mexPrintf( "%s\n", ::getLocaleMsg( MSG_BUSYTIMEOUTFAIL ) );
                m_err.set( SQLstack.current().getErr(&errid), errid );
            }
        }
        
        
        /*
         * always return the used database id
         */
        m_plhs[0] = mxCreateDoubleScalar( (double)m_dbid );

        ::utils_free_ptr( dbname );
        
        return !errPending();
    }
    

   /**
    * \brief Handle close command
    * \returns true if no error occured
    *
    * Closes a database.
    * If a dbid of 0 is given, all open dbs
    * will be closed.
    */
    bool cmdHandleClose()
    {
        if( errPending() ) return false;

        /*
         * There should be no argument to close
         */
        if( m_narg > 0 )
        {
            m_err.set( MSG_INVALIDARG );
            return false;
        }

        /*
         * if the database id is 0 than close all open databases
         */
        if( !m_dbid_req )
        {
            SQLstack.closeAllDbs();
        }
        else
        {
            /*
             * If the database is open, then close it. Otherwise
             * inform the user
             */
            
            if( !SQLstack.current().closeDb() )
            {
                // adopt sql error message
                const char* errid = NULL;
                m_err.set( SQLstack.current().getErr(&errid), errid );
            }
        }
        
        return errPending();
    }
    
    
    /**
     * \brief Transfer SQL value to MATLAB array
     *
     * @param[in] value encapsulated SQL field value
     * @returns a MATLAB array due to value type (string or numeric content)
     *
     * @see g_result_type
     */
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
                *(sqlite3_int64*)mxGetData( item ) = value.m_integer;
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
                // check if typed BLOBs are disabled
                if( !typed_blobs_mode_on() )
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
    
    
    /**
     * \brief Create a MATLAB cell array of column names
     *
     * @param[in] cols container for SQLite fetched table
     * @returns a MATLAB cell array with 2 columns. First column holds
     *  the original SQLite field names and the 2nd column holds the MATLAB
     *  struct fieldnames.
     *
     * @see g_result_type
     */
    mxArray* createResultColNameMatrix( const ValueSQLCols& cols )
    {
        mxArray* colNames = mxCreateCellMatrix( (int)cols.size(), (int)cols.size() ? 2 : 0 );
        
        // iterate columns
        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            // get the real column name
            mxArray* colNameSql = mxCreateString( cols[i].m_col_name.c_str() );
            mxArray* colNameMat = mxCreateString( cols[i].m_name.c_str() );

            if( !colNames || !colNameSql || !colNameMat )
            {
                ::utils_destroy_array( colNameSql );
                ::utils_destroy_array( colNameMat );
                ::utils_destroy_array( colNames );

                m_err.set( MSG_ERRMEMORY );
            }
            else
            {
                // replace cell contents
                int j = (int)cols.size() + i;  // 2nd cell matrix column
                mxDestroyArray( mxGetCell( colNames, i ) );
                mxDestroyArray( mxGetCell( colNames, j ) );
                mxSetCell( colNames, i, colNameSql );
                mxSetCell( colNames, j, colNameMat );
            }
        }
        
        return colNames;
    }
    
    
    /**
     * \brief Transform SQL fetch to MATLAB array of structs
     *
     * @param[in] cols container for SQLite fetched table
     * @returns a MATLAB array of structs. The array size is the row count
     *  of the table, that \a cols holds. The struct field names are the column
     *  names, and may be modified due to MATLAB naming conventions.
     *
     * @see g_result_type
     */
    mxArray* createResultAsArrayOfStructs( ValueSQLCols& cols )
    {
        /*
         * Allocate an array of MATLAB structs to return as result
         */
        mxArray* result = mxCreateStructMatrix( (int)cols[0].size(), 1, 0, NULL );

        // iterate columns
        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            int j;

            // insert new field into struct
            if( !result || -1 == ( j = mxAddField( result, cols[i].m_name.c_str() ) ) )
            {
                m_err.set( MSG_ERRMEMORY );
            }

            // iterate rows
            for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
            {
                // get current table element at row and column
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

                    cols[i].Destroy(row); // release memory
                    item = NULL;  // Do not destroy! (Occupied by MATLAB struct now)
                }
            } /* end for (rows) */
        } /* end for (cols) */
        
        return result;
    }
    
    
    /**
     * \brief Transform SQL fetch to MATLAB struct of arrays
     *
     * @param[in] cols container for SQLite fetched table
     * @returns a MATLAB struct with arrays. The array size is the row count
     *  of the table, that \a cols holds. The struct field names are the column
     *  names, and may be modified due to MATLAB naming conventions. Pure
     *  numeric vectors are given as double arrays, other types will be 
     *  returned as cell array.
     *
     * @see g_result_type
     */
    mxArray* createResultAsStructOfArrays( ValueSQLCols& cols )
    {
        /*
         * Allocate a MATLAB struct of arrays to return as result
         */
        mxArray* result = mxCreateStructMatrix( 1, 1, 0, NULL );

        // iterate columns
        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            mxArray* column = NULL;
            int j;

            // Pure floating point can be archieved in a numeric matrix
            // mixed types must be stored in a cell matrix
            column = cols[i].m_isAnyType ?
                     mxCreateCellMatrix( (int)cols[0].size(), 1 ) :
                     mxCreateDoubleMatrix( (int)cols[0].size(), 1, mxREAL );

            // add a new field in the struct
            if( !result || !column || -1 == ( j = mxAddField( result, cols[i].m_name.c_str() ) ) )
            {
                m_err.set( MSG_ERRMEMORY );
                ::utils_destroy_array( column );
            }

            if( !cols[i].m_isAnyType )
            {
                // fast copy of pure floating point data, iterating rows
                for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
                {
                    assert( cols[i][row].m_typeID == SQLITE_FLOAT );
                    mxGetPr(column)[row] = cols[i][row].m_float;
                } /* end for (rows) */
            }
            else
            {
                // build cell array, iterating rows
                for( int row = 0; !errPending() && row < (int)cols[i].size(); row++ )
                {
                    mxArray* item = createItemFromValueSQL( cols[i][row] );

                    if( !item )
                    {
                        m_err.set( MSG_ERRMEMORY );
                        ::utils_destroy_array( column );
                    }
                    else
                    {
                        // destroy previous item
                        mxDestroyArray( mxGetCell( column, row ) );
                        // and replace with new one
                        mxSetCell( column, row, item );

                        cols[i].Destroy(row);  // release memory
                        item = NULL;  // Do not destroy! (Occupied by MATLAB cell array now)
                    }
                } /* end for (rows) */
            } /* end if */

            if( !errPending() )
            {
                // assign columns data to struct field
                mxSetFieldByNumber( result, 0, j, column );
                column = NULL;  // Do not destroy! (Occupied by MATLAB struct now)
            }
        } /* end for (cols) */
        
        return result;
    }
    
    
    /**
     * \brief Transform SQL fetch to MATLAB (cell) array
     *
     * @param[in] cols SQLite fetched table
     * @returns a MATLAB cell array. The array is organized as MxN matrix,
     *  where M is the row count of the table, that \a cols holds and N 
     *  is its column count. 
     *
     * @see g_result_type
     */
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

        // iterate columns
        for( int i = 0; !errPending() && i < (int)cols.size(); i++ )
        {
            if( !result )
            {
                m_err.set( MSG_ERRMEMORY );
                ::utils_destroy_array( result );
            }

            // iterate rows
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

                        cols[i].Destroy(row);  // release memory
                        item = NULL;  // Do not destroy! (Occupied by MATLAB cell array now)
                    }
                }
            } /* end for (rows) */
        } /* end for (cols) */
                 
        return result;
    }
    
    
    /**
     * \brief Handle common SQL statement
     *
     * @returns true when SQLite accepted and proceeded the command.
     *
     * The mksqlite command string will be delegated to the SQLite engine.
     */
    bool cmdHandleSQLStatement()
    {
        if( errPending() ) return false;
        
        /*** Selecting database ***/

        if( !ensureDbIsOpen() )
        {
            // ensureDbIsOpen() sets m_err
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

        if( !SQLstack.current().setQuery( m_query ) )
        {
            const char* errid = NULL;
            m_err.set( SQLstack.current().getErr(&errid), errid );
            return false;
        }

        /*** Progress parameters for subsequent queries ***/

        ValueSQLCols    cols;
        const mxArray** nextBindParam   = m_parg;
        int             countBindParam  = m_narg;
        int             argsNeeded      = SQLstack.current().getParameterCount();

        // Check if a lonely cell argument is passed
        if( countBindParam == 1 && ValueMex(*nextBindParam).IsCell() )
        {
            // Only allowed, when streaming is off
            if( g_streaming )
            {
                m_err.set( MSG_SINGLECELLNOTALLOWED );
                return false;
            }
            
            // redirect cell elements as bind arguments
            countBindParam = (int)ValueMex(*nextBindParam).NumElements();
            nextBindParam = (const mxArray**)ValueMex(*nextBindParam).Data();
        }

        /* 
         * If g_param_wrapping is set, more parameters as needed with current
         * statement may be passed. 
         */
        
        if( g_param_wrapping )
        {
            // exceeding argument list allowed to omit multiple queries
            
            int count       = argsNeeded ? ( countBindParam / argsNeeded ) : 1;  // amount of proposed queries
            int remain      = argsNeeded ? ( countBindParam % argsNeeded ) : 0;  // must be 0

            // remain must be 0, all placeholders must be fulfilled
            if( remain || !count )
            {
                m_err.set( MSG_MISSINGARG );
                return false;
            }
        }
        else
        {
            bool flagIgnoreLessParameters = true;
            
            // the number of arguments may not exceed the number of placeholders
            // in the sql statement
            if( countBindParam > argsNeeded )
            {
                m_err.set( MSG_UNEXPECTEDARG );
                return false;
            }

            // number of arguments must match now
            if( !flagIgnoreLessParameters && countBindParam != argsNeeded )
            {
                m_err.set( MSG_MISSINGARGL );
                return false;
            }
        }

        // loop over parameters
        for(;;)
        {
            // reset SQL statement
            SQLstack.current().reset();

            /*** Bind parameters ***/
        
            // bind each argument to SQL statements placeholders 
            for( int iParam = 0; !errPending() && iParam < argsNeeded && countBindParam; iParam++, countBindParam-- )
            {
                if( !SQLstack.current().bindParameter( iParam+1, *nextBindParam++, can_serialize() ) )
                {
                    const char* errid = NULL;
                    m_err.set( SQLstack.current().getErr(&errid), errid );
                    return false;
                }
            }

            /*** fetch results and store results for output ***/

            // cumulate in "cols"
            if( !errPending() && !SQLstack.current().fetch( cols ) )
            {
                const char* errid = NULL;
                m_err.set( SQLstack.current().getErr(&errid), errid );
                return false;
            }

            if( countBindParam <= 0 )
            {
                break;
            }
        }

        /*
         * finalize current sql statement
         */
        SQLstack.current().finalize();

        /*** Prepare results to return ***/
        
        if( errPending() )
        {
            return false;
        }
        
        // check if result is empty (no columns)
        if( !cols.size() )
        {
            /*
             * got nothing? return an empty result to MATLAB
             */
            for( int i = 0; i < m_nlhs; i++ )
            {
                mxArray* result = mxCreateDoubleMatrix( 0, 0, mxREAL );
                if( !result )
                {
                    m_err.set( MSG_CANTCREATEOUTPUT );
                    break;
                }
                else
                {
                    m_plhs[i] = result;
                }
            }
        }
        else
        {
            mxArray* result = NULL;
            
            // dispatch regarding result type
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
                // Get a cell matrix of column name relations (SQL <-> MATLAB)
                m_plhs[2] = createResultColNameMatrix( cols );
            }
        }

        return !errPending();
        
    } /* end cmdHandleSQLStatement() */

};






    
    
    

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* the main routine */

/**
 * \brief Entry Function of Mex-DLL
 *
 * \param[in] nlhs Number of "left hand side" parameters (expected results)
 * \param[out] plhs Pointer to "left hand side" parameters
 * \param[in] nrhs Number of "right hand side" parameters (arguments)
 * \param[in] prhs Pomter to "right hand side" parameters
 */
void mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray*prhs[] )
{
    /*
     * Get the current language
     * -1 means "undefined" (only one init on module load required)
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
     * Print version information, initializing, ...
     */
    mex_module_init();  // only done once
    
    Mksqlite mksqlite( nlhs, plhs, nrhs, prhs );
    
    // read first numeric argument if given and use it as dbid, then read command string
    if( !mksqlite.argTryReadValidDbid() || !mksqlite.argReadCommand() )
    {
        mksqlite.returnWithError();
    }
    
    // analyse command string (switch, command, or query)
    switch( mksqlite.cmdAnalyseCommand() )
    {
      case Mksqlite::OPEN:
        (void)mksqlite.cmdHandleOpen(); // "open" command
        break;
        
      case Mksqlite::CLOSE:
        (void)mksqlite.cmdHandleClose(); // "close" command
        break;
        
      case Mksqlite::QUERY:
        (void)mksqlite.cmdHandleSQLStatement();  // common sql query
        break;
        
      case Mksqlite::FAILED:
        break;
        
      case Mksqlite::DONE:
        break; // switches (flags) are already handled
        
      default:
        assert( false );
    }
    
    
    if( mksqlite.errPending() )
    {
        mksqlite.returnWithError();
    }

#if CONFIG_USE_HEAP_CHECK
    mksqlite.Release();    // antedate destructors work
    HeapCheck.Walk();      // Report, if any leaks exist
#endif
    
    return;
}
