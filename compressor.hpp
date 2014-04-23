/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

#pragma once

/*
 * The compressor squeezes the data from a MATLAB variable using one of 
 * the available packing algorithms (blosc, lc4, qlin16, qlog16).
 * qlin16 and qlog16 are lossy compression algorithms.
 * The compressor accepts only types known by get_type_complexity(...)
 * that returns TC_UNSUPP otherwise.
 * If supported (see have_serialize()), item can be converted into a byte 
 * stream, before compression.
 */

extern "C"
{
  #include "blosc/blosc.h"
}
#include "serialize.hpp"
#include "locale.hpp"

#define BLOSC_LZ4_ID            BLOSC_LZ4_COMPNAME
#define BLOSC_LZ4HC_ID          BLOSC_LZ4HC_COMPNAME
#define BLOSC_DEFAULT_ID        BLOSC_BLOSCLZ_COMPNAME
#define QLIN16_ID               "QLIN16"
#define QLOG16_ID               "QLOG16"

/* Which compression method is to use, if its name is empty */
#define COMPRESSOR_DEFAULT_ID   BLOSC_DEFAULT_ID



/* compressor class */

class Compressor 
{
public:
    // categories
    typedef enum
    {
        CT_NONE = 0,
        CT_BLOSC,
        CT_QLIN16,
        CT_QLOG16,
    } compressor_type_e;
    
    /* Complexity information about a MATLAB variable.
     * For testing if a variable is able to be packed or not.
     */
    typedef enum {
        TC_EMPTY = 0,       // Empty
        TC_SIMPLE,          // 1-byte non-complex scalar, char or simple string (SQLite simple types)
        TC_SIMPLE_VECTOR,   // non-complex numeric vectors (SQLite BLOB)
        TC_SIMPLE_ARRAY,    // multidimensional non-complex numeric or char arrays (SQLite typed BLOB)
        TC_COMPLEX,         // structs, cells, complex data (SQLite typed ByteStream BLOB)
        TC_UNSUPP = -1      // all other (unsuppored types)
    } type_complexity_e;
    
