/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      sql_builtin_functions.hpp
 *  @brief     SQL builtin functions, automatically attached to each database
 *  @details   Additional functions in SQL statements (MD5, regex, pow, and packing ratio/time)
 *  @see       http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data
 *  @authors   Martin Kortmann <mail@kortmann.de>,
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.5
 *  @date      2008-2017
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

/* Extending SQLite with additional builtin functions */

//#include "config.h"
//#include "global.hpp"
//#include "sqlite/sqlite3.h"
#include "typed_blobs.hpp"
#include "number_compressor.hpp"
#include "serialize.hpp"
#include "deelx/deelx.h"
//#include "utils.hpp"

extern "C"
{
  #include "md5/md5.h"  /* little endian only! */
}

/* SQLite function extensions by mksqlite */
void pow_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void lg_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void ln_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void exp_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void regex_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_ratio_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_pack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_unpack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void MD5_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );


// Forward declarations
int  blob_pack    ( const mxArray* pcItem, bool bStreamable, 
                    void** ppBlob, size_t* pBlob_size, 
                    double *pdProcess_time, double* pdRatio,
                    const char* compressor = g_compression_type, 
                    int level = g_compression_level );
int  blob_unpack  ( const void* pBlob, size_t blob_size, 
                    bool bStreamable, mxArray** ppItem, 
                    double* pProcess_time, double* pdRatio );
void blob_free    ( void** pBlob );


#ifdef MAIN_MODULE

/* sqlite builtin functions, implementations */

/**
 * \brief Power function implementation
 *
 * Computes the equation result = pow( base, exponent )\n
 * where base is argv[0] and exponent is argv[1]
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void pow_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 2 ) ;
    double base, exponent, result;
    
    // Check "base" parameter (handles NULL and double types)
    switch( sqlite3_value_type( argv[0] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            base = sqlite3_value_double( argv[0] );
    }
    
    // Check "exponent" parameter (handles NULL and double types)
    switch( sqlite3_value_type( argv[1] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            exponent = sqlite3_value_double( argv[1] );
    }

    try
    {
        result = pow( base, exponent );
    }
    catch( ... )
    {
        sqlite3_result_error( ctx, "pow(): evaluation error", -1 );
        return;
    }
    
    sqlite3_result_double( ctx, result );
}


/* sqlite builtin functions, implementations */

