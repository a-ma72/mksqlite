/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      sql_interface.hpp
 *  @brief     SQLite interface class
 *  @details   SQLite accessing functions, for single-file databases
 *  @see       http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data
 *  @authors   Martin Kortmann <mail@kortmann.de>,
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.9
 *  @date      2008-2020
 *  @copyright Distributed under BSD-2
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

//#include "config.h"
//#include "global.hpp"
//#include "sqlite/sqlite3.h"
#include "sql_builtin_functions.hpp"
//#include "utils.hpp"
//#include "value.hpp"
//#include "locale.hpp"
#include <map>

// Handling Ctrl+C functions, see also http://undocumentedmatlab.com/blog/mex-ctrl-c-interrupt
extern "C" bool utIsInterruptPending();
extern "C" bool utSetInterruptEnabled( bool );
extern "C" bool utSetInterruptHandled( bool );

/// type for column container
typedef vector<ValueSQLCol> ValueSQLCols;

extern ValueMex createItemFromValueSQL( const ValueSQL& value, int& err_id );  /* mksqlite.cpp */
extern ValueSQL createValueSQLFromItem( const ValueMex& item, bool bStreamable, int& iTypeComplexity, int& err_id );  /* mksqlite.cpp */

class SQLstack;
class SQLiface;
class MexFunctors;


/// Class holding an error
class SQLerror : public Err
{
public:
    /// Set error by its result code
    void setSqlError( sqlite3* dbid, int rc )
    {
        if( SQLITE_OK == rc )
        {
            clear();
        }
        else
        {
            if( rc < 0 )
            {
                rc = sqlite3_extended_errcode( dbid );
            }
            set_printf( sqlite3_errmsg( dbid ), trans_err_to_ident( rc ) );
        }
    }

