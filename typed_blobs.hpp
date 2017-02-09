/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      typed_blobs.hpp
 *  @brief     Packing MATLAB data in a memory block with type information for storing as SQL BLOB
 *  @details   
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

//#include "config.h"
//#include "global.hpp"
#include "utils.hpp"

#ifdef _WIN32
  #define GCC_PACKED_STRUCT
  #pragma pack(1)
#else
  #define GCC_PACKED_STRUCT __attribute__((packed))
#endif
  
/**
 * \file
 * Size of blob-header identifies type 1 or type 2 (with compression feature).
 *
 * BLOBs of type mxUNKNOWN_CLASS reflects serialized (streamed) data and should be handled
 * as mxCHAR_CLASS thus. Before packing data into a typed blob the caller is 
 * responsible to ensure not to deal with mxUNKNOWN_CLASS data.
 *
 * In this module it's countless if the data is serialized or not, since it's a simple char
 * array in that case. 
 *
 * Store type and dimensions of MATLAB vectors/arrays in BLOBs 
 * native and free of matlab types, to provide data sharing 
 * with other applications.\n
 * Switched with the command mksqlite('typedBLOBs', \<integer value\>), 
 * where \<integer value\> is 0 for "off" and 1 for "on".
 */


#if !defined( MATLAB_MEX_FILE )
    /// MATLAB identifiers, if compiling modules reside MATLAB
    typedef enum 
    {
        mxUNKNOWN_CLASS = 0,
        mxCELL_CLASS,
        mxSTRUCT_CLASS,
        mxLOGICAL_CLASS,
        mxCHAR_CLASS,
        mxVOID_CLASS,
        mxDOUBLE_CLASS,
        mxSINGLE_CLASS,
        mxINT8_CLASS,
        mxUINT8_CLASS,
        mxINT16_CLASS,
        mxUINT16_CLASS,
        mxINT32_CLASS,
        mxUINT32_CLASS,
        mxINT64_CLASS,
        mxUINT64_CLASS,
        mxFUNCTION_CLASS,
        mxOPAQUE_CLASS,
        mxOBJECT_CLASS
    } mxClassID;

    typedef size_t  mwSize;

    /// If not compiling a max file, data type is native byte
    #define mxGetClassID(x) mxUINT8_CLASS
#endif


/**
 * \name Header field limits
 *
 * Length definitions for typed header fields (max lengths).
 *
 * @{
 */
#define TBH_MAGIC_MAXLEN      14
#define TBH_PLATFORM_MAXLEN   11
#define TBH_COMPRID_MAXLEN    12
#define TBH_ENDIAN_MAXLEN      2
/** @} */


/**
 * \name Text fields for integrity check (platform and magic)
 *
 * @{
 */
extern const char  TBH_MAGIC[];     ///< identify string
extern       char  TBH_platform[];  ///< platform name
extern       char  TBH_endian[];    ///< endian (little or big)
/** @} */


#ifdef MAIN_MODULE

/* Implementations */

static int typed_blobs_mode = 0; ///< typed blobs are off by default

namespace old_version { int check_compatibility(void); };

/**
 * \brief Initialization
 *
 * Get platform information. Data stored in the database may be dependent and
 * will be checked.\n
 * (Compatibility test in debug mode)
 */
void typed_blobs_init()
{
    mxArray *plhs[3] = {0};
    
    assert( old_version::check_compatibility() );
    
#if defined( MATLAB_MEX_FILE)
    if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
    {
        mxGetString( plhs[0], TBH_platform, TBH_PLATFORM_MAXLEN );
        mxGetString( plhs[2], TBH_endian, TBH_ENDIAN_MAXLEN );

        ::utils_destroy_array( plhs[0] );
        ::utils_destroy_array( plhs[1] );
        ::utils_destroy_array( plhs[2] );
    }
#else
    /// \todo How getting platform and endian?
#endif
}

/// Set mode of typed blob usage
void typed_blobs_mode_set( int mode )
{
    typed_blobs_mode = (mode != 0);
}

/// Get mode of typed blob usage
int typed_blobs_mode_on()
{
    return typed_blobs_mode;
}