/**
 * \brief Logarithm function to base 10 implementation
 *
 * Computes the equation result = log10( value )\n
 * where value is argv[0] 
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void lg_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    double value, result;
    
    // Check "value" parameter (handles NULL and double types)
    switch( sqlite3_value_type( argv[0] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            value = sqlite3_value_double( argv[0] );
    }
    
    try
    {
        result = log10( value );
    }
    catch( ... )
    {
        sqlite3_result_error( ctx, "lg(): evaluation error", -1 );
        return;
    }
    
    sqlite3_result_double( ctx, result );
}


/**
 * \brief Natural logarithm function implementation
 *
 * Computes the equation result = ln( value )\n
 * where value is argv[0] 
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void ln_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    double value, result;
    
    // Check "value" parameter (handles NULL and double types)
    switch( sqlite3_value_type( argv[0] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            value = sqlite3_value_double( argv[0] );
    }
    
    try
    {
        result = log( value );
    }
    catch( ... )
    {
        sqlite3_result_error( ctx, "ln(): evaluation error", -1 );
        return;
    }
    
    sqlite3_result_double( ctx, result );
}


/**
 * \brief Exponential function implementation
 *
 * Computes the equation result = exp( value )\n
 * where value is argv[0] 
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void exp_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    double value, result;
    
    // Check "value" parameter (handles NULL and double types)
    switch( sqlite3_value_type( argv[0] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            value = sqlite3_value_double( argv[0] );
    }
    
    try
    {
        result = exp( value );
    }
    catch( ... )
    {
        sqlite3_result_error( ctx, "exp(): evaluation error", -1 );
        return;
    }
    
    sqlite3_result_double( ctx, result );
}


/**
 * \brief Regular expression function implementation
 *
 * Regular expression function ( with replace option, then argc > 2 )\n
 * regex(str,pattern) returns the matching substring of str, which
 * matches against pattern, where str is argv[0] and pattern is argv[1].
 *
 * If 3 arguments passed regex(str,pattern,replacement) the substring
 * will be modified regarding replacement parameter before returned.
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void regex_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc >= 2 ); // at least 2 arguments needed
    char *str = NULL, *pattern = NULL, *replace = NULL;
    
    sqlite3_result_null( ctx );
    
    // Get input arguments
    str = utils_strnewdup( (const char*)sqlite3_value_text( argv[0] ), g_convertUTF8 );
    pattern = utils_strnewdup( (const char*)sqlite3_value_text( argv[1] ), g_convertUTF8 );
    
    HC_NOTES( str, "regex_func" );
    HC_NOTES( pattern, "regex_func" );
    
    // Optional 3rd parameter is the replacement pattern
    if( argc > 2 )
    {
        replace = utils_strnewdup( (const char*)sqlite3_value_text( argv[2] ), g_convertUTF8 );
        HC_NOTES( replace, "regex_func" );
    }
    
    CRegexpT <char> regexp( pattern );

    // find and match
    MatchResult result = regexp.Match( str );

    // result
    if( result.IsMatched() )
    {
        char *str_value = NULL;
        
        if( argc == 2 )
        {
            // Match mode
            int start = result.GetStart();  // first match position (0 based)
            int end   = result.GetEnd();    // position afterwards matching substring (0 based)
            int len   = end - start;

            str_value = (char*)MEM_ALLOC( len + 1, sizeof(char) );
            
            // make a substring copy
            if( str_value && len > 0 )
            {
                memset( str_value, 0, len + 1 );
                strncpy( str_value, &str[start], len );
            }
        }
        else
        {
            // Replace mode (allocates space)
            char* result = regexp.Replace( str, replace );
            
            // make a copy with own memory management
            if( result )
            {
                int len = (int)strlen( result );
                str_value = (char*)MEM_ALLOC( len + 1, sizeof(char) );
                
                if( str_value && len )
                {
                    strcpy( str_value, result );
                }
                
                CRegexpT<char>::ReleaseString( result );
            }
        }
        
        // str_value holds substring now, if any (otherwise NULL)
        
        // Optionally convert result string to UTF
        if( str_value && g_convertUTF8 )
        {
            int len = utils_latin2utf( (unsigned char*)str_value, NULL ); // get the size only
            char *temp = (char*)MEM_ALLOC( len, sizeof(char) ); // allocate memory
            if( temp )
            {
                ::utils_latin2utf( (unsigned char*)str_value, (unsigned char*)temp );
                ::utils_free_ptr( str_value );
                str_value = temp;
            }
        }
        
        // Return a string copy and delete the original
        if( str_value )
        {
            sqlite3_result_text( ctx, str_value, -1, SQLITE_TRANSIENT );
            ::utils_free_ptr( str_value );
        }
    }
   
    if( str )
    {
        ::utils_free_ptr( str );
    }
    
    if( pattern )
    {
        ::utils_free_ptr( pattern );
    }
    
    if( replace )
    {
        ::utils_free_ptr( replace );
    }
}


/**
 * \brief MD5 hashing implementation
 *
 * md5(value) calculates the md5 hash (RSA), where value is argv[0]
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void MD5_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    // two versions of typed header will be tested
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    tbhv1_t* tbh1 = NULL;
    tbhv2_t* tbh2 = NULL;
    
    MD5_CTX md5_ctx; // md5 context
    unsigned char digest[16];
    char* str_result = NULL;
    const char hex_chars[] = "0123456789ABCDEF";
    
    sqlite3_result_null( ctx );
    
    if( TBH_endian[0] != 'L' )
    {
        sqlite3_result_error( ctx, "MD5(): implementation for litte endian only!", -1 );
        return;
    }
    
    // get and handle argument "value"
    switch( sqlite3_value_type( argv[0] ) )
    {
      case SQLITE_INTEGER:
      {
          int bytes = sqlite3_value_bytes( argv[0] );
          sqlite3_int64 value = (long)sqlite3_value_int64( argv[0] );
          
          MD5_Init( &md5_ctx );
          MD5_Update( &md5_ctx, &value, bytes );
          MD5_Final( digest, &md5_ctx );
          
          break;
      }
      case SQLITE_FLOAT:
      {
          int bytes = sqlite3_value_bytes( argv[0] );
          double value = (double)sqlite3_value_double( argv[0] );
          
          MD5_Init( &md5_ctx );
          MD5_Update( &md5_ctx, &value, sizeof( double ) );
          MD5_Final( digest, &md5_ctx );
          
          break;
      }
      case SQLITE_TEXT:
      {
          int bytes = sqlite3_value_bytes( argv[0] );
          char* value = utils_strnewdup( (const char*)sqlite3_value_text( argv[0] ), g_convertUTF8 );
          assert( NULL != value );
          
          HC_NOTES( value, "MD5_func" );

          MD5_Init( &md5_ctx );
          MD5_Update( &md5_ctx, value, (int)strlen( value ) );
          MD5_Final( digest, &md5_ctx );

          ::utils_free_ptr( value );
          break;
      }
      case SQLITE_BLOB:
      {
          int bytes = sqlite3_value_bytes( argv[0] );
          tbhv1_t* tbh1 = (tbhv1_t*)sqlite3_value_blob( argv[0] );
          tbhv2_t* tbh2 = (tbhv2_t*)sqlite3_value_blob( argv[0] );
          
          /* No typed header? Use raw blob data then */
          if( !tbh1->validMagic() )
          {
              MD5_Init( &md5_ctx );
              MD5_Update( &md5_ctx, (void*)tbh1, bytes );
              MD5_Final( digest, &md5_ctx );
              break;
          }
          
          /* uncompressed typed header? */
          if( tbh1->validVer() )
          {
              MD5_Init( &md5_ctx );
              MD5_Update( &md5_ctx, tbh1->getData(), (int)(bytes - tbh1->dataOffset()) );
              MD5_Final( digest, &md5_ctx );
              break;
          }
          
          /* compressed typed header? Decompress first */
          if( tbh2->validVer() && tbh2->validCompression() )
          {
              mxArray* pItem = NULL;
              double process_time = 0.0, ratio = 0.0;
              
              if( blob_unpack( tbh2, (int)bytes, can_serialize(), &pItem, &process_time, &ratio ) && pItem )
              {
                  size_t data_size = TypedBLOBHeaderBase::getDataSize( pItem );
                  
                  MD5_Init( &md5_ctx );
                  MD5_Update( &md5_ctx, mxGetData( pItem ), (int)data_size );
                  MD5_Final( digest, &md5_ctx );
                  
                  ::utils_destroy_array( pItem );
              }
              break;
          }
      }
      case SQLITE_NULL:
      default:
        return;
    }
    
    // build result string
    str_result = (char*)MEM_ALLOC( 16*2+1, 1 );
    
    if( str_result )
    {
        memset( str_result, 0, 16*2+1 );

        for( int i = 0; i < 16; i++ )
        {
            str_result[2*i+0] = hex_chars[digest[i] >> 4];
            str_result[2*i+1] = hex_chars[digest[i] & 0x0f];
        }
        
        sqlite3_result_text( ctx, str_result, -1, SQLITE_TRANSIENT );
        ::utils_free_ptr( str_result );
    }
}