    /**
     * \brief Get least SQLite error code and return identifier as string
     */
    const char* trans_err_to_ident( int errorcode )
    {
        static char dummy[32];

        //int errorcode = sqlite3_extended_errcode( m_db );

        switch( errorcode )
        {    
            case SQLITE_OK:                         return( "SQLITE:OK" );
            case SQLITE_ERROR:                      return( "SQLITE:ERROR" );
            case SQLITE_INTERNAL:                   return( "SQLITE:INTERNAL" );
            case SQLITE_PERM:                       return( "SQLITE:PERM" );
            case SQLITE_ABORT:                      return( "SQLITE:ABORT" );
            case SQLITE_BUSY:                       return( "SQLITE:BUSY" );
            case SQLITE_LOCKED:                     return( "SQLITE:LOCKED" );
            case SQLITE_NOMEM:                      return( "SQLITE:NOMEM" );
            case SQLITE_READONLY:                   return( "SQLITE:READONLY" );
            case SQLITE_INTERRUPT:                  return( "SQLITE:INTERRUPT" );
            case SQLITE_IOERR:                      return( "SQLITE:IOERR" );
            case SQLITE_CORRUPT:                    return( "SQLITE:CORRUPT" );
            case SQLITE_NOTFOUND:                   return( "SQLITE:NOTFOUND" );
            case SQLITE_FULL:                       return( "SQLITE:FULL" );
            case SQLITE_CANTOPEN:                   return( "SQLITE:CANTOPEN" );
            case SQLITE_PROTOCOL:                   return( "SQLITE:PROTOCOL" );
            case SQLITE_EMPTY:                      return( "SQLITE:EMPTY" );
            case SQLITE_SCHEMA:                     return( "SQLITE:SCHEMA" );
            case SQLITE_TOOBIG:                     return( "SQLITE:TOOBIG" );
            case SQLITE_CONSTRAINT:                 return( "SQLITE:CONSTRAINT" );
            case SQLITE_MISMATCH:                   return( "SQLITE:MISMATCH" );
            case SQLITE_MISUSE:                     return( "SQLITE:MISUSE" );
            case SQLITE_NOLFS:                      return( "SQLITE:NOLFS" );
            case SQLITE_AUTH:                       return( "SQLITE:AUTH" );
            case SQLITE_FORMAT:                     return( "SQLITE:FORMAT" );
            case SQLITE_RANGE:                      return( "SQLITE:RANGE" );
            case SQLITE_NOTADB:                     return( "SQLITE:NOTADB" );
            case SQLITE_NOTICE:                     return( "SQLITE:NOTICE" );
            case SQLITE_WARNING:                    return( "SQLITE:WARNING" );
            case SQLITE_ROW:                        return( "SQLITE:ROW" );
            case SQLITE_DONE:                       return( "SQLITE:DONE" );
            /* extended codes */
            case SQLITE_IOERR_READ:                 return( "SQLITE:IOERR_READ" );
            case SQLITE_IOERR_SHORT_READ:           return( "SQLITE:IOERR_SHORT_READ" );
            case SQLITE_IOERR_WRITE:                return( "SQLITE:IOERR_WRITE" );
            case SQLITE_IOERR_FSYNC:                return( "SQLITE:IOERR_FSYNC" );
            case SQLITE_IOERR_DIR_FSYNC:            return( "SQLITE:IOERR_DIR_FSYNC" );
            case SQLITE_IOERR_TRUNCATE:             return( "SQLITE:IOERR_TRUNCATE" );
            case SQLITE_IOERR_FSTAT:                return( "SQLITE:IOERR_FSTAT" );
            case SQLITE_IOERR_UNLOCK:               return( "SQLITE:IOERR_UNLOCK" );
            case SQLITE_IOERR_RDLOCK:               return( "SQLITE:IOERR_RDLOCK" );
            case SQLITE_IOERR_DELETE:               return( "SQLITE:IOERR_DELETE" );
            case SQLITE_IOERR_BLOCKED:              return( "SQLITE:IOERR_BLOCKED" );
            case SQLITE_IOERR_NOMEM:                return( "SQLITE:IOERR_NOMEM" );
            case SQLITE_IOERR_ACCESS:               return( "SQLITE:IOERR_ACCESS" );
            case SQLITE_IOERR_CHECKRESERVEDLOCK:    return( "SQLITE:IOERR_CHECKRESERVEDLOCK" );
            case SQLITE_IOERR_LOCK:                 return( "SQLITE:IOERR_LOCK" );
            case SQLITE_IOERR_CLOSE:                return( "SQLITE:IOERR_CLOSE" );
            case SQLITE_IOERR_DIR_CLOSE:            return( "SQLITE:IOERR_DIR_CLOSE" );
            case SQLITE_IOERR_SHMOPEN:              return( "SQLITE:IOERR_SHMOPEN" );
            case SQLITE_IOERR_SHMSIZE:              return( "SQLITE:IOERR_SHMSIZE" );
            case SQLITE_IOERR_SHMLOCK:              return( "SQLITE:IOERR_SHMLOCK" );
            case SQLITE_IOERR_SHMMAP:               return( "SQLITE:IOERR_SHMMAP" );
            case SQLITE_IOERR_SEEK:                 return( "SQLITE:IOERR_SEEK" );
            case SQLITE_IOERR_DELETE_NOENT:         return( "SQLITE:IOERR_DELETE_NOENT" );
            case SQLITE_IOERR_MMAP:                 return( "SQLITE:IOERR_MMAP" );
            case SQLITE_IOERR_GETTEMPPATH:          return( "SQLITE:IOERR_GETTEMPPATH" );
            case SQLITE_IOERR_CONVPATH:             return( "SQLITE:IOERR_CONVPATH" );
            case SQLITE_LOCKED_SHAREDCACHE:         return( "SQLITE:LOCKED_SHAREDCACHE" );
            case SQLITE_BUSY_RECOVERY:              return( "SQLITE:BUSY_RECOVERY" );
            case SQLITE_BUSY_SNAPSHOT:              return( "SQLITE:BUSY_SNAPSHOT" );
            case SQLITE_CANTOPEN_NOTEMPDIR:         return( "SQLITE:CANTOPEN_NOTEMPDIR" );
            case SQLITE_CANTOPEN_ISDIR:             return( "SQLITE:CANTOPEN_ISDIR" );
            case SQLITE_CANTOPEN_FULLPATH:          return( "SQLITE:CANTOPEN_FULLPATH" );
            case SQLITE_CANTOPEN_CONVPATH:          return( "SQLITE:CANTOPEN_CONVPATH" );
            case SQLITE_CORRUPT_VTAB:               return( "SQLITE:CORRUPT_VTAB" );
            case SQLITE_READONLY_RECOVERY:          return( "SQLITE:READONLY_RECOVERY" );
            case SQLITE_READONLY_CANTLOCK:          return( "SQLITE:READONLY_CANTLOCK" );
            case SQLITE_READONLY_ROLLBACK:          return( "SQLITE:READONLY_ROLLBACK" );
            case SQLITE_READONLY_DBMOVED:           return( "SQLITE:READONLY_DBMOVED" );
            case SQLITE_ABORT_ROLLBACK:             return( "SQLITE:ABORT_ROLLBACK" );
            case SQLITE_CONSTRAINT_CHECK:           return( "SQLITE:CONSTRAINT_CHECK" );
            case SQLITE_CONSTRAINT_COMMITHOOK:      return( "SQLITE:CONSTRAINT_COMMITHOOK" );
            case SQLITE_CONSTRAINT_FOREIGNKEY:      return( "SQLITE:CONSTRAINT_FOREIGNKEY" );
            case SQLITE_CONSTRAINT_FUNCTION:        return( "SQLITE:CONSTRAINT_FUNCTION" );
            case SQLITE_CONSTRAINT_NOTNULL:         return( "SQLITE:CONSTRAINT_NOTNULL" );
            case SQLITE_CONSTRAINT_PRIMARYKEY:      return( "SQLITE:CONSTRAINT_PRIMARYKEY" );
            case SQLITE_CONSTRAINT_TRIGGER:         return( "SQLITE:CONSTRAINT_TRIGGER" );
            case SQLITE_CONSTRAINT_UNIQUE:          return( "SQLITE:CONSTRAINT_UNIQUE" );
            case SQLITE_CONSTRAINT_VTAB:            return( "SQLITE:CONSTRAINT_VTAB" );
            case SQLITE_CONSTRAINT_ROWID:           return( "SQLITE:CONSTRAINT_ROWID" );
            case SQLITE_NOTICE_RECOVER_WAL:         return( "SQLITE:NOTICE_RECOVER_WAL" );
            case SQLITE_NOTICE_RECOVER_ROLLBACK:    return( "SQLITE:NOTICE_RECOVER_ROLLBACK" );
            case SQLITE_WARNING_AUTOINDEX:          return( "SQLITE:WARNING_AUTOINDEX" );
            case SQLITE_AUTH_USER:                  return( "SQLITE:AUTH_USER" );
            
            default:
                _snprintf( dummy, sizeof( dummy ), "SQLITE:ERRNO%d", errorcode );
                return dummy;
         }
    }
};


/// Functors for SQL application-defined (aggregation) functions
class MexFunctors
{
    ValueMex   m_functors[3];   ///< Function handles (function, step, final)
    ValueMex   m_group_data;    ///< Data container for "step" and "final" functions
    ValueMex*  m_pexception;    ///< Exception stack information

    /// inhibit standard Ctor
    MexFunctors() {}

    /// Copy Ctor
    MexFunctors( const MexFunctors& other )
    {
        ValueMex exception = other.m_pexception->Duplicate();
        *this = MexFunctors( exception, other.getFunc(FCN), other.getFunc(STEP), other.getFunc(FINAL) );
        m_group_data = other.m_group_data;
    }


public:

