/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      sql_user_functions.hpp
 *  @brief     SQL user defined functions, automatically attached to each database
 *  @details   Additional functions in SQL statements (MD5, regex, pow, and packing ratio/time)
 *  @see       http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data
 *  @author    Martin Kortmann <mail@kortmann.de>
 *  @author    Andreas Martin  <andi.martin@gmx.net>
 *  @version   2.0
 *  @date      2008-2014
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

/* Extending SQLite with additional user functions */

#include "config.h"
#include "global.hpp"
#include "sqlite/sqlite3.h"
#include "deelx/deelx.h"
#include "typed_blobs.hpp"
#include "number_compressor.hpp"
#include "serialize.hpp"
#include "utils.hpp"

extern "C"
{
  #include "md5/md5.h"  /* little endian only! */
}

/* SQLite function extensions by mksqlite */
void pow_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void regex_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_ratio_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_pack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_unpack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void MD5_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );


// Forward declarations
int blob_pack( const mxArray* pcItem, bool bStreamable, void** ppBlob, size_t* pBlob_size, double *pdProcess_time, double* pdRatio );
int blob_unpack( const void* pBlob, size_t blob_size, bool bStreamable, mxArray** ppItem, double* pProcess_time, double* pdRatio );


#ifdef MAIN_MODULE

/* sqlite user functions, implementations */

// power function
void pow_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 2 ) ;
    double base, exponent, result;
    switch( sqlite3_value_type( argv[0] ) )
    {
        case SQLITE_NULL:
            sqlite3_result_null( ctx );
            return;
        default:
            base = sqlite3_value_double( argv[0] );
    }
    
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


// regular expression function (with replace option, then argc > 2 )
void regex_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc >= 2 );
    char *str, *pattern, *replace = NULL;
    
    sqlite3_result_null( ctx );
    
    str = utils_strnewdup( (const char*)sqlite3_value_text( argv[0] ) );
    pattern = utils_strnewdup( (const char*)sqlite3_value_text( argv[1] ) );
    
    if( argc > 2 )
    {
        replace = utils_strnewdup( (const char*)sqlite3_value_text( argv[2] ) );
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
            int start = result.GetStart();
            int end   = result.GetEnd();
            int len   = end - start;

            str_value = (char*)MEM_ALLOC( len + 1, sizeof(char) );
            
            if( str_value && len > 0 )
            {
                memset( str_value, 0, len + 1 );
                strncpy( str_value, &str[start], len );
            }
        }
        else
        {
            // Replace mode
            char* result = regexp.Replace( str, replace );
            
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
        
        if( str_value && g_convertUTF8 )
        {
            int len = utils_latin2utf( (unsigned char*)str_value, NULL ); // get the size only
            char *temp = (char*)MEM_ALLOC( len, sizeof(char) ); // allocate memory
            if( temp )
            {
                utils_latin2utf( (unsigned char*)str_value, (unsigned char*)temp );
                utils_free_ptr( str_value );
                str_value = temp;
            }
        }
        
        if( str_value )
        {
            sqlite3_result_text( ctx, str_value, -1, SQLITE_TRANSIENT );
            utils_free_ptr( str_value );
        }
    }
   
    if( str )
    {
        delete[] str;
    }
    
    if( pattern )
    {
        delete[] pattern;
    }
    
    if( replace )
    {
        delete[] replace;
    }
}


// Calculates the md5 hash (RSA)
void MD5_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    tbhv1_t* tbh1 = NULL;
    tbhv2_t* tbh2 = NULL;
    
    MD5_CTX md5_ctx;
    unsigned char digest[16];
    char* str_result = NULL;
    const char hex_chars[] = "0123456789ABCDEF";
    
    sqlite3_result_null( ctx );
    
    if( TBH_endian[0] != 'L' )
    {
        sqlite3_result_error( ctx, "MD5(): implementation for litte endian only!", -1 );
        return;
    }
    
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
          char* value = utils_strnewdup( (const char*)sqlite3_value_text( argv[0] ) );
          assert( NULL != value );

          MD5_Init( &md5_ctx );
          MD5_Update( &md5_ctx, value, (int)strlen( value ) );
          MD5_Final( digest, &md5_ctx );

          delete[] value;
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
                  
                  utils_destroy_array( pItem );
              }
              break;
          }
      }
      case SQLITE_NULL:
      default:
        return;
    }
    
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
        utils_free_ptr( str_result );
    }
}