/**
 * \brief BDCRatio function implementation
 *
 * BDCRatio(value) calculates the compression ratio 
 * for a blob, where value is argv[0]
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void BDC_ratio_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    // two versions of typed header will be tested
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    // get and handle "value"
    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv1_t* tbh1    = (tbhv1_t*)sqlite3_value_blob( argv[0] );
        tbhv2_t* tbh2    = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        size_t blob_size = (size_t)sqlite3_value_bytes( argv[0] );
        double ratio     = 0.0;
        
        // omit ratio of 1, if blob is type V1 (uncompressed)
        if( tbh1 && tbh1->validMagic() && tbh1->validVer() )
        {
            sqlite3_result_double( ctx, 1.0 );
        }
        else if( tbh2 && tbh2->validMagic() && tbh2->validVer() && tbh2->validCompression() )
        {
            mxArray* pItem = NULL;
            double process_time = 0.0;
            
            if( !blob_unpack( (void*)tbh2, blob_size, can_serialize(), &pItem, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while unpacking occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, ratio );
            }
            
            ::utils_destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCRatio(): only BLOB type supported!", -1 );
    }
}


/**
 * \brief BDCPackTime function implementation
 *
 * BDCPackTime(value) calculates the compression time on a blob,
 * where value is argv[0]
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void BDC_pack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    // two versions of typed header will be tested
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    // get and handle "value" argument
    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv1_t* tbh1       = (tbhv1_t*)sqlite3_value_blob( argv[0] );
        tbhv2_t* tbh2       = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        size_t blob_size    = (size_t)sqlite3_value_bytes( argv[0] );
        double process_time = 0.0;
        double ratio        = 0.0;

        sqlite3_result_null( ctx );
        
        // omit process time of zero, if blob is type V1 (uncompressed)
        if( tbh1 && tbh1->validMagic() && tbh1->validVer() )
        {
            sqlite3_result_double( ctx, 0.0 );
        } 
        else if( tbh2 && tbh2->validMagic() && tbh2->validVer() && tbh2->validCompression() )
        {
            mxArray*  pItem             = NULL;
            void*     dummy_blob        = NULL;
            size_t    dummy_blob_size   = 0;
            double    process_time      = 0.0;
            
            if( !blob_unpack( (void*)tbh2,  blob_size, can_serialize(), &pItem, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while unpacking occured!", -1 );
            }
            else if( !blob_pack( pItem, can_serialize(), &dummy_blob, &dummy_blob_size, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while packing occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, process_time );
            }
            
            sqlite3_free( dummy_blob );
            ::utils_destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCPackTime(): only BLOB type supported!", -1 );
    }
}


/**
 * \brief BDCUnpackTime function implementation
 *
 * BDCUnpackTime(value) calculates the uncompression time on a blob,
 * where value is argv[0]
 *
 * \param[in] ctx SQL context parameter
 * \param[in] argc Argument count
 * \param[in] argv SQL argument values
 */