    const char*             m_strCompressorType;  // name of compressor to use
    compressor_type_e       m_eCompressorType;    // enum type of compressor to use
    int                     m_iCompressionLevel;  // compression level (0 to 9)
    mxArray*                m_pItem;              // (a copy of) data to compress
    bool                    m_bIsByteStream;      // flag, true if it's a byte stream
    size_t                  m_szElement;          // size of one element in bytes
    size_t                  m_cntElements;        // number of elements
    mwSize                  m_nDims;              // number of data dimensions
    void*                   m_rdata;              // raw data, pointing tp pItemCopy data
    size_t                  m_rdata_size;         // size of raw data in bytes
    void*                   m_cdata;              // compressed data for blob storage
    size_t                  m_cdata_size;         // size of compressed data in bytes  
    bool                    m_bStreamable;        // flag, true if typed blobs mode is TYBLOB_BYSTREAM

private:
    // inhibit copy constructor and assignment operator
    Compressor( const Compressor& );
    Compressor& operator=( const Compressor& );
    
public:
    // constructor
    explicit
    Compressor( bool bStreamable )
    {
        m_strCompressorType   = COMPRESSOR_DEFAULT_ID;
        m_eCompressorType     = CT_NONE;
        m_iCompressionLevel   = 0;
        m_pItem               = NULL;
        m_bIsByteStream       = false;
        m_szElement           = 0;
        m_cntElements         = 0;
        m_nDims               = 0;
        m_rdata               = NULL;
        m_rdata_size          = 0;
        m_cdata               = NULL;
        m_cdata_size          = 0;
        m_bStreamable         = bStreamable;
    }
    
    
    // destructor
    ~Compressor()
    {
        // memory allocation is done outside of this class!
        // reason: rdata may point to const data of m_pItem
        assert( NULL == m_pItem && NULL == m_cdata && NULL == m_rdata );
    }
    
    
    // converts compressor id string to enum category
    bool setCompressor( const char *strCompressorType, int iCompressionLevel = -1 )
    {
        if( 0 == iCompressionLevel || NULL == strCompressorType || !*strCompressorType )
        {
            m_eCompressorType   = CT_NONE;
            m_strCompressorType = NULL;
            m_iCompressionLevel = 0;
        }
        else
        {
            if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4_ID ) )
            {
                m_eCompressorType = CT_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4HC_ID ) )
            {
                m_eCompressorType = CT_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, BLOSC_DEFAULT_ID ) )
            {
                m_eCompressorType = CT_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, QLIN16_ID ) )
            {
                m_eCompressorType = CT_QLIN16;
            }
            else if( 0 == _strcmpi( strCompressorType, QLOG16_ID ) )
            {
                m_eCompressorType = CT_QLOG16;
            } 
            else
            {
                m_strCompressorType = NULL;
                return false;
            }
            
            m_strCompressorType = strCompressorType;
            m_iCompressionLevel = iCompressionLevel;
        }

        if( m_eCompressorType == CT_BLOSC )
        {
            blosc_set_compressor( m_strCompressorType );
        }
        
        return true;
    }
    
    
    // adopt a Matlab variable (item) and its properties
    bool attachItem( mxArray* pItem )
    {
        m_pItem = pItem;
        
        // save item properties
        if( NULL != m_pItem )
        {
            m_szElement   = mxGetElementSize( m_pItem );        
            m_cntElements = mxGetNumberOfElements( m_pItem );   
            m_nDims       = mxGetNumberOfDimensions( m_pItem ); 
            m_rdata       = mxGetData( m_pItem );
            m_rdata_size  = m_cntElements * m_szElement;              
        }
        else
        {
            m_szElement   = 0;
            m_cntElements = 0;
            m_nDims       = 0;
            m_rdata       = NULL;
            m_rdata_size  = 0;
        }
        
        return true;
    }
      
    
    // create a copy of an item, so that is's serializable
    bool attachStreamableCopy( mxArray* pItem )
    {
        assert( NULL != pItem && NULL == m_pItem );

        m_bIsByteStream = false;

        // structural data must be converted into byte stream
        if( TC_COMPLEX == get_type_complexity( pItem, m_bStreamable ) )
        {
            mxArray* pByteStream = NULL;

            // typed BLOBs only support numeric (non-complex) arrays or strings,
            // but if the user allows undocumented ByteStreams, they can be stored 
            // either.
            if( m_bStreamable )
            {
                serialize( pItem, pByteStream );
            }

            if( !pByteStream )
            {
                SETERR( getMsg( MSG_UNSUPPVARTYPE ) );
                return false;
            }

            attachItem( pByteStream );
            m_bIsByteStream = true;
        } 
        else 
        {
            // data type itself is streamable, duplicate only
            attachItem( mxDuplicateArray( pItem ) );
        }
        
        return NULL != m_pItem;
    }
    
    
    // allocates m_cdata and use it to store compressed data from m_rdata
    // (lossless data compression)
    bool bloscCompress()
    {
        assert( NULL == m_cdata && NULL != m_rdata );
        
        // BLOSC grants for compressed data never 
        // exceeds original size + BLOSC_MAX_OVERHEAD
        m_cdata_size  = m_rdata_size + BLOSC_MAX_OVERHEAD; 
        m_cdata       = mxMalloc( m_cdata_size );

        if( NULL == m_cdata )
        {
            SETERR( getMsg( MSG_ERRMEMORY ) );
            return false;
        }

        /* compress raw data (rdata) and store it in cdata */
        m_cdata_size = blosc_compress( 
          /*clevel*/     m_iCompressionLevel, 
          /*doshuffle*/  BLOSC_DOSHUFFLE, 
          /*typesize*/   m_szElement, 
          /*nbytes*/     m_rdata_size, 
          /*src*/        m_rdata, 
          /*dest*/       m_cdata, 
          /*destsize*/   m_cdata_size );
        
        return NULL != m_cdata;
    }
    
    
    // uncompress compressed data m_cdata to data m_rdata
    // m_rdata must point to writable data of item m_pItem
    bool bloscDecompress()
    {
        assert( NULL != m_cdata && NULL != m_pItem && NULL != m_rdata );
        
        size_t blosc_nbytes, blosc_cbytes, blosc_blocksize; 
        
        blosc_cbuffer_sizes( m_cdata, &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );
        
        // uncompressed data must fit into item
//        if( blosc_nbytes != TypedBLOBHeaderBase::getDataSize( m_pItem ) )
        if( blosc_nbytes != get_data_size( m_pItem ) )
        {
            SETERR( getMsg( MSG_ERRCOMPRESSION ) );
            return false;
        }
        
        // decompress directly into item
        if( blosc_decompress( m_cdata, m_rdata, m_rdata_size ) <= 0 )
        {
            SETERR( getMsg( MSG_ERRCOMPRESSION ) );
            return false;
        } 
        
        return true;
    }
    
    
    // lossy data compression by linear or logarithmic quantization (16 bit)
    bool linlogQuantizerCompress( bool bDoLog )
    {
        assert( NULL == m_cdata && NULL != m_pItem && NULL != m_rdata );
        
        double dOffset = 0.0, dScale = 1.0;
        double dMinVal, dMaxVal;
        bool bMinValSet = false, bMaxValSet = false;
        double* rdata = (double*)m_rdata;
        float* pFloatData;
        uint16_t* pUintData;
        
        // compressor works for double type only
        if( mxDOUBLE_CLASS != mxGetClassID( m_pItem ) )
        {
            SETERR( getMsg( MSG_ERRCOMPRARG ) );
            return false;
        }
        
        // seek data bounds for quantization
        for( size_t i = 0; i < m_cntElements; i++ )
        {
            if( mxIsFinite( rdata[i] ) && rdata[i] != 0.0 )
            {
                if( !bMinValSet || rdata[i] < dMinVal )
                {
                    dMinVal = rdata[i];
                    bMinValSet = true;
                }
                
                if( !bMaxValSet || rdata[i] > dMaxVal )
                {
                    dMaxVal = rdata[i];
                    bMaxValSet = true;
                }
            }
        }

        // in logarithmic mode, no negative values are allowed
        if( bDoLog && dMinVal < 0.0 )
        {
            SETERR( getMsg( MSG_ERRCOMPRLOGMINVALS ) );
            return false;
        }

        // compressor converts each value to uint16_t
        // 2 additional floats for offset and scale
        m_cdata_size = 2 * sizeof( float ) + m_cntElements * sizeof( uint16_t );  
        m_cdata      = mxMalloc( m_cdata_size );

        if( NULL == m_cdata )
        {
            SETERR( getMsg( MSG_ERRMEMORY ) );
            return false;
        }
        
        pFloatData   = (float*)m_cdata;
        pUintData    = (uint16_t*)&pFloatData[2];

        // calculate offset information
        if( bMinValSet )
        {
            dOffset = bDoLog ? log( dMinVal ) : dMinVal;
        }

        // calculate scale information
        if( bMaxValSet )
        {
            double dValue = bDoLog ? log( dMaxVal ) : dMaxVal;

            // data is mapped on 65529 (0xFFF8u) levels
            dScale  = ( dValue - dOffset ) / 0xFFF8u;

            // if dMaxValue == dMinValue, scale would be set to zero.
            // to avoid division by zero on decompression, it is set to 1.0 here.
            // this doesn't affect the result (0/1 = 0)
            if( dScale == 0.0 )
            {
                dScale = 1.0;
            }
        }

        // store offset and scale information for decompression
        pFloatData[0] = (float)dOffset;
        pFloatData[1] = (float)dScale;
        
        // quantization
        for( size_t i = 0; i < m_cntElements; i++ )
        {
            // non-finite values and zero are mapped to special values
            if( mxIsFinite( rdata[i] ) && rdata[i] != 0.0 )
            {
                double dValue = bDoLog ? log( rdata[i] ) : rdata[i];

                *pUintData++ = (uint16_t) ( (dValue - dOffset ) / dScale ) & 0xFFF8u;
            } 
            else
            {
                // special values for zero, infinity and nan
                if( fabs( rdata[i] ) == 0.0 )
                {
                    *pUintData++ = 0xFFF8u + 1 + ( copysign( 1.0, rdata[i] ) < 0.0 );
                }
                else if( mxIsInf( rdata[i] ) )
                {
                    *pUintData++ = 0xFFF8u + 3 + ( copysign( 1.0, rdata[i] ) < 0.0 );
                }
                else if( mxIsNaN( rdata[i] ) )
                {
                    *pUintData++ = 0xFFF8u + 5;
                }
            }
        }
        
        return true;
    }
    
    
    // lossy data compression by linear or logarithmic quantization (16 bit)
    bool linlogQuantizerDecompress( bool bDoLog )
    {
        assert( NULL != m_cdata && NULL != m_pItem && NULL != m_rdata );
        
        double dOffset = 0.0, dScale = 1.0;
        double* rdata = (double*)m_rdata;
        float* pFloatData = (float*)m_cdata;
        uint16_t* pUintData = (uint16_t*)&pFloatData[2];
        
        // compressor works for double type only
        if( mxDOUBLE_CLASS != mxGetClassID( m_pItem ) )
        {
            SETERR( getMsg( MSG_ERRCOMPRARG ) );
            return false;
        }
        
        // restore offset and scale information
        dOffset = pFloatData[0];
        dScale  = pFloatData[1];
        
        // rescale values to its originals
        for( size_t i = 0; i < m_cntElements; i++ )
        {
            if( *pUintData > 0xFFF8u )
            {
                // handle special values for zero, infinity and nan
                switch( *pUintData - 0xFFF8u )
                {
                    case 1: *rdata = +0.0;        break;
                    case 2: *rdata = -0.0;        break;
                    case 3: *rdata = +mxGetInf(); break;
                    case 4: *rdata = -mxGetInf(); break;
                    case 5: *rdata = +mxGetNaN(); break;
                }

                pUintData++;
                rdata++;
            }
            else
            {
                // all other values are rescaled due to offset and scale
                if( bDoLog )
                {
                    *rdata++ = exp( (double)*pUintData++ * dScale + dOffset );
                }
                else
                {
                    *rdata++ = (double)*pUintData++ * dScale + dOffset;
                }
            }
        }
        
        return true;
    }
    
    
    // calls the qualified compressor (deflate)
    bool pack()
    {
        switch( m_eCompressorType )
        {
          case CT_BLOSC:
            return bloscCompress();
            
          case CT_QLIN16:
            return linlogQuantizerCompress( /* bDoLog*/ false );
            
          case CT_QLOG16:
            return linlogQuantizerCompress( /* bDoLog*/ true );
            
          default:
            return false;
        }
    }
    
    
    // calls the qualified compressor (inflate)
    bool unpack()
    {
        switch( m_eCompressorType )
        {
          case CT_BLOSC:
            return bloscDecompress();
            
          case CT_QLIN16:
            return linlogQuantizerDecompress( /* bDoLog*/ false );
            
          case CT_QLOG16:
            return linlogQuantizerDecompress( /* bDoLog*/ true );
            
          default:
            return false;
        }
    }
    
    
    // check if item is scalar (any dimension must be 1)
    static bool is_scalar( const mxArray* item )
    {
        assert( item );
        return mxGetNumberOfElements( item ) == 1;
    }
    
    
    // check if item is a vector (one dimension must be 1)
    static bool is_vector( const mxArray* item )
    {
        assert( item );
        return    mxGetNumberOfDimensions( item ) == 2
               && min( mxGetM( item ), mxGetN( item ) ) == 1;
    }
    
    
    // get the complexitity of an item (salar, vector, ...)
    // (to consider how to pack)
    // Returns TC_UNSUPP, if the Compressor can't hande the item type.
    // If bStreamable is true, the Compressor is allowed to convert the
    // item data into a byte stream, before compression.
    static type_complexity_e get_type_complexity( const mxArray* pItem, bool bStreamable )
    {
        assert( pItem );
        mxClassID clsid = mxGetClassID( pItem );

        if( mxIsEmpty( pItem ) ) return TC_EMPTY;

        switch( clsid )
        {
            case  mxDOUBLE_CLASS:
            case  mxSINGLE_CLASS:
                if( mxIsComplex( pItem ) )
                {
                    return TC_COMPLEX;
                }
                /* fallthrough */
            case mxLOGICAL_CLASS:
            case    mxINT8_CLASS:
            case   mxUINT8_CLASS:
            case   mxINT16_CLASS:
            case  mxUINT16_CLASS:
            case   mxINT32_CLASS:
            case  mxUINT32_CLASS:
            case   mxINT64_CLASS:
            case  mxUINT64_CLASS:
                if( is_scalar( pItem ) ) return TC_SIMPLE;
                return is_vector( pItem ) ? TC_SIMPLE_VECTOR : TC_SIMPLE_ARRAY;
            case    mxCHAR_CLASS:
                return ( is_scalar( pItem) || is_vector( pItem ) ) ? TC_SIMPLE : TC_SIMPLE_ARRAY;
            case mxUNKNOWN_CLASS:
                // serialized data marked as "unknown" type
                return bStreamable ? TC_COMPLEX : TC_UNSUPP;
            case  mxSTRUCT_CLASS:
            case    mxCELL_CLASS:
                return TC_COMPLEX;
            default:
                return TC_UNSUPP;
        }
    }
};
