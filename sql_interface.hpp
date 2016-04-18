/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      sql_interface.hpp
 *  @brief     SQLite interface class
 *  @details   SQLite accessing functions, for single-file databases
 *  @see       http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data
 *  @authors   Martin Kortmann <mail@kortmann.de>,
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.2
 *  @date      2008-2016
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

//#include "config.h"
//#include "global.hpp"
//#include "sqlite/sqlite3.h"
#include "sql_user_functions.hpp"
//#include "utils.hpp"
//#include "value.hpp"
//#include "locale.hpp"


// For SETERR usage:
#define SQL_ERR        "SQL_ERR"          ///< if attached to g_finalize_msg, function returns with least SQL error message
#define SQL_ERR_CLOSE  "SQL_ERR_CLOSE"    ///< same as SQL_ERR, additionally the responsible db will be closed

/// type for column container
typedef vector<ValueSQLCol> ValueSQLCols;

/**
 * \brief SQLite interface
 *
 * Encapsulates one sqlite object with pending command and statement.
 */
class SQLiface
{
    sqlite3*        m_db;           ///< SQLite db object
    const char*     m_command;      ///< SQL query (no ownership, read-only!)
    sqlite3_stmt*   m_stmt;         ///< SQL statement (sqlite bridge)
    Err             m_lasterr;      ///< recent error message
          
public:
  /// Standard ctor
  SQLiface() :
    m_db( NULL ),
    m_command( NULL ),
    m_stmt( NULL )
  {
      // Multiple calls of sqlite3_initialize() are harmless no-ops
      sqlite3_initialize();
  }
    
  
  /// Dtor
  ~SQLiface()
  {
      closeDb();
  }
  
  
  /// Clear recent error message
  void clearErr()
  {
      m_lasterr.clear();
  }
  