void BDC_unpack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    // two versions of typed header will be tested
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    // get and handle "value" argument
    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv1_t*  tbh1          = (tbhv1_t*)sqlite3_value_blob( argv[0] );
        tbhv2_t*  tbh2          = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        size_t    blob_size     = (size_t)sqlite3_value_bytes( argv[0] );
        double    process_time  = 0.0;
        double    ratio         = 0.0;
        
        // omit process time of zero, if blob is type V1 (uncompressed)
        if( tbh1 && tbh1->validMagic() && tbh1->validVer() )
        {
            sqlite3_result_double( ctx, 0.0 );
        }
        else if( tbh2 && tbh2->validMagic() && tbh2->validVer() && tbh2->validCompression() )
        {
            mxArray *pItem = NULL;;

            if( !blob_unpack( (void*)tbh2, blob_size, can_serialize(), &pItem, &process_time, &ratio ) )
            {
              sqlite3_result_error( ctx, "BDCUnpackTime(): an error while unpacking occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, process_time );
            }

            ::utils_destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCUnpackTime(): only BLOB type supported!", -1 );
    }
}





/*
 * Functions for BLOB handling (compression and typing)
 */

/**
 * @brief Free memory allocated for a BLOB
 */
void blob_free( void** ppBlob )
{
    if( ppBlob )
    {
        sqlite3_free( *ppBlob );
        *ppBlob = NULL;
    }
}


/**
 * \brief create a compressed typed blob from a Matlab item (deep copy)
 *
 * \param[in] pcItem MATLAB array to compress
 * \param[in] bStreamable if true, streaming preprocess is intended
 * \param[out] ppBlob Created BLOB, allocated by sqlite3_malloc
 * \param[out] pBlob_size Size of BLOB in bytes
 * \param[out] pdProcess_time Processing time in seconds
 * \param[out] pdRatio Realized compression ratio
 * \param[in] compressor name of compressor to use (optional). 
 *            Default is global setting g_compression_type
 * \param[in] level compression level (optional). 
 *            Default is global setting g_compression_level
 */
