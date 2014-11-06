/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      number_compressor.hpp
 *  @brief     Compression of numeric (real number) arrays
 *  @details   Using "blosc" as lossless compressor and a lossy quantising compressor
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

/**
 * @file
 * The compressor squeezes an array of numeric values using one of 
 * the available packing algorithms (blosc, lc4, qlin16, qlog16).
 * qlin16 and qlog16 are lossy compression algorithms and only available for 
 * values of type double.
 * Although BLOSC is designed to compress doubles, it is allowed to
 * use it with other data then doubles. The QLIN16 and QLOG16
 * algorithms do not!
 */

extern "C"
{
  #include "blosc/blosc.h"
}
#include "locale.hpp"

#define BLOSC_LZ4_ID            BLOSC_LZ4_COMPNAME
#define BLOSC_LZ4HC_ID          BLOSC_LZ4HC_COMPNAME
#define BLOSC_DEFAULT_ID        BLOSC_BLOSCLZ_COMPNAME
#define QLIN16_ID               "QLIN16"
#define QLOG16_ID               "QLOG16"

/// Which compression method is to use, if its name is empty
#define COMPRESSOR_DEFAULT_ID   BLOSC_DEFAULT_ID

/// compressor class
class NumberCompressor 
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
    
    bool                    m_result_is_const;
    void*                   m_result;
    size_t                  m_result_size;
    

private:
    Err                     m_err;                    // recent error
    
    const char*             m_strCompressorType;      // name of compressor to use
    compressor_type_e       m_eCompressorType;        // enum type of compressor to use
    int                     m_iCompressionLevel;      // compression level (0 to 9)
public:
    void*                   m_rdata;                  // uncompressed data
    size_t                  m_rdata_size;             // size of uncompressed data in bytes
    size_t                  m_rdata_element_size;     // size of one element in bytes
    bool                    m_rdata_is_double_type;   // Flag type is mxDOUBLE_CLASS
    void*                   m_cdata;                  // compressed data
    size_t                  m_cdata_size;             // size of compressed data in bytes
private:
    
    void*                   (*m_Allocator)( size_t szBytes );
    void                    (*m_DeAllocator)( void* ptr );

    // inhibit copy constructor and assignment operator
    NumberCompressor( const NumberCompressor& );
    NumberCompressor& operator=( const NumberCompressor& );
    
    