    bool m_busy;              ///< true, if function is in progress to prevent recursive calls
    enum {FCN, STEP, FINAL};  ///< Subfunction number

    
    /// Ctor
    MexFunctors( ValueMex& exception, const ValueMex& func, const ValueMex& step, const ValueMex& final )
    {
        m_busy = false;

        m_functors[FCN]   = ValueMex(func).Duplicate();
        m_functors[STEP]  = ValueMex(step).Duplicate();
        m_functors[FINAL] = ValueMex(final).Duplicate();

        for( int i = 0; i < 3; i++ )
        {
            // Prevent function handles to be deleted when mex function returns
            // (MATLAB automatically deletes arrays, allocated in mex functions)
            m_functors[i].MakePersistent();
        }

        initGroupData();
        m_pexception = &exception;
    }


    /// Copy Ctor
    MexFunctors( MexFunctors& other )
    {
        *this = MexFunctors( *other.m_pexception, other.getFunc(FCN), other.getFunc(STEP), other.getFunc(FINAL) );
        m_group_data = other.m_group_data;
    }


    /// Move Ctor
    MexFunctors( MexFunctors&& other )
    {
        for( int i = 0; i < 3; i++ )
        {
            m_functors[i] = other.m_functors[i];
        }
        m_group_data = other.m_group_data;
        m_pexception = other.m_pexception;
    }


    /// Copy assignment
    MexFunctors& operator=( const MexFunctors& other )
    {
        if( this != &other )
        {
            *this = MexFunctors( other );
        }
        return *this;
    }


    /// Move assignment
    MexFunctors& operator=( MexFunctors&& other )
    {
        if( this != &other )
        {
            *this = MexFunctors( other );
        }
        return *this;
    }


    /// Dtor
    ~MexFunctors() 
    {
        for( int i = 0; i < 3; i++ )
        {
            m_functors[i].Destroy();
        }

        m_group_data.Destroy();

#ifndef NDEBUG
        PRINTF( "%s\n", "Functors destroyed" );
#endif
    }


    /// Exchange exception stack information
    void swapException( ValueMex& exception ) 
    { 
        std::swap( *m_pexception, exception ); 
    }


    /// Initialize data for "step" and "final" function
    void initGroupData()
    {
        m_group_data.Destroy();
        m_group_data = ValueMex::CreateCellMatrix( 0, 0 );
        m_group_data.MakePersistent();
    }


    /// Return data array from "step" and "final" function
    ValueMex& getData()
    {
        return m_group_data;
    }


    /// Return one of the functors (function, init or final)
    const ValueMex& getFunc(int nr) const 
    { 
        return m_functors[nr]; 
    }

    
    /// Duplicate one of the functors (function, init or final)
    ValueMex dupFunc(int nr)  const 
    { 
        return ValueMex( getFunc(nr) ).Duplicate(); 
    }

    
    /// Check if function handle is valid (not empty and of functon handle class)
    bool checkFunc(int nr)  const 
    { 
        return ValueMex( getFunc(nr) ).IsFunctionHandle(); 
    }

    
    /// Check if one of the functors is empty
    bool IsEmpty() const 
    { 
        return m_functors[FCN].IsEmpty() && m_functors[STEP].IsEmpty() && m_functors[FINAL].IsEmpty(); 
    }


    /// Check if all functors are valid
    bool IsValid() const
    {
        for( int i = 0; i < 3; i++ )
        {
            if( m_functors[i].IsEmpty() )
            {
                continue;
            }

            if( !m_functors[i].IsFunctionHandle() || m_functors[i].NumElements() != 1 )
            {
                return false;
            }
        }

        if( IsEmpty() )
        {
            return false;
        }

        return true;
    }
};


/// Class holding an exception array, the function map and the handle for one database
class SQLstackitem
{
    typedef map<string, MexFunctors*> MexFunctorsMap;   ///< Dictionary: function name => function handles

    sqlite3*        m_db;           ///< SQLite db object
    MexFunctorsMap  m_fcnmap;       ///< MEX function map with MATLAB functions for application-defined SQL functions
    ValueMex        m_exception;    ///< MATALAB exception array, may be thrown when mksqlite function leaves

public:

    /// Ctor
    SQLstackitem() : m_db( NULL )
    {}


    /// Dtor
    ~SQLstackitem()
    {
        SQLerror err;
        closeDb( err );
    }


    /// Return 
    sqlite3* dbid()
    {
        return m_db;
    }

    /// Returns the exception array for this database
    ValueMex& getException()
    {
        return m_exception;
    }


    /// (Re-)Throws an exception, if any occurred
    void throwOnException()
    {
        m_exception.Throw();
    }

  
    /// Returns the function map for this database
    MexFunctorsMap& fcnmap()
    {
        return m_fcnmap;
    }


    /// Progress handler (watchdog)
    static
    int progressHandler( void* data )
    {
        // Ctrl+C pressed?
        if( utIsInterruptPending() )
        {
            utSetInterruptHandled( true );
            PRINTF( "%s\n", ::getLocaleMsg( MSG_ABORTED ) );
            return 1;
        }
        return 0;
    }


    /// Installing progress handler to enable aborting by Ctrl+C
    void setProgressHandler( bool enable = true )
    {
        const int N_INSTRUCTIONS = 1000;
        sqlite3_progress_handler( m_db, enable ? N_INSTRUCTIONS : 0, &SQLstackitem::progressHandler, NULL );
    }