int blob_pack( const mxArray* pcItem, bool bStreamable, 
               void** ppBlob, size_t* pBlob_size, 
               double *pdProcess_time, double* pdRatio,
               const char* compressor, int level )
{
    Err err;
    
    assert( pcItem && ppBlob && pBlob_size && pdProcess_time && pdRatio );
    
    ValueMex          value( pcItem );           // object wrapper
    mxArray*          byteStream        = NULL;  // for stream preprocessing
    NumberCompressor  numericSequence;           // compressor
    
    // BLOB packaging in 3 steps:
    // 1. Serialize
    // 2. Compress
    // 3. Encapsulate (typed BLOBs)
    
    if( value.Complexity() == ValueMex::TC_COMPLEX )
    {
        if( !bStreamable || !serialize( pcItem, byteStream ) )
        {
            err.set( MSG_ERRMEMORY );  /// \todo MSG_ERRSERIALIZE
            goto finalize;
        }
        
        // inherit new byte stream instead original array
        value = ValueMex( byteStream );
    }
    
    
    *ppBlob         = NULL;
    *pBlob_size     = 0;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    /* 
     * create a typed blob. Header information is generated
     * according to value and type of the matrix and the machine
     */
    // setCompressor() always returns true, since parameters had been checked already
    (void)numericSequence.setCompressor( compressor, level );
    
    // only if compression is desired
    if( g_compression_level )
    {
        double start_time = utils_get_wall_time();
        
        numericSequence.pack( value.Data(), value.ByData(), value.ByElement(), 
                              value.IsDoubleClass() );  // allocates m_rdata
        
        *pdProcess_time = utils_get_wall_time() - start_time;
        
        // any compressed data omitted?
        if( numericSequence.m_result_size > 0 )
        {
            size_t blob_size_uncompressed;
            
            *pBlob_size = 
                TypedBLOBHeaderV2::dataOffset( value.NumDims() ) +
                numericSequence.m_result_size;
            
            blob_size_uncompressed =
                TypedBLOBHeaderV1::dataOffset( value.NumDims() ) +
                value.ByData();
            
            assert( blob_size_uncompressed != 0 );
            
            // calculate the compression ratio
            *pdRatio = (double)*pBlob_size / blob_size_uncompressed;
            
            if( *pBlob_size >= blob_size_uncompressed )
            {
                // Switch zu uncompressed blob, it's not worth the efford.
                numericSequence.free_result();
            }
        }

        // still use compressed data to store in the blob?
        if( numericSequence.m_result_size >  0 )
        {
            TypedBLOBHeaderV2* tbh2 = NULL;
            
            // discard data if it exeeds max allowd size by sqlite
            if( *pBlob_size > CONFIG_MKSQLITE_MAX_BLOB_SIZE )
            {
                err.set( MSG_BLOBTOOBIG );
                goto finalize;
            }

            // allocate space for a typed blob containing compressed data
            tbh2 = (TypedBLOBHeaderV2*)sqlite3_malloc( (int)*pBlob_size );
            if( NULL == tbh2 )
            {
                err.set( MSG_ERRMEMORY );
                goto finalize;
            }

            // blob typing...
            tbh2->init( value.Item() );
            tbh2->setCompressor( numericSequence.getCompressorName() );

            // ...and copy compressed data
            /// \todo Do byteswapping here if big endian? 
            // (Most platforms use little endian)
            memcpy( (char*)tbh2->getData(), numericSequence.m_result, numericSequence.m_result_size );
            
            // optionally check if compressed data equals to original?
            if( g_compression_check && !numericSequence.isLossy() )
            {
                mxArray* unpacked = NULL;
                bool is_equal = false;
                double dummy;

                // inflate compressed data again
                if( !blob_unpack( (void*)tbh2, (int)*pBlob_size, bStreamable, &unpacked, &dummy, &dummy ) )
                {
                    sqlite3_free( tbh2 );
                    
                    err.set( MSG_ERRCOMPRESSION );
                    goto finalize;
                }

                is_equal = ( memcmp( value.Data(), ValueMex(unpacked).Data(), value.ByData() ) == 0 );
                ::utils_destroy_array( unpacked );

                // check if uncompressed data equals original
                if( !is_equal )
                {
                    sqlite3_free( tbh2 );
                    
                    err.set( MSG_ERRCOMPRESSION );
                    goto finalize;
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
        *pBlob_size = TypedBLOBHeaderV1::dataOffset( value.NumDims() ) + value.ByData();

        if( *pBlob_size > CONFIG_MKSQLITE_MAX_BLOB_SIZE )
        {
            err.set( MSG_BLOBTOOBIG );
            goto finalize;
        }

        tbh1 = (TypedBLOBHeaderV1*)sqlite3_malloc( (int)*pBlob_size );
        if( NULL == tbh1 )
        {
            err.set( MSG_ERRMEMORY );
            goto finalize;
        }

        // blob typing...
        tbh1->init( value.Item() );

        // and copy uncompressed data
        /// \todo Do byteswapping here if big endian? 
        // (Most platforms use little endian)
        memcpy( tbh1->getData(), value.Data(), value.ByData() );

        *ppBlob = (void*)tbh1;
    }
    
    // mark data type as "unknown", means that it holds a serialized item as byte stream
    if( byteStream )
    {
        ((TypedBLOBHeaderV1*)*ppBlob)->m_clsid = mxUNKNOWN_CLASS;
    }
    
finalize:
  
    // cleanup
    // free cdata and byteStream if left any
    ::utils_destroy_array( byteStream );
    
    return err.getMsgId();
}


/**
 * \brief uncompress a typed blob and return as MATLAB array
 *
 * \param[out] pBlob BLOB to decompress
 * \param[out] blob_size Size of BLOB in bytes
 * \param[in] bStreamable if true, streaming preprocess is intended
 * \param[in] ppItem MATLAB array to compress
 * \param[out] pdProcess_time Processing time in seconds
 * \param[out] pdRatio Realized compression ratio
 */
int blob_unpack( const void* pBlob, size_t blob_size, bool bStreamable, 
                 mxArray** ppItem, 
                 double* pdProcess_time, double* pdRatio )
{
    Err err;
    bool bIsByteStream = false;
    
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    mxArray* pItem = NULL;
    NumberCompressor numericSequence;

    assert( NULL != ppItem && NULL != pdProcess_time && NULL != pdRatio );
    
    *ppItem         = NULL;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    tbhv1_t* tbh1 = (tbhv1_t*)pBlob;
    tbhv2_t* tbh2 = (tbhv2_t*)pBlob;
    
    /* test valid platform */
    if( !tbh1->validPlatform() )
    {
        mexWarnMsgIdAndTxt( "MATLAB:MKSQLITE:BlobDiffArch", ::getLocaleMsg( MSG_WARNDIFFARCH ) );
        /// \todo warning, error or automatic conversion..?
        // since mostly platforms (except SunOS) use LE encoding
        // and unicode is not supported here, there is IMHO no need 
        // for conversions...
        
        // Warning can switched off via:
        // warning( 'off', 'MATLAB:MKSQLITE:BlobDiffArch' );
    }

    /* check for valid header */
    if( !tbh1->validMagic() )
    {
        err.set( MSG_UNSUPPTBH );
        goto finalize;
    }

    // serialized array marked as "unknown class" is a byte stream
    if( tbh1->m_clsid == mxUNKNOWN_CLASS )
    {
        bIsByteStream = true;
        tbh1->m_clsid = mxUINT8_CLASS;
    }

    switch( tbh1->m_ver )
    {
      // typed blob with uncompressed data
      case sizeof( tbhv1_t ):
      {
          // get data from header "type 1" is easy
          pItem = tbh1->createNumericArray( /* doCopyData */ true );
          break;
      }

      // typed blob with compressed data
      case sizeof( tbhv2_t ):
      {
          if( !tbh2->validCompression() )
          {
              err.set( MSG_UNKCOMPRESSOR );
              goto finalize;
          }
          
          // create an empty MATLAB array
          pItem = tbh2->createNumericArray( /* doCopyData */ false );
          
          // space allocated?
          if( pItem )
          {
              numericSequence.setCompressor( tbh2->m_compression );

              double start_time = utils_get_wall_time();
              void*  cdata      = tbh2->getData();  // get compressed data
              size_t cdata_size = blob_size - tbh2->dataOffset(); // and its size
              
              // data will be unpacked directly into MATLAB variable data space
              if( !numericSequence.unpack( cdata, cdata_size, ValueMex(pItem).Data(), ValueMex(pItem).ByData(), ValueMex(pItem).ByElement() ) )
              {
                  err.set( MSG_ERRCOMPRESSION );
                  goto finalize;
              } 
              
              *pdProcess_time = utils_get_wall_time() - start_time;

              // any data omitted?
              if( ValueMex(pItem).ByData() > 0 )
              {
                  *pdRatio = (double)cdata_size / numericSequence.m_result_size;
              }
              else
              {
                  *pdRatio = 0.0;
              }

              /// \todo Do byteswapping here if needed, depend on endian?
          }
          break;
      }

      default:
          err.set( MSG_UNSUPPTBH );
          goto finalize;
    }


    // revert if streaming preprocess was done
    if( bIsByteStream  )
    {
        mxArray* pDeStreamed = NULL;
        
        if( !deserialize( pItem, pDeStreamed ) )
        {
            err.set( MSG_ERRMEMORY );
            goto finalize;
        }
        
        ::utils_destroy_array( pItem );
        pItem = pDeStreamed;
    }

    *ppItem = pItem;
    pItem = NULL;
      
finalize:
    
    // cleanup
    // rdata is owned by a MATLAB variable (will be freed by MATLAB)
    // cdata is owned by the blob (const parameter pBlob)
    // so inhibit from freeing through destructor:
    ::utils_destroy_array( pItem );

    return err.getMsgId();
}


#endif