public:
    // constructor
    explicit
    NumberCompressor() : m_result(0)
    {
        m_Allocator   = malloc;
        m_DeAllocator = free;

        // no compression is the default
        setCompressor( COMPRESSOR_DEFAULT_ID, 0 );
        
        clear_data();
        free_result();
    }
    
    
    // clear self created results with deallocation
    void free_result()
    {
        if( m_result && !m_result_is_const )
        {
            m_DeAllocator( m_result );
            m_result            = NULL;
            m_result_size       = 0;
            m_result_is_const   = true;
        }
    }            

    
    // reset input data (compressed and uncompressed)
    // without deallocation!
    void clear_data()
    {
        m_rdata                 = NULL;
        m_rdata_size            = 0;
        m_cdata                 = NULL;
        m_cdata_size            = 0;
        m_rdata_is_double_type  = false;
    }
    
    
    void clear_err()
    {
        m_err.clear();
    }
    

    int get_err()
    {
        return m_err.getErrId();
    }
    
    
    // destructor
    ~NumberCompressor()
    {
        clear_data();
        free_result();
    }
    
    
    void setAllocator( void* (*Allocator)(size_t), void (*DeAllocator)(void*) )
    {
        if( Allocator && DeAllocator )
        {
            m_Allocator   = Allocator;
            m_DeAllocator = DeAllocator;
        }
        else
        {
            assert( false );
        }
    }
    
    
    // converts compressor id string to category enum
    bool setCompressor( const char *strCompressorType, int iCompressionLevel = -1 )
    {
        compressor_type_e eCompressorType = CT_NONE;
        
        m_err.clear();
        
        if( 0 == iCompressionLevel || !strCompressorType || !*strCompressorType )
        {
            strCompressorType = COMPRESSOR_DEFAULT_ID;
            iCompressionLevel = 0;
        }
        
        if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4_ID ) )
        {
            eCompressorType = CT_BLOSC;
        }
        else if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4HC_ID ) )
        {
            eCompressorType = CT_BLOSC;
        }
        else if( 0 == _strcmpi( strCompressorType, BLOSC_DEFAULT_ID ) )
        {
            eCompressorType = CT_BLOSC;
        }
        else if( 0 == _strcmpi( strCompressorType, QLIN16_ID ) )
        {
            eCompressorType = CT_QLIN16;
        }
        else if( 0 == _strcmpi( strCompressorType, QLOG16_ID ) )
        {
            eCompressorType = CT_QLOG16;
        } 

        if( CT_NONE != eCompressorType )
        {
            m_strCompressorType = strCompressorType;
            m_eCompressorType   = eCompressorType;

            if( iCompressionLevel >= 0 )
            {
                m_iCompressionLevel = iCompressionLevel;
            }

            if( m_eCompressorType == CT_BLOSC )
            {
                blosc_set_compressor( m_strCompressorType );
            }

            return true;
        }
        else return false;
    }
    
    const char* getCompressorName()
    {
        return m_strCompressorType;
    }


    bool isLossy()
    {
        return m_eCompressorType == CT_QLIN16 || m_eCompressorType == CT_QLOG16;
    }
    
    
    // calls the qualified compressor (deflate) which always allocates sufficient memory (m_cdata)
    bool pack( void* rdata, size_t rdata_size, size_t rdata_element_size, bool isDoubleClass )
    {
        bool status = false;
        
        free_result();
        clear_data();
        clear_err();
        
        m_rdata                 = rdata;
        m_rdata_size            = rdata_size;
        m_rdata_element_size    = rdata_element_size;
        m_rdata_is_double_type  = isDoubleClass;
        
        switch( m_eCompressorType )
        {
          case CT_BLOSC:
            status = bloscCompress();
            break;
            
          case CT_QLIN16:
            status = linlogQuantizerCompress( /* bDoLog*/ false );
            break;
            
          case CT_QLOG16:
            status = linlogQuantizerCompress( /* bDoLog*/ true );
            break;
            
          default:
            break;
        }
        
        m_result_is_const   = false;
        m_result            = m_cdata;
        m_result_size       = m_cdata_size;
        
        return status;
    }
    
    
    // calls the qualified compressor (inflate)
    bool unpack( void* cdata, size_t cdata_size, void* rdata, size_t rdata_size, size_t rdata_element_size )
    {
        bool status = false;

        assert( rdata && rdata_size > 0 );
        
        free_result();
        clear_data();
        clear_err();
        
        m_cdata               = cdata;
        m_cdata_size          = cdata_size;
        m_rdata               = rdata;
        m_rdata_size          = rdata_size;
        m_rdata_element_size  = rdata_element_size;
        
        switch( m_eCompressorType )
        {
          case CT_BLOSC:
            status = bloscDecompress();
            break;
            
          case CT_QLIN16:
            status = linlogQuantizerDecompress( /* bDoLog*/ false );
            break;
            
          case CT_QLOG16:
            status = linlogQuantizerDecompress( /* bDoLog*/ true );
            break;
            
          default:
            break;
        }
        
        m_result_is_const   = true;
        m_result            = m_rdata;
        m_result_size       = m_rdata_size;
        
        return status;        
    }
    
    