    /**
     * \brief Opens (or create) database
     *
     * \param[in] filename Name of database file
     * \param[in] openFlags Flags for access rights (see SQLite documentation for sqlite3_open_v2())
     * \param[out] err Error information
     * \returns true if succeeded
     */
    bool openDb( const char* filename, int openFlags, SQLerror& err )
    {
        if( !closeDb( err ) )
        {
            return false;
        }
        
        /*
         * Open the database
         * m_db is assigned by sqlite3_open_v2(), even if an error
         * occures
         */
        unsigned char* filename_utf8 = NULL;
        int filename_utf8_bytes = utils_latin2utf( (const unsigned char*)filename, NULL );

        if( filename_utf8_bytes )
        {
            filename_utf8 = (unsigned char*)MEM_ALLOC( filename_utf8_bytes, sizeof(char) );
            utils_latin2utf( (const unsigned char*)filename, filename_utf8 );

            if( !filename_utf8 )
            {
                err.set( MSG_ERRMEMORY );
            }
        }

        if( filename_utf8 && !err.isPending() )
        {
            int rc = sqlite3_open_v2( (char*)filename_utf8, &m_db, openFlags, NULL );

            if( SQLITE_OK != rc )
            {
                err.setSqlError( m_db, -1 );
            }

            sqlite3_extended_result_codes( m_db, true );
            attachBuiltinFunctions();
            utSetInterruptEnabled( true );
            setProgressHandler( true );
        }

        MEM_FREE( filename_utf8 );

        return !err.isPending();
    }


    /// Close database
    bool closeDb( SQLerror& err )
    {
        // Deallocate functors
        for( MexFunctorsMap::iterator it = m_fcnmap.begin(); it != m_fcnmap.end(); it++ )
        {
            delete it->second;
        }
        m_fcnmap.clear();

        // m_db may be NULL, since sqlite3_close with a NULL argument is a harmless no-op
        int rc = sqlite3_close( m_db );
        if( SQLITE_OK == rc )
        {
            m_db = NULL;
        }
        else
        {
            PRINTF( "%s\n", ::getLocaleMsg( MSG_ERRCANTCLOSE ) );
            err.setSqlError( m_db, -1 ); /* not SQL_ERR_CLOSE */
        }
        
        return !isOpen();
    }


    /// Returns true, if database is opened
    bool isOpen()
    {
        return NULL != m_db;
    }
    

    /**
     * \brief Attach builtin functions to database object
     *
     * Following builtin functions are involved:
     * - pow
     * - regex
     * - bcdratio
     * - bdcpacktime
     * - bdcunpacktime
     * - md5
     */
    void attachBuiltinFunctions()
    {
        if( !isOpen() )
        {
            assert( false );
        }
        else
        {
            // attach new SQL commands to opened database
            sqlite3_create_function( m_db, "pow", 2, SQLITE_UTF8, NULL, pow_func, NULL, NULL );                       // power function (math)
            sqlite3_create_function( m_db, "lg", 1, SQLITE_UTF8, NULL, lg_func, NULL, NULL );                         // power function (math)
            sqlite3_create_function( m_db, "ln", 1, SQLITE_UTF8, NULL, ln_func, NULL, NULL );                         // power function (math)
            sqlite3_create_function( m_db, "exp", 1, SQLITE_UTF8, NULL, exp_func, NULL, NULL );                       // power function (math)
            sqlite3_create_function( m_db, "regex", 2, SQLITE_UTF8, NULL, regex_func, NULL, NULL );                   // regular expressions (MATCH mode)
            sqlite3_create_function( m_db, "regex", 3, SQLITE_UTF8, NULL, regex_func, NULL, NULL );                   // regular expressions (REPLACE mode)
            sqlite3_create_function( m_db, "bdcratio", 1, SQLITE_UTF8, NULL, BDC_ratio_func, NULL, NULL );            // compression ratio (blob data compression)
            sqlite3_create_function( m_db, "bdcpacktime", 1, SQLITE_UTF8, NULL, BDC_pack_time_func, NULL, NULL );     // compression time (blob data compression)
            sqlite3_create_function( m_db, "bdcunpacktime", 1, SQLITE_UTF8, NULL, BDC_unpack_time_func, NULL, NULL ); // decompression time (blob data compression)
            sqlite3_create_function( m_db, "md5", 1, SQLITE_UTF8, NULL, MD5_func, NULL, NULL );                       // Message-Digest (RSA)
        }
    }
};






/**
 * \brief SQLite interface
 *
 * Encapsulates one sqlite object with pending command and statement.
 */
class SQLiface
{
    SQLstackitem*   m_pstackitem;   ///< pointer to current database
    sqlite3*        m_db;           ///< SQLite db handle
    const char*     m_command;      ///< SQL query (no ownership, read-only!)
    sqlite3_stmt*   m_stmt;         ///< SQL statement (sqlite bridge)
    SQLerror        m_lasterr;      ///< recent error message
          
public:
  friend class SQLerror;

  /// Standard ctor
  SQLiface( SQLstackitem& stackitem ) :
    m_pstackitem( &stackitem ),
    m_db( stackitem.dbid() ),
    m_command( NULL ),
    m_stmt( NULL )
  {
      // Multiple calls of sqlite3_initialize() are harmless no-ops
      sqlite3_initialize();
  }
    
  
  /// Dtor
  ~SQLiface()
  {
      closeStmt();
  }


  /// Returns true, if database is open
  bool isOpen()
  {
      return NULL != m_db;
  }


  /// Clear recent error message
  void clearErr()
  {
      m_lasterr.clear();
  }
  

  /// Get recent error message
  const char* getErr( const char** errid = NULL )
  {   
      return m_lasterr.get( errid );
  }


  /// Sets an error by its ID (see \ref MSG_IDS)
  void setErr( int err_id )
  {
      m_lasterr.set( err_id );
  }


  /// Sets an error by its SQL return code
  void setSqlError( int rc )
  {
      m_lasterr.setSqlError( m_db, rc );
  }


  /// Returns true, if an unhandled error is pending
  bool errPending()
  {
      return m_lasterr.isPending();
  }


  /// Get the filename of current database
  const char* getDbFilename( const char* database )
  {
      if( !isOpen() )
      {
          assert( false );
          return NULL;
      }

      return sqlite3_db_filename( m_db, database ? database : "MAIN" );
  }