/*static*/ const char  TBH_MAGIC[TBH_MAGIC_MAXLEN]         = "mkSQLite.tbh\0"; ///< identifying string
/*static*/       char  TBH_platform[TBH_PLATFORM_MAXLEN]   = {0};              ///< platform name
/*static*/       char  TBH_endian[TBH_ENDIAN_MAXLEN]       = {0};              ///< endian (little or big)

#endif

/**
 * \brief 1st (base) version of typed BLOB header with ability of storing multidimensional MATLAB arrays.
 * 
 * This struct is the unique and mandatory header prelude for typed blob headers
 */
struct GCC_PACKED_STRUCT TypedBLOBHeaderBase 
{
  char    m_magic[TBH_MAGIC_MAXLEN];        ///< + 14 small fail-safe header check
  int16_t m_ver;                            ///< +  2 Struct size as kind of header version number for later backwards compatibility (may increase only!)
  int32_t m_clsid;                          ///< +  4 Matlab ClassID of variable (see mxClassID)
  char    m_platform[TBH_PLATFORM_MAXLEN];  ///< + 11 Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
  char    m_endian;                         ///< +  1 Byte order: 'L'ittle endian or 'B'ig endian
                                            ///< = 32 Bytes (+4 bytes for int32_t m_nDims[1] later)
  
  /// Initialize structure with class ID and platform information
  void init( mxClassID clsid )
  {
    strcpy( m_magic,    TBH_MAGIC );
    strcpy( m_platform, TBH_platform );
    
    m_ver       = (int16_t)sizeof( *this ); // Header size used as version number
    m_clsid     = (int32_t)clsid;           // MATLABs class ID
    m_endian    = TBH_endian[0];            // First letter only ('L'ittle or 'B'ig)
  }
  
  /// Check identifying string (magic)
  bool validMagic()
  {
    return 0 == _strnicmp( m_magic, TBH_MAGIC, TBH_MAGIC_MAXLEN );
  }
  
  /// Check if class id is valid for typed blob (TYBLOB_ARRAYS)
  static
  bool validClsid( mxClassID clsid )
  {
    switch( clsid )
    {
      // standard types can be stored in any typed blob
      case mxLOGICAL_CLASS:
      case    mxCHAR_CLASS:
      case  mxDOUBLE_CLASS:
      case  mxSINGLE_CLASS:
      case    mxINT8_CLASS:
      case   mxUINT8_CLASS:
      case   mxINT16_CLASS:
      case  mxUINT16_CLASS:
      case   mxINT32_CLASS:
      case  mxUINT32_CLASS:
      case   mxINT64_CLASS:
      case  mxUINT64_CLASS:
        return true;
      default:
        // no other types supported so far
        return false;
    }
  }
  

  /// Check if class id is valid for typed blob (TYBLOB_ARRAYS)
  static
  bool validClsid( const mxArray* pItem )
  {
    return pItem && validClsid( mxGetClassID( pItem ) );
  }
  
  
  /// Check for valid self class ID
  bool validClsid()
  {
    return validClsid( (mxClassID)m_clsid );
  }
  
  
  /// Check if originate platform equals to running one
  bool validPlatform()
  {
    return TBH_endian[0] == m_endian && 0 == _strnicmp( TBH_platform, m_platform, TBH_PLATFORM_MAXLEN );
  }
  
  
#if defined( MATLAB_MEX_FILE )
  /// Get data size of an array in bytes
  static
  size_t getDataSize( const mxArray* pItem )
  {
    size_t data_size = 0;
    
    if( pItem )
    {
      size_t szElement   = mxGetElementSize( pItem );
      size_t cntElements = mxGetNumberOfElements( pItem );
      
      data_size = szElement * cntElements;
    }
    
    return data_size;
  }
#endif
};


/**
 * \brief 2nd version of typed blobs with additional compression feature.
 * 
 * \attention
 * NEVER ADD VIRTUAL FUNCTIONS TO HEADER CLASSES DERIVED FROM BASE!
 * Reason: Size of struct wouldn't match since a hidden vtable pointer 
 * would be attached then!
 */
struct GCC_PACKED_STRUCT TypedBLOBHeaderCompressed : public TypedBLOBHeaderBase 
{
  /// name of the compression algorithm used. Other algorithms possible in future..?
  char m_compression[12];