  /// Get recent error message
  const char* getErr( const char** errid = NULL )
  {
      if( m_lasterr.get() == SQL_ERR || m_lasterr.get() == SQL_ERR_CLOSE )
      {
          if( errid )
          {
              *errid = trans_err_to_ident();
          }
          
          // return SQLite error message
          return sqlite3_errmsg( m_db );
      }
      else
      {
          if( errid )
          {
              *errid = NULL;
          }
          
          // return mksqlite error message
          return m_lasterr.get();
      }
  }
  
  
  /// Returns true, if an unhandled error is pending
  bool errPending()
  {
      return m_lasterr.isPending();
  }
  
  
  /// Returns true, if database is open
  bool isOpen()
  {
      return NULL != m_db;
  }
  
  
  /// Sets the busy tiemout in milliseconds
  bool setBusyTimeout( int iTimeoutValue )
  {
      if( !isOpen() )
      {
          assert( false );
          return false;
      }
      
      if( SQLITE_OK != sqlite3_busy_timeout( m_db, iTimeoutValue ) )
      {
          m_lasterr.set( SQL_ERR );
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
      
      if( SQLITE_OK != sqlite3_busy_timeout( m_db, iTimeoutValue ) )
      {
          m_lasterr.set( SQL_ERR );
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
      
      if( SQLITE_OK != sqlite3_enable_load_extension( m_db, flagOnOff != 0 ) )
      {
          m_lasterr.set( SQL_ERR );
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
  
  
  /// Close database
  bool closeDb()
  {
      closeStmt();
      
      // sqlite3_close with a NULL argument is a harmless no-op
      if( SQLITE_OK == sqlite3_close( m_db ) )
      {
          m_db = NULL;
      }
      else
      {
          mexPrintf( "%s\n", ::getLocaleMsg( MSG_ERRCANTCLOSE ) );
          m_lasterr.set( SQL_ERR ); /* not SQL_ERR_CLOSE */
      }
      
      return !isOpen();
  }
  
  
  /**
   * \brief Opens (or create) database
   *
   * \param[in] filename Name of database file
   * \param[in] openFlags Flags for access rights (see SQLite documentation for sqlite3_open_v2())
   * \returns true if succeeded
   */
  bool openDb( const char* filename, int openFlags )
  {
      if( errPending() ) return false;
      
      if( !closeDb() )
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
              m_lasterr.set( MSG_ERRMEMORY );
          }
      }

      if( filename_utf8 && !errPending() )
      {
          int rc = sqlite3_open_v2( (char*)filename_utf8, &m_db, openFlags, NULL );

          if( SQLITE_OK != rc )
          {
              m_lasterr.set( SQL_ERR );
          }
      }

      MEM_FREE( filename_utf8 );

      if( errPending() )
      {
          return false;
      } 
      else 
      {
          attachFunctions();
          return true;
      }
  }
  
  
  /**
   * \brief Get least SQLite error code and return identifier as string
   */
  const char* trans_err_to_ident()
  {
      static char dummy[32];

      int errorcode = sqlite3_extended_errcode( m_db );

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
  
  
  /**
   * \brief Attach user functions to database object
   *
   * Following user functions are involved:
   * - pow
   * - regex
   * - bcdratio
   * - bdcpacktime
   * - bdcunpacktime
   * - md5
   */
  void attachFunctions()
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
  
  
  /**
   * \brief Dispatch a SQL query
   *
   * \param[in] query String containing SQL statement
   */
  bool setQuery( const char* query )
  {
      /*
       * complete the query
       */
      // sqlite3_complete() returns 1 if the string is complete and valid...
      if( !sqlite3_complete( query ) )
      {
          m_lasterr.set( MSG_INVQUERY );
          return false;
      }

      // Close previous statement, if any
      closeStmt();
      
      /*
       * and prepare it
       * if anything is wrong with the query, than complain about it.
       */
      if( SQLITE_OK != sqlite3_prepare_v2( m_db, query, -1, &m_stmt, 0 ) )
      {
          m_lasterr.set( SQL_ERR );
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
   * \param[in] pItem MATLAB array
   * \param[in] bStreamable true, if streaming is possible and desired
   */
  bool bindParameter( int index, const mxArray* pItem, bool bStreamable )
  {
      ValueMex value( pItem );  // MATLAB array wrapper
      int iTypeComplexity = pItem ? value.Complexity( bStreamable ) : ValueMex::TC_EMPTY;

      switch( iTypeComplexity )
      {
        case ValueMex::TC_COMPLEX:
          // structs, cells and complex data 
          // can only be stored as officially undocumented byte stream feature
          // (SQLite typed ByteStream BLOB)
          if( !bStreamable || !typed_blobs_mode_on() )
          {
              m_lasterr.set( MSG_INVALIDARG );
              break;
          }
          
          /* fallthrough */
        case ValueMex::TC_SIMPLE_ARRAY:
          // multidimensional non-complex numeric or char arrays
          // will be stored as vector(!).
          // Caution: Array dimensions are lost, if you don't use typed blobs
          // nor serialization
            
          /* fallthrough */
        case ValueMex::TC_SIMPLE_VECTOR:
          // non-complex numeric vectors (SQLite BLOB)
          if( !typed_blobs_mode_on() )
          {
              // array data will be stored as anonymous byte stream blob.
              // no automatically reconstruction of data dimensions or types 
              // is available!
              
              // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
              if( SQLITE_OK != sqlite3_bind_blob( m_stmt, index, value.Data(), 
                                                  (int)value.ByData(),
                                                  SQLITE_TRANSIENT ) )
              {
                  m_lasterr.set( SQL_ERR );
              }
          } 
          else 
          {
              // data will be stored in a typed blob. Data and structure types
              // will be recovered, on data recall
              void*  blob          = NULL;
              size_t blob_size     = 0;
              double process_time  = 0.0;
              double ratio         = 0.0;

              /* blob_pack() modifies g_finalize_msg */
              m_lasterr.set( blob_pack( pItem, bStreamable, &blob, &blob_size, &process_time, &ratio ) );
              
              if( !errPending() )
              {
                  // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
                  // the sqlite allocator provided blob memory
                  if( SQLITE_OK != sqlite3_bind_blob( m_stmt, index, blob, (int)blob_size, sqlite3_free ) )
                  {
                      m_lasterr.set( SQL_ERR );
                  }  
              }
          }
          break;
          
        case ValueMex::TC_SIMPLE:
          // 1-value non-complex scalar, char or simple string (SQLite simple types)
          switch( value.ClassID() )
          {
              case mxLOGICAL_CLASS:
              case mxINT8_CLASS:
              case mxUINT8_CLASS:
              case mxINT16_CLASS:
              case mxINT32_CLASS:
              case mxUINT16_CLASS:
              case mxUINT32_CLASS:
                  // scalar integer value
                  if( SQLITE_OK != sqlite3_bind_int( m_stmt, index, value.GetInt() ) )
                  {
                      m_lasterr.set( SQL_ERR );
                  }
                  break;
                  
              case mxINT64_CLASS:
                  // scalar integer value
                  if( SQLITE_OK != sqlite3_bind_int64( m_stmt, index, value.GetInt64() ) )
                  {
                      m_lasterr.set( SQL_ERR );
                  }
                  break;

              case mxDOUBLE_CLASS:
              case mxSINGLE_CLASS:
                  // scalar floating point value
                  if( SQLITE_OK != sqlite3_bind_double( m_stmt, index, value.GetScalar() ) )
                  {
                      m_lasterr.set( SQL_ERR );
                  }
                  break;
                  
              case mxCHAR_CLASS:
              {
                  // string argument
                  char* str_value = value.GetEncString();
                  
                  if( !str_value )
                  {
                      m_lasterr.set( MSG_ERRMEMORY );
                  }
                  
                  if( !errPending() )
                  {
                      // SQLite makes a local copy of the blob (thru SQLITE_TRANSIENT)
                      if( SQLITE_OK != sqlite3_bind_text( m_stmt, index, str_value, -1, SQLITE_TRANSIENT ) )
                      {
                          m_lasterr.set( SQL_ERR );
                      }
                  }
                  
                  ::utils_free_ptr( str_value );
                  break;
              }

              default:
                  // all other (unsuppored types)
                  m_lasterr.set( MSG_INVALIDARG );
                  break;

          } // end switch
          break;
          
        case ValueMex::TC_EMPTY:
            if( SQLITE_OK != sqlite3_bind_null( m_stmt, index ) )
            {
                m_lasterr.set( SQL_ERR );
            }
            break;
          
        default:
            // all other (unsuppored types)
            m_lasterr.set( MSG_INVALIDARG );
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
      return test;
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
          return ::isalnum(a) ? a : '_';
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
                  m_lasterr.set( MSG_ERRVARNAME );
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
      sqlite3_clear_bindings( m_stmt );
      sqlite3_finalize( m_stmt );
      m_stmt = NULL;
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
              m_lasterr.set( SQL_ERR );
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

                      mxArray* item = mxCreateNumericMatrix( (int)bytes, bytes ? 1 : 0, mxUINT8_CLASS, mxREAL );

                      if( item )
                      {
                          value = ValueSQL( item );

                          if( bytes )
                          {
                              ValueMex v(item);
                              memcpy( v.Data(), colBlob( jCol ), bytes );
                          }
                      }
                      else
                      {
                          m_lasterr.set( MSG_ERRMEMORY );
                          continue;
                      }

                      break;
                  }

                  default:
                      m_lasterr.set( MSG_UNKNWNDBTYPE );
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