  /// Sets the busy timemout in milliseconds
  bool setBusyTimeout( int iTimeoutValue )
  {
      if( !isOpen() )
      {
          assert( false );
          return false;
      }
      
      int rc = sqlite3_busy_timeout( m_db, iTimeoutValue );
      if( SQLITE_OK != rc )
      {
          setSqlError( rc );
          return false;
      }
      return true;
  }
  
  
  /// Returns the busy timeout in milliseconds
  bool getBusyTimeout( int& iTimeoutValue )
  {
      if( !isOpen() )
      {
          assert( false );
          return false;
      }
      
      int rc = sqlite3_busy_timeout( m_db, iTimeoutValue );
      if( SQLITE_OK != rc )
      {
          setSqlError( rc );
          return false;
      }
      return true;
  }
  

  /// Enable or disable load extensions
  bool setEnableLoadExtension( int flagOnOff )
  {
      if( !isOpen() )
      {
          assert( false );
          return false;
      }
      
      int rc = sqlite3_enable_load_extension( m_db, flagOnOff != 0 );
      if( SQLITE_OK != rc )
      {
          setSqlError( rc );
          return false;
      }
      return true;
  }
  
  
  /// Closing current statement
  void closeStmt()
  {
      if( m_stmt )
      {
          // sqlite3_reset() does not reset the bindings on a prepared statement!
          sqlite3_clear_bindings( m_stmt );
          sqlite3_reset( m_stmt );
          sqlite3_finalize( m_stmt );
          m_stmt = NULL;
          m_command = NULL;
      }
  }
  
  
  /// Wrapper for SQL function
  static 
  void mexFcnWrapper_FCN( sqlite3_context *ctx, int argc, sqlite3_value **argv )
  {
      mexFcnWrapper( ctx, argc, argv, MexFunctors::FCN );
  }
  
  /// Wrapper for SQL step function (aggregation)
  static 
  void mexFcnWrapper_STEP( sqlite3_context *ctx, int argc, sqlite3_value **argv )
  {
      mexFcnWrapper( ctx, argc, argv, MexFunctors::STEP );
  }
  
  /// Wrapper for SQL final function (aggregation)
  static 
  void mexFcnWrapper_FINAL( sqlite3_context *ctx )
  {
      mexFcnWrapper( ctx, 0, NULL, MexFunctors::FINAL );
  }
  