  /// Initialization
  void init( mxClassID clsid )
  {
    TypedBLOBHeaderBase::init( clsid );
    setCompressor( "" );
  }

  /// Compressor selection
  void setCompressor( const char* strCompressorType )
  {
    strncpy( m_compression, strCompressorType, sizeof( m_compression ) );
  }
  
  /// Get compressor name
  const char* getCompressor()
  {
    return m_compression;
  }
  
  /**
   * \brief Check for valid compressor
   * \todo Any checking needed?
   */
  bool validCompression()
  {
    return 1;
  }
};


/**
 * \brief Template class extending base class uniquely.
 * \relates TypedBLOBHeader
 * \relates TypedBLOBHeaderCompressed
 * 
 * This template class appends the number of dimensions, their extents and
 * finally the numeric data itself to the header.\
 * HeaderBaseType is either TypedBLOBHeader or TypedBLOBHeaderCompressed.
 */
template< typename HeaderBaseType >
struct GCC_PACKED_STRUCT TBHData : public HeaderBaseType
{
  /// Number of dimensions, followed by sizes of each dimension (BLOB data follows after last dimension size...)
  int32_t m_nDims[1];  

  /** 
   * \brief Initialization (hides base class init() function)
   *
   * \param[in] clsid MATLAB class ID of array elements
   * \param[in] nDims Amount of array dimensions
   * \param[in] pSize Pointer to vector of dimension lengths
   */
  void init( mxClassID clsid, mwSize nDims, const mwSize* pSize )
  {
    HeaderBaseType::init( clsid );
    HeaderBaseType::m_ver = sizeof( *this );
    
    assert( nDims >= 0 );
    assert( !nDims || pSize );
    
    m_nDims[0] = nDims;
    for( int i = 0; i < (int)nDims; i++ )
    {
      m_nDims[i+1] = (int32_t)pSize[i];
    }
  }
  
  
  /// Set class ID and dimension information of an array item
  void init( const mxArray* pItem )
  {
    assert( pItem );
    mxClassID clsid = mxGetClassID( pItem );
    mwSize nDims = mxGetNumberOfDimensions( pItem );
    const mwSize* dimensions = mxGetDimensions( pItem );
    
    init( clsid, nDims, dimensions );
  }
  
  
  /**
   * \brief Header version checking
   * 
   * Version information is stored as struct size. compare if
   * current size equals to assumed.
   */
  bool validVer()
  {
      return sizeof(*this) == (size_t)HeaderBaseType::m_ver;
  }
  
  
  /**
   * \brief Get pointer to array data (while initializing)
   *
   * Get a pointer to data begin with number of dimensions given
   * first data byte starts after last dimension.
   */
  void* getData( mwSize nDims )
  {
    return (void*)&m_nDims[ nDims + 1 ];
  }
  
  
  /// get a pointer to array data (initialized)
  void* getData()
  {
    return getData( m_nDims[0] );
  }
  
  
  /**
   * \brief Get header offset to begin of array data (while initializing)
   * 
   * Get the offset from structure begin to data begin with given
   * number of dimensions.
   */
  static
  size_t dataOffset( mwSize nDims )
  {
    //return offsetof( TBHData, m_nDims[nDims+1] ); /* doesn't work on linux gcc 4.1.2 */
    TBHData* p = (TBHData*)1024; // p must be something other than 0, due to compiler checking
    return (char*)&p->m_nDims[nDims+1] - (char*)p;
  }
  
  
  /// Get header offset to begin of array data (initialized)
  size_t dataOffset()
  {
    return dataOffset( m_nDims[0] );
  }
  
  
  /// Get data size in bytes, returns 0 on error
  size_t getDataSize()
  {
    return utils_elbytes( (mxClassID)HeaderBaseType::m_clsid );
  }
  
  
  /**
   * \brief
   * \param[in] doCopyData If true, containig data will be copied
   * 
   * Create a MATLAB array suitable for containing data,
   * which will optionally copied.
   */
  mxArray* createNumericArray( bool doCopyData )
  {
    mwSize nDims = m_nDims[0];
    mwSize* dimensions = new mwSize[nDims];
    mxArray* pItem = NULL;
    mxClassID clsid = (mxClassID)HeaderBaseType::m_clsid;
    
    for( int i = 0; i < (int)nDims; i++ )
    {
      dimensions[i] = (mwSize)m_nDims[i+1];
    }
    
    pItem = mxCreateNumericArray( nDims, dimensions, clsid, mxREAL );
    delete[] dimensions;
    
    // copy hosted item data into numeric array
    if( pItem && doCopyData )
    {
      memcpy( mxGetData( pItem ), getData(), TypedBLOBHeaderBase::getDataSize( pItem ) );
    }
    
    return pItem;
  }
  
private: 
  /**
   * \name Inhibitance
   *
   * Creation of instances inhibited.
   * Scheme represented by this template, and others derived, do only shadow 
   * memory got from allocators (malloc)
   *
   * @{
   */
  TBHData();
  TBHData( const TBHData& );
  TBHData& operator=( const TBHData& );
  /** @} */

};