private:
    // allocates m_cdata and use it to store compressed data from m_rdata
    // (lossless data compression)
    bool bloscCompress()
    {
        assert( m_rdata && !m_cdata );
        
        // BLOSC grants for compressed data never 
        // exceeds original size + BLOSC_MAX_OVERHEAD
        m_cdata_size  = m_rdata_size + BLOSC_MAX_OVERHEAD; 
        m_cdata       = m_Allocator( m_cdata_size );

        if( NULL == m_cdata )
        {
            m_err.set( MSG_ERRMEMORY );
            return false;
        }

        /* compress raw data (rdata) and store it in cdata */
        m_cdata_size = blosc_compress( 
          /*clevel*/     m_iCompressionLevel, 
          /*doshuffle*/  BLOSC_DOSHUFFLE, 
          /*typesize*/   m_rdata_element_size, 
          /*nbytes*/     m_rdata_size, 
          /*src*/        m_rdata, 
          /*dest*/       m_cdata, 
          /*destsize*/   m_cdata_size );
        
        return NULL != m_cdata;
    }
    
    
    // uncompress compressed data m_cdata to data m_rdata.
    // m_rdata must point to writable storage space and
    // m_rdata_size must specify the legal space.
    bool bloscDecompress()
    {
        assert( m_rdata && m_cdata );
        
        size_t blosc_nbytes, blosc_cbytes, blosc_blocksize; 
        
        blosc_cbuffer_sizes( m_cdata, &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );
        
        // uncompressed data must fit into item
        if( blosc_nbytes != m_rdata_size )
        {
            m_err.set( MSG_ERRCOMPRESSION );
            return false;
        }
        
        // decompress directly into item
        if( blosc_decompress( m_cdata, m_rdata, m_rdata_size ) <= 0 )
        {
            m_err.set( MSG_ERRCOMPRESSION );
            return false;
        } 
        
        return true;
    }
    
    
    // lossy data compression by linear or logarithmic quantization (16 bit)
    // allocates m_cdata and use it to store compressed data from m_rdata
    bool linlogQuantizerCompress( bool bDoLog )
    {
        assert( m_rdata && !m_cdata && 
                m_rdata_element_size == sizeof( double ) && 
                m_rdata_size % m_rdata_element_size == 0 );
        
        double    dOffset = 0.0, dScale = 1.0;
        double    dMinVal, dMaxVal;
        bool      bMinValSet = false, bMaxValSet = false;
        double*   rdata = (double*)m_rdata;
        size_t    cntElements = m_rdata_size / sizeof(*rdata);
        float*    pFloatData;
        uint16_t* pUintData;
        
        // compressor works for double type only
        if( !m_rdata_is_double_type )
        {
            m_err.set( MSG_ERRCOMPRARG );
            return false;
        }
        
        // seek data bounds for quantization
        for( size_t i = 0; i < cntElements; i++ )
        {
            if( DBL_ISFINITE( rdata[i] ) && rdata[i] != 0.0 )
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
            m_err.set( MSG_ERRCOMPRLOGMINVALS );
            return false;
        }

        // compressor converts each value to uint16_t
        // 2 additional floats for offset and scale
        m_cdata_size = 2 * sizeof( float ) + cntElements * sizeof( uint16_t );  
        m_cdata      = m_Allocator( m_cdata_size );

        if( !m_cdata )
        {
            m_err.set( MSG_ERRMEMORY );
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
        for( size_t i = 0; i < cntElements; i++ )
        {
            // non-finite values and zero are mapped to special values
            if( DBL_ISFINITE( rdata[i] ) && rdata[i] != 0.0 )
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
                else if( DBL_ISINF( rdata[i] ) )
                {
                    *pUintData++ = 0xFFF8u + 3 + ( copysign( 1.0, rdata[i] ) < 0.0 );
                }
                else if( DBL_ISNAN( rdata[i] ) )
                {
                    *pUintData++ = 0xFFF8u + 5;
                }
            }
        }
        
        return true;
    }
    
    
    // uncompress compressed data m_cdata to data m_rdata.
    // m_rdata must point to writable storage space and
    // m_rdata_size must specify the legal space.
    // (lossy data compression)
    bool linlogQuantizerDecompress( bool bDoLog )
    {
        assert( m_rdata && m_cdata && 
                m_rdata_element_size == sizeof( double ) && 
                m_rdata_size % m_rdata_element_size == 0 );
        
        double    dOffset = 0.0, dScale = 1.0;
        double*   rdata = (double*)m_rdata;
        size_t    cntElements = m_rdata_size / sizeof(*rdata);
        float*    pFloatData = (float*)m_cdata;
        uint16_t* pUintData = (uint16_t*)&pFloatData[2];
        
        // compressor works for double type only
        if( m_rdata_is_double_type )
        {
            m_err.set( MSG_ERRCOMPRARG );
            return false;
        }
        
        // restore offset and scale information
        dOffset = pFloatData[0];
        dScale  = pFloatData[1];
        
        // rescale values to its originals
        for( size_t i = 0; i < cntElements; i++ )
        {
            if( *pUintData > 0xFFF8u )
            {
                // handle special values for zero, infinity and nan
                switch( *pUintData - 0xFFF8u )
                {
                    case 1: *rdata = +0.0;      break;
                    case 2: *rdata = -0.0;      break;
                    case 3: *rdata = +DBL_INF;  break;  // pos. infinity
                    case 4: *rdata = -DBL_INF;  break;  // neg. infinity
                    case 5: *rdata = DBL_NAN;   break;  // not a number (NaN)
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
    
};