  /// Common wrapper for all user defined SQL functions
  static
  void mexFcnWrapper( sqlite3_context *ctx, int argc, sqlite3_value **argv, int func_nr )
  {
      MexFunctors* fcn = (MexFunctors*)sqlite3_user_data( ctx );
      int nArgs = argc + 1 + (func_nr > MexFunctors::FCN);  // Number of arguments for "feval"
      ValueMex arg( ValueMex::CreateCellMatrix( 1, nArgs ) );
      bool failed = false;

      assert( fcn && arg.Item() );
      
      if( !fcn->checkFunc(func_nr) )
      {
          arg.Destroy();
          sqlite3_result_error( ctx, ::getLocaleMsg( MSG_INVALIDFUNCTION ), -1 );
          failed = true;
      }

      if( fcn->m_busy )
      {
          arg.Destroy();
          sqlite3_result_error( ctx, ::getLocaleMsg( MSG_RECURSIVECALL ), -1 );
          failed = true;
      }

      // Transform SQL value arguments into a MATLAB cell array
      for( int j = 0; j < nArgs && !failed; j++ )
      {
          int err_id = MSG_NOERROR;
          int i = j;
          ValueSQL value;
          ValueMex item;
          
          if( j == 0 )
          {
              arg.SetCell( j, fcn->dupFunc(func_nr).Detach() );
              continue;
          }
          i--;

          if( func_nr > MexFunctors::FCN )
          {
              if( j == 1 )
              {
                  assert( fcn->getData().Item() );
                  arg.SetCell( j, fcn->getData().Duplicate().Detach() );
                  continue;
              }
              i--;
          }

          switch( sqlite3_value_type( argv[i] ) )
          {
              case SQLITE_NULL:
                  break;

              case SQLITE_INTEGER:   
                  value = ValueSQL( sqlite3_value_int64( argv[i] ) );
                  break;

              case SQLITE_FLOAT:
                  value = ValueSQL( sqlite3_value_double( argv[i] ) );
                  break;

              case SQLITE_TEXT:
              {
                  char* str = (char*)utils_strnewdup( (const char*)sqlite3_value_text( argv[i] ), g_convertUTF8 );

                  if( str )
                  {
                      value = ValueSQL( str );
                      break;
                  }
              }

              case SQLITE_BLOB:      
              {
                  size_t bytes = sqlite3_value_bytes( argv[i] );

                  item = ValueMex( (int)bytes, bytes ? 1 : 0, ValueMex::UINT8_CLASS );

                  if( item.Data() )
                  {
                      if( bytes )
                      {
                          memcpy( item.Data(), sqlite3_value_blob( argv[i] ), bytes );
                      }

                      value = ValueSQL( item.Detach() );
                  }
                  else
                  {
                      sqlite3_result_error( ctx, getLocaleMsg( MSG_ERRMEMORY ), -1 );
                      failed = true;
                  }

                  break;
              }

              default:
                  sqlite3_result_error( ctx, getLocaleMsg( MSG_UNKNWNDBTYPE ), -1 );
                  failed = true;
          }
          
          if( failed )
          {
              value.Destroy();
          }
          else
          {
              // Cumulate arguments into a cell array
              arg.SetCell( j, createItemFromValueSQL( value, err_id ).Detach() );
              
              if( MSG_NOERROR != err_id )
              {
                  sqlite3_result_error( ctx, ::getLocaleMsg( err_id ), -1 );
                  failed = true;
              }
          }
      }
      
      if( !failed )
      {
          ValueMex exception, item;
          fcn->m_busy = true;
          arg.Call( &item, &exception );
          fcn->m_busy = false;

          if( !exception.IsEmpty() )
          {
              // Exception handling
              fcn->swapException( exception );
              sqlite3_result_error( ctx, "MATLAB Exception!", -1 );
              failed = true;
          }
          else
          {
              if( func_nr == MexFunctors::STEP )
              {
                  if( !item.IsEmpty() )
                  {
                      item.MakePersistent();
                      std::swap( fcn->getData(), item );
                      item.Destroy();
                  }
                  sqlite3_result_null( ctx );
              }
              else
              {
                  if( !item.IsEmpty() )
                  {
                      int iTypeComplexity;
                      int err_id = MSG_NOERROR;

                      ValueSQL value = createValueSQLFromItem( item, can_serialize(), iTypeComplexity, err_id );

                      if( MSG_NOERROR != err_id )
                      {
                          Err err;
                          err.set( err_id );
                          sqlite3_result_error( ctx, err.get(), -1 );
                      }
                      else
                      {
                          switch( value.m_typeID )
                          {
                              case SQLITE_NULL:
                                  sqlite3_result_null( ctx );
                                  break;

                              case SQLITE_FLOAT:
                                  // scalar floating point value
                                  sqlite3_result_double( ctx, item.GetScalar() );
                                  break;

                              case SQLITE_INTEGER:
                                  if( (int)item.ClassID() == (int)ValueMex::INT64_CLASS )
                                  {
                                      // scalar integer value
                                      sqlite3_result_int64( ctx, item.GetInt64() );
                                  }
                                  else
                                  {
                                      // scalar integer value
                                      sqlite3_result_int( ctx, item.GetInt() );
                                  }
                                  break;

                              case SQLITE_TEXT:
                                  // string argument
                                  // SQLite makes a local copy of the text (thru SQLITE_TRANSIENT)
                                  sqlite3_result_text( ctx, value.m_text, -1, SQLITE_TRANSIENT );
                                  break;

                              case SQLITE_BLOB:
                                  // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
                                  sqlite3_result_blob( ctx, item.Data(), 
                                                       (int)item.ByData(),
                                                       SQLITE_TRANSIENT );
                                  break;

                              case SQLITE_BLOBX:
                                  // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
                                  // the sqlite allocator provided blob memory
                                  sqlite3_result_blob( ctx, value.Detach(), 
                                                       (int)value.m_blobsize, 
                                                       sqlite3_free );
                                  break;

                              default:
                              {
                                  // all other (unsuppored types)
                                  Err err;
                                  err.set( MSG_INVALIDARG );
                                  sqlite3_result_error( ctx, err.get(), - 1 );
                                  break;
                              }
                          }
                      }
                  }
                  else
                  {
                      sqlite3_result_null( ctx );
                  }
              }
          }

          item.Destroy();
          exception.Destroy();
      }

      arg.Destroy();

      if( func_nr == MexFunctors::FINAL )
      {
          fcn->initGroupData();
      }
  }
  
  
  /**
   * \brief Attach application-defined function to database object
   */
  bool attachMexFunction( const char* name, const ValueMex& func, const ValueMex& step, const ValueMex& final, ValueMex& exception )
  {
      if( !isOpen() )
      {
          assert( false );
      }
      else
      {
          MexFunctors* fcn = new MexFunctors( exception, func, step, final );
          int rc = SQLITE_OK;
          int action = -1;
          bool failed = false;
          
          if( fcn->IsEmpty() )
          {
              // Remove function
              rc = sqlite3_create_function( m_db, name, -1, SQLITE_UTF8, NULL, NULL, NULL, NULL );
              action = 0;
          }
          else
          {
              if( !fcn->IsValid() )
              {
                  setErr( MSG_FCNHARGEXPCT );
                  failed = true;
              }
              else
              {
                  // Add function
                  void (*xFunc)(sqlite3_context*,int,sqlite3_value**) = mexFcnWrapper_FCN;
                  void (*xStep)(sqlite3_context*,int,sqlite3_value**) = mexFcnWrapper_STEP;
                  void (*xFinal)(sqlite3_context*)                    = mexFcnWrapper_FINAL;

                  if( fcn->getFunc(MexFunctors::FCN).IsEmpty() )   xFunc  = NULL;
                  if( fcn->getFunc(MexFunctors::STEP).IsEmpty() )  xStep  = NULL;
                  if( fcn->getFunc(MexFunctors::FINAL).IsEmpty() ) xFinal = NULL;

                  rc = sqlite3_create_function( m_db, name, -1, SQLITE_UTF8, (void*)fcn, xFunc, xStep, xFinal );
                  action = 1;
              }
          }
                  
          if( SQLITE_OK != rc )
          {
              setSqlError( rc );
              failed = true;
          }

          if( !failed )
          {
              if( action >= 0 )
              {
                  if( m_pstackitem->fcnmap().count(name) )
                  {
#ifndef NDEBUG
                      PRINTF( "Deleting functors for %s\n", name );
#endif
                      delete m_pstackitem->fcnmap()[name];
                      m_pstackitem->fcnmap().erase(name);
                  }
              }

              if( action > 0 )
              {
                  m_pstackitem->fcnmap()[name] = fcn;
                  fcn = NULL;
              }
          }

          delete fcn;

          return !errPending();
      }
      
      return true;
  }
  
  
  /**
   * \brief Dispatch a SQL query
   *
   * \param[in] query String containing SQL statement
   */
  bool setQuery( const char* query )
  {
      if( !isOpen() )
      {
          assert( false );
          return false;
      }

      /*
       * complete the query
       */
      // sqlite3_complete() returns 1 if the string is complete and valid...
      if( !sqlite3_complete( query ) )
      {
          setErr( MSG_INVQUERY );
          return false;
      }

      // Close previous statement, if any
      closeStmt();
      
      /*
       * and prepare it
       * if anything is wrong with the query, than complain about it.
       */
      int rc = sqlite3_prepare_v2( m_db, query, -1, &m_stmt, 0 );
      if( SQLITE_OK != rc )
      {
          setSqlError( rc );
          return false;
      }
      
      m_command = query;
      return true;
  }
  
  
  /// Returns the count of parameters the current statement expects
  int getParameterCount()
  {
      return m_stmt ? sqlite3_bind_parameter_count( m_stmt ) : 0;
  }
  