typedef TBHData<TypedBLOBHeaderBase>       TypedBLOBHeaderV1;  ///< typed blob header for MATLAB arrays
typedef TBHData<TypedBLOBHeaderCompressed> TypedBLOBHeaderV2;  ///< typed blob header for MATLAB arrays with compression feature


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/// Test backward compatibility, will be removed in future releases
namespace old_version {
    /* Store type and dimensions of MATLAB vectors/arrays in BLOBs */
    static const char TBH_MAGIC[] = "mkSQLite.tbh";  ///< identifying string (magic) (deprecated)
    static char TBH_platform[11]    = {0};           ///< platform name (i.e. "PCWIN") (deprecated)
    static char TBH_endian[2]       = {0};           ///< endian used ('L'ittle or 'B'ig) (deprecated)

    // typed BLOB header agreement
    // native and free of matlab types, to provide data sharing with other applications

#ifdef _WIN32
#pragma pack( push )
#pragma pack()
#endif

    /// typed BLOB header (deprecated)
    typedef struct 
    {
      char magic[sizeof(TBH_MAGIC)];  ///< small fail-safe header check
      int16_t ver;                    ///< Struct size as kind of header version number for later backwards compatibility (may increase only!)
      int32_t clsid;                  ///< Matlab ClassID of variable (see mxClassID)
      char platform[11];              ///< Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
      char endian;                    ///< Byte order: 'L'ittle endian or 'B'ig endian
      int32_t sizeDims[1];            ///< Number of dimensions, followed by sizes of each dimension
                                      ///< First byte after header at &tbh->sizeDims[tbh->sizeDims[0]+1]
    } TypedBLOBHeader;

#ifdef _WIN32
#pragma pack( pop )
#endif

/// Get pointer to hosted data  (deprecated)
#define TBH_DATA(tbh)            ((void*)&tbh->sizeDims[tbh->sizeDims[0]+1])
/// Get offset from header start to hosted data  (deprecated)
#define TBH_DATA_OFFSET(nDims)   ((ptrdiff_t)&((TypedBLOBHeader*) 0)->sizeDims[nDims+1])

    /// Checks for valid header size, platform name, endian type and magic  (deprecated)
    int check_compatibility()
    {
        TypedBLOBHeaderV1*  tbh1         = (TypedBLOBHeaderV1*)1024;
        TypedBLOBHeader*    old_struct   = (TypedBLOBHeader*)1024;

        if(     (void*)&tbh1->m_ver          == (void*)&old_struct->ver
            &&  (void*)&tbh1->m_clsid        == (void*)&old_struct->clsid
            &&  (void*)&tbh1->m_platform[0]  == (void*)&old_struct->platform[0]
            &&  (void*)&tbh1->m_endian       == (void*)&old_struct->endian
            &&  (void*)&tbh1->m_nDims        == (void*)&old_struct->sizeDims
            &&   TBH_DATA_OFFSET(2)          == TypedBLOBHeaderV1::dataOffset(2) )
        {
            return 1;
        } 
        else 
        {
            return 0;
        }
    }

#undef TBH_DATA
#undef TBH_DATA_OFFSET
};