// BDCRatio function. Calculates the compression ratio for a blob
void BDC_ratio_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

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
            
            utils_destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCRatio(): only BLOB type supported!", -1 );
    }
}


// BDCPackTime function. Calculates the compression time on a blob
void BDC_pack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

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
            utils_destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCPackTime(): only BLOB type supported!", -1 );
    }
}


// BDCUnpackTime function. Calculates the decompression time on a blob
void BDC_unpack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    sqlite3_result_null( ctx );

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

            utils_destroy_array( pItem );
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


// create a compressed typed blob from a Matlab item (deep copy)
int blob_pack( const mxArray* pcItem, bool bStreamable, 
               void** ppBlob, size_t* pBlob_size, 
               double *pdProcess_time, double* pdRatio )
{
    Err err;
    
    assert( pcItem && ppBlob && pBlob_size && pdProcess_time && pdRatio );
    
    ValueMex          value( pcItem );
    mxArray*          byteStream        = NULL;
    NumberCompressor  numericSequence;
    
    // BLOB packaging in 3 steps:
    // 1. Serialize
    // 2. Compress
    // 3. Encapsulate (typed BLOBs)
    
    if( value.Complexity() == ValueMex::TC_COMPLEX )
    {
        if( !bStreamable || !serialize( pcItem, byteStream ) )
        {
            err.set( MSG_ERRMEMORY );  // \todo MSG_ERRSERIALIZE
            goto finalize;
        }
        
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
    (void)numericSequence.setCompressor( g_compression_type, g_compression_level );
    
   
    if( g_compression_level )
    {
        double start_time = utils_get_wall_time();
        
        numericSequence.pack( value.Data(), value.ByData(), value.ByElement(), 
                              value.IsDoubleClass() );  // allocates m_rdata
        
        *pdProcess_time = utils_get_wall_time() - start_time;
        
        // did the compressor ommits compressed data?
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
            // TODO: Do byteswapping here if big endian? 
            // (Most platforms use little endian)
            memcpy( (char*)tbh2->getData(), numericSequence.m_result, numericSequence.m_result_size );
            
            // check if compressed data equals to original?
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
                utils_destroy_array( unpacked );

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
        // TODO: Do byteswapping here if big endian? 
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
    utils_destroy_array( byteStream );
    
    return err.getErrId();
}


// uncompress a typed blob and return its matlab item
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
        err.set( MSG_UNSUPPTBH );
        goto finalize;
    }

    // serialized item marked as "unknown class" is a byte stream
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
          
          // create an empty MATLAB variable
          pItem = tbh2->createNumericArray( /* doCopyData */ false );
          
          // space allocated?
          if( pItem )
          {
              numericSequence.setCompressor( tbh2->m_compression );

              double start_time = utils_get_wall_time();
              void*  cdata      = tbh2->getData();
              size_t cdata_size = blob_size - tbh2->dataOffset();
              
              // data will be unpacked directly into MATLAB variable data space
              if( !numericSequence.unpack( cdata, cdata_size, ValueMex(pItem).Data(), ValueMex(pItem).ByData(), ValueMex(pItem).ByElement() ) )
              {
                  err.set( MSG_ERRCOMPRESSION );
                  goto finalize;
              } 
              
              *pdProcess_time = utils_get_wall_time() - start_time;

              if( ValueMex(pItem).ByData() > 0 )
              {
                  *pdRatio = (double)cdata_size / numericSequence.m_result_size;
              }
              else
              {
                  *pdRatio = 0.0;
              }

              // TODO: Do byteswapping here if needed, depend on endian?
          }
          break;
      }

      default:
          err.set( MSG_UNSUPPTBH );
          goto finalize;
    }


    if( bIsByteStream  )
    {
        mxArray* pDeStreamed = NULL;
        
        if( !deserialize( pItem, pDeStreamed ) )
        {
            err.set( MSG_ERRMEMORY );
            goto finalize;
        }
        
        utils_destroy_array( pItem );
        pItem = pDeStreamed;
    }

    *ppItem = pItem;
    pItem = NULL;
      
finalize:
    
    // cleanup
    // rdata is owned by a MATLAB variable (will be freed by MATLAB)
    // cdata is owned by the blob (const parameter pBlob)
    // so inhibit from freeing through destructor:
    utils_destroy_array( pItem );

    return err.getErrId();
}


#endif