  /// Returns the name for nth parameter
  const char* getParameterName( int n )
  {
      return m_stmt ? sqlite3_bind_parameter_name( m_stmt, n ) : NULL;
  }
  
  /// kv69: Returns the number of last row id; usefull for inserts in tables with autoincrement primary keys
  long getLastRowID()
  {
      return m_stmt ? (long)sqlite3_last_insert_rowid(m_db) : 0;
  }
  
  
  /// Clears all parameter bindings from current statement
  void clearBindings()
  {
      if( m_stmt )
      {
          sqlite3_clear_bindings( m_stmt );
      }
  }
  
  
  /**
   * \brief Binds one parameter from current statement to a MATLAB array
   *
   * \param[in] index Parameter number (0 based)
   * \param[in] item MATLAB array
   * \param[in] bStreamable true, if streaming is possible and desired
   */
  bool bindParameter( int index, const ValueMex& item, bool bStreamable )
  {
      int err_id = MSG_NOERROR;
      int iTypeComplexity;
      int rc;

      assert( isOpen() );

      ValueSQL value = createValueSQLFromItem( item, bStreamable, iTypeComplexity, err_id );

      if( MSG_NOERROR != err_id )
      {
          setErr( err_id );
          return false;
      }

      switch( value.m_typeID )
      {
          case SQLITE_NULL:
              rc = sqlite3_bind_null( m_stmt, index );
              if( SQLITE_OK != rc )
              {
                  setSqlError( rc );
              }
              break;

          case SQLITE_FLOAT:
              // scalar floating point value
              rc = sqlite3_bind_double( m_stmt, index, item.GetScalar() );
              if( SQLITE_OK != rc )
              {
                  setSqlError( rc );
              }
              break;

          case SQLITE_INTEGER:
              if( (int)item.ClassID() == (int)ValueMex::INT64_CLASS )
              {
                  // scalar integer value
                  rc = sqlite3_bind_int64( m_stmt, index, item.GetInt64() );
                  if( SQLITE_OK != rc )
                  {
                      setSqlError( rc );
                  }
              }
              else
              {
                  // scalar integer value
                  rc = sqlite3_bind_int( m_stmt, index, item.GetInt() );
                  if( SQLITE_OK != rc )
                  {
                      setSqlError( rc );
                  }
              }
              break;

          case SQLITE_TEXT:
              // string argument
              // SQLite makes a local copy of the text (thru SQLITE_TRANSIENT)
              rc = sqlite3_bind_text( m_stmt, index, value.m_text, -1, SQLITE_TRANSIENT );
              if( SQLITE_OK != rc )
              {
                  setSqlError( rc );
              }
              break;

          case SQLITE_BLOB:
              // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
              rc = sqlite3_bind_blob( m_stmt, index, item.Data(), 
                                      (int)item.ByData(),
                                      SQLITE_TRANSIENT );
              if( SQLITE_OK != rc )
              {
                  setSqlError( rc );
              }
              break;

          case SQLITE_BLOBX:
              // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
              // the sqlite allocator provided blob memory
              rc = sqlite3_bind_blob( m_stmt, index, value.Detach(), 
                                      (int)value.m_blobsize, 
                                      sqlite3_free );
              if( SQLITE_OK != rc )
              {
                  setSqlError( rc );
              }  
              break;

          default:
              // all other (unsuppored types)
              setErr( MSG_INVALIDARG );
              break;
      }
      
      return !errPending();
  }
  
  
  /// Evaluates current SQL statement
  int step()
  {
      return m_stmt ? sqlite3_step( m_stmt ) : SQLITE_ERROR;
  }
  
  
  /// Returns the column count for current statement
  int colCount()
  {
      return m_stmt ? sqlite3_column_count( m_stmt ) : 0;
  }
  
  
  /**
   * \brief Returns the (prefered) type of one column for current statement
   *
   * \param[in] index Index number of desired column (0 based)
   * \returns -1 on error
   */
  int colType( int index )
  {
      return m_stmt ? sqlite3_column_type( m_stmt, index ) : -1;
  }
  
  
  /// Returns an integer value for least fetch and column number
  sqlite3_int64 colInt64( int index )
  {
      return m_stmt ? sqlite3_column_int64( m_stmt, index ) : 0;
  }
  
  
  /// Returns a floating point value for least fetch and column number
  double colFloat( int index )
  {
      return m_stmt ? sqlite3_column_double( m_stmt, index ) : 0.0;
  }
  
  
  /// Returns a text value for least fetch and column number
  const unsigned char* colText( int index )
  {
      return m_stmt ? sqlite3_column_text( m_stmt, index ) : (const unsigned char*)"";
  }
  
  
  /// Returns a BLOB for least fetch and column number
  const void* colBlob( int index )
  {
      unsigned char* test = (unsigned char*)sqlite3_column_blob( m_stmt, index );
      return m_stmt ? sqlite3_column_blob( m_stmt, index ) : NULL;
  }
  
  
  /// Returns the size of one value of least fetch and column number in bytes
  size_t colBytes( int index )
  {
      return m_stmt ? sqlite3_column_bytes( m_stmt, index ) : 0;
  }
  
  
  /// Returns the column name least fetch and column number 
  const char* colName( int index )
  {
      return m_stmt ? sqlite3_column_name( m_stmt, index ) : "";
  }
  
  
  /// Converts one char to a printable (non-white-space) character
  struct to_alphanum
  {
      /// Functor
      char operator()( char a )
      {
          if( a >= 'a' && a <= 'z' ) return a;
          if( a >= 'A' && a <= 'Z' ) return a;
          if( a >= '0' && a <= '9' ) return a;

          return '_';
      }
  };
  
  
  /**
   * \brief Returns the column names of least fetch
   *
   * The column names are used as field names in MATLAB arrays.
   * MATLAB doesn't permit all characters in field names, moreover
   * fieldnames must be unambiguous. So there is a second name which
   * is used to be that fieldname. Column name and field name are 
   * represented by string pairs.
   *
   * \param[out] names String pair list for column names
   * \returns Column count
   */
  int getColNames( ValueSQLCol::StringPairList& names )
  {
      names.clear();
      
      // iterate columns
      for( int i = 0; i < colCount(); i++ )
      {
          pair<string,string> item( colName(i), colName(i) );
          
          // truncate column name if necessary
          item.second = item.second.substr( 0, g_namelengthmax );
          
          // only alphanumeric characters allowed, transform alias
          std::transform( item.second.begin(), item.second.end(), item.second.begin(), to_alphanum() );
          
          // fieldname must start with a valid letter
          if( !item.second.size() || !isalpha(item.second[0]) )
          {
              item.second = string("X") + item.second;  /// \todo Any other (better) ideas?
          }
          
          // Optionally ensure fieldnames are unambiguous
          if( g_check4uniquefields )
          {
              int loop = 0;
              int number = 1;
              string new_name(item.second);
              
              // break if more than 100 equal column names  \literal
              while( loop < i && number < 100 )
              {
                  // if name exists already, then append consecutive numbers to differ
                  if( new_name == names[loop].second )
                  {
                      char* str_number      = new char[g_namelengthmax+1];
                      char* buffer          = new char[g_namelengthmax+1];
                      int   str_number_len;

                      // measure suffix length 
                      str_number_len = _snprintf( str_number, g_namelengthmax+1, "_%d", number );
                      // truncate name if necessary and append suffix
                      _snprintf( buffer, g_namelengthmax+1, "%.*s%s", g_namelengthmax - str_number_len, item.second.c_str(), str_number );
                      new_name = buffer;

                      delete[] str_number;
                      delete[] buffer;
                      
                      loop = 0; // name is new, check again precedent
                      number++;

                  } else loop++;
              }
              
              // number may not exceed limit
              if( loop < i )
              {
                  names.clear();
                  setErr( MSG_ERRVARNAME );
                  break;
              }
              
              item.second = new_name;
          }
          
          names.push_back( item );
      }
      
      return (int)names.size();
  }
  
  
  /// Reset current SQL statement
  void reset()
  {
      if( m_stmt )
      {
          sqlite3_reset( m_stmt );
      }
  }
  
  
  /// Clear parameter bindings and finalize current statement
  void finalize()
  {
      if( m_stmt )
      {
          sqlite3_clear_bindings( m_stmt );
          sqlite3_finalize( m_stmt );
          m_stmt = NULL;
      }
  }

    
  /** 
   * \brief Proceed a table fetch
   *
   * \param[out] cols Column vectors to collect results
   * \param[in] initialize Initializing \a cols if set (only on first call of fetch() 
   *                       when parameter wrapping is on)
   *
   * Stepping through the results and stores the results in column vectors.
   */
  bool fetch( ValueSQLCols& cols, bool initialize = false ) // kv69: enable for skipping initialization to accumulate query results
  {
      assert( isOpen() );
      
      if( initialize )
      {
          ValueSQLCol::StringPairList  names;

          getColNames( names );
          cols.clear();

          // build column vectors
          for( int i = 0; i < (int)names.size(); i++ )
          {
              cols.push_back( ValueSQLCol(names[i]) );
          }

          names.clear();
      }

      // step through
      for( ; !errPending() ; )
      {
          /*
           * Advance to the next row
           */
          int step_res = step();

          if (step_res == SQLITE_DONE) // kv69 sqlite has finished executing
          {
              break;
          }


          if (step_res != SQLITE_ROW) // kv69 no other row ? this must be an error
          {
              setSqlError( step_res );
              break;
          }

          /*
           * get new memory for the result
           */
          for( int jCol = 0; jCol < (int)cols.size() && !errPending(); jCol++ )
          {
              // Init value as SQLITE_NULL;
              ValueSQL value;

              switch( colType( jCol ) )
              {
                  case SQLITE_NULL:      
                      break;

                  case SQLITE_INTEGER:   
                      value = ValueSQL( colInt64( jCol ) );
                      break;

                  case SQLITE_FLOAT:
                      value = ValueSQL( colFloat( jCol ) );
                      break;

                  case SQLITE_TEXT:
                      value = ValueSQL( (char*)utils_strnewdup( (const char*)colText( jCol ), g_convertUTF8 ) );
                      break;

                  case SQLITE_BLOB:      
                  {
                      size_t bytes = colBytes( jCol );

                      ValueMex item = ValueMex( (int)bytes, bytes ? 1 : 0, ValueMex::UINT8_CLASS );

                      if( item.Item() )
                      {
                          if( bytes )
                          {
                              memcpy( item.Data(), colBlob( jCol ), bytes );
                          }
                      }
                      else
                      {
                          setErr( MSG_ERRMEMORY );
                          continue;
                      }

                      value = ValueSQL( item.Detach() );
                      break;
                  }

                  default:
                      setErr( MSG_UNKNWNDBTYPE );
                      continue;
              }
              
              cols[jCol].append( value );
          }
      }
      
      if( errPending() )
      {
          cols.clear();
          return false;
      }

      return true;
  }
  
};
