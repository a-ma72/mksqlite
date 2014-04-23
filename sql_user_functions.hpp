/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

#pragma once

/* Extending SQLite with additional user functions */

#include "config.h"
#include "global.hpp"
#include "sqlite/sqlite3.h"
#include "deelx/deelx.h"
#include "typed_blobs.hpp"
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
    
    str = strnewdup( (const char*)sqlite3_value_text( argv[0] ) );
    pattern = strnewdup( (const char*)sqlite3_value_text( argv[1] ) );
    
    if( argc > 2 )
    {
        replace = strnewdup( (const char*)sqlite3_value_text( argv[2] ) );
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

            str_value = (char*)mxCalloc( len + 1, sizeof(char) );
            
            if( str_value && len > 0 )
            {
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
                str_value = (char*)mxCalloc( len + 1, sizeof(char) );
                
                if( str_value && len )
                {
                    strncpy( str_value, result, len );
                }
                
                CRegexpT<char>::ReleaseString( result );
            }
        }
        
        if( str_value && g_convertUTF8 )
        {
            int len = latin2utf( (unsigned char*)str_value, NULL ); // get the size only
            char *temp = (char*)mxCalloc( len, sizeof(char) ); // allocate memory
            if( temp )
            {
                latin2utf( (unsigned char*)str_value, (unsigned char*)temp );
                free_ptr( str_value );
                str_value = temp;
            }
        }
        
        if( str_value )
        {
            sqlite3_result_text( ctx, str_value, -1, SQLITE_TRANSIENT );
            free_ptr( str_value );
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
          char* value = strnewdup( (const char*)sqlite3_value_text( argv[0] ) );
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
              
              if( blob_unpack( tbh2, (int)bytes, &pItem, &process_time, &ratio ) && pItem )
              {
                  size_t data_size = TypedBLOBHeaderBase::getDataSize( pItem );
                  
                  MD5_Init( &md5_ctx );
                  MD5_Update( &md5_ctx, mxGetData( pItem ), (int)data_size );
                  MD5_Final( digest, &md5_ctx );
                  
                  destroy_array( pItem );
              }
              break;
          }
      }
      case SQLITE_NULL:
      default:
        return;
    }
    
    str_result = (char*)mxCalloc( 16*2+1, 1 );
    
    if( str_result )
    {
        for( int i = 0; i < 16; i++ )
        {
            str_result[2*i+0] = hex_chars[digest[i] >> 4];
            str_result[2*i+1] = hex_chars[digest[i] & 0x0f];
        }
        
        sqlite3_result_text( ctx, str_result, -1, SQLITE_TRANSIENT );
        free_ptr( str_result );
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
            
            if( !blob_unpack( (void*)tbh2, blob_size, &pItem, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while unpacking occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, ratio );
            }
            
            destroy_array( pItem );
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
            mxArray* pItem          = NULL;
            void* dummy_blob        = NULL;
            size_t dummy_blob_size  = 0;
            double process_time     = 0.0;
            
            if( !blob_unpack( (void*)tbh2, blob_size, &pItem, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while unpacking occured!", -1 );
            }
            else if( !blob_pack( pItem, &dummy_blob, &dummy_blob_size, &process_time, &ratio ) )
            {
                sqlite3_result_error( ctx, "BDCRatio(): an error while packing occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, process_time );
            }
            
            sqlite3_free( dummy_blob );
            destroy_array( pItem );
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
        tbhv1_t* tbh1       = (tbhv1_t*)sqlite3_value_blob( argv[0] );
        tbhv2_t* tbh2       = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        size_t blob_size    = (size_t)sqlite3_value_bytes( argv[0] );
        double process_time = 0.0;
        double ratio        = 0.0;
        
        // omit process time of zero, if blob is type V1 (uncompressed)
        if( tbh2 && tbh2->validMagic() && tbh2->validVer() && tbh2->validCompression() )
        {
            sqlite3_result_double( ctx, 0.0 );
        }
        else if( tbh2 && tbh2->validMagic() && tbh2->validVer() && tbh2->validCompression() )
        {
            mxArray *pItem = NULL;;

            if( !blob_unpack( (void*)tbh2, blob_size, &pItem, &process_time, &ratio ) )
            {
              sqlite3_result_error( ctx, "BDCUnpackTime(): an error while unpacking occured!", -1 );
            }
            else
            {
                sqlite3_result_double( ctx, process_time );
            }

            destroy_array( pItem );
        }
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCUnpackTime(): only BLOB type supported!", -1 );
    }
}

#endif