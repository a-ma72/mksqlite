/*
 * mksqlite: A MATLAB Interface To SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

#ifdef _WIN32
  /* solve the 'error C2371: 'char16_t' : redefinition; different basic types' problem */
  /* ref: http://www.mathworks.com/matlabcentral/newsreader/view_thread/281754 */
  /* ref: http://connect.microsoft.com/VisualStudio/feedback/details/498952/vs2010-iostream-is-incompatible-with-matlab-matrix-h */
  #ifdef _WIN32
    #include <yvals.h>
    #if (_MSC_VER >= 1600)
      #define __STDC_UTF_16__
    #endif
/* Next solution doesn't work with Matlab R2011b, MSVC2010, Win7 64bit
    #ifdef _CHAR16T
      #define CHAR16_T
    #endif
*/
  #endif
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include "mex.h"
  #define GCC_PACKED_STRUCT
  #pragma pack(1)
  #define copysign _copysign
#else
  #include <cstring>
  #include <ctype.h>
  #define _strcmpi strcasecmp
  #define _snprintf snprintf
  #include "mex.h"
  #define GCC_PACKED_STRUCT __attribute__((packed))
#endif

#include <cmath>
#include <cassert>
#include <climits>
#include <cstdint>
#include "sqlite/sqlite3.h"
#include "deelx/deelx.h"
        
extern "C"
{
  #include "blosc/blosc.h"
  #include "md5/md5.h"  /* little endian only! */
}

/* Early bind mxSerialize and mxDeserialize */
#ifndef EARLY_BIND_SERIALIZE
#define EARLY_BIND_SERIALIZE 0
#endif

/* Versionstrings */
#define SQLITE_VERSION_STRING SQLITE_VERSION
#define DEELX_VERSION_STRING "1.2"
#define MKSQLITE_VERSION_STRING "1.14candidate"

/* get the SVN Revisionnumber */
#include "svn_revision.h"

/* Default Busy Timeout */
#define DEFAULT_BUSYTIMEOUT 1000


// SQLite itself limits BLOBs to 1MB, mksqlite limits to INT32_MAX
#define MKSQLITE_MAX_BLOB_SIZE ((mwSize)INT32_MAX)

// static assertion (compile time), ensures int32_t and mwSize as 4 byte data representation
static char SA_UIN32[ (sizeof(uint32_t)==4 && sizeof(mwSize)==4) ? 1 : -1 ]; 

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/*
 * Minimal structured "exception" handling with goto's, hence
 * a finally-block is missed to use try-catch-blocks here efficiently.
 * pseudo-code:
 **** %< ****
 * try-block
 * 
 * catch ( e )
 *  ->free local variables<-
 *  exit mex function and report error
 * 
 * catch (...)
 *  ->free local variables<-
 *  rethrow exception
 * 
 * ->free local variables<-
 * exit normally
 **** >% ****
 * 
 * Here, exception handling is done with goto's. Goto's are ugly and 
 * should be avoided in modern art of programming.
 * error handling is the lonely reason to use them: try-catch mechanism 
 * does the same but encapsulated and in a friendly and safe manner...
 */
static const char *g_finalize_msg = NULL;              // if assigned, function returns with an appropriate error message
static const char* SQL_ERR = "SQL_ERR";                // if attached to g_finalize_msg, function returns with least SQL error message
static const char* SQL_ERR_CLOSE = "SQL_ERR_CLOSE";    // same as SQL_ERR, additionally the responsible db will be closed
#define SETERR( msg ) ( g_finalize_msg = msg )
#define FINALIZE( msg ) { SETERR(msg); goto finalize; }
#define FINALIZE_IF( cond, msg ) { if(cond) FINALIZE( msg ) }

/* declare the MEX Entry function as pure C */
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );
extern "C" mxArray* mxSerialize(const mxArray*);
extern "C" mxArray* mxDeserialize(const void*, size_t);
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* common global states */

/*
 * Table of used database ids.
 */
#define MaxNumOfDbs 5
static sqlite3* g_dbs[MaxNumOfDbs] = { 0 };

/* declare the MEX Entry function as pure C */
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );

/* Flag: Show the welcome message, initializing... */
static bool g_is_initialized = false;

/* Flag: return NULL as NaN  */
static bool g_NULLasNaN  = false;
static const double g_NaN = mxGetNaN();

/* Flag: Check for unique fieldnames */
static bool g_check4uniquefields = true;

/* compression level: Using compression on typed blobs when > 0 */
static int         g_compression_level = 0;    // no compression by default
static const char* g_compression_type  = NULL; // compressor type
static int         g_compression_check = 1;    // Flag: check compressed against original data

/* Convert UTF-8 to ascii, otherwise set slCharacterEncoding('UTF-8') */
static bool g_convertUTF8 = true;

typedef enum {
    TC_EMPTY = 0,       // Empty
    TC_SIMPLE,          // 1-byte non-complex scalar, char or simple string (SQLite simple types)
    TC_SIMPLE_VECTOR,   // non-complex numeric vectors (SQLite BLOB)
    TC_SIMPLE_ARRAY,    // multidimensional non-complex numeric or char arrays (SQLite typed BLOB)
    TC_COMPLEX,         // structs, cells, complex data (SQLite typed ByteStream BLOB)
    TC_UNSUPP = -1      // all other (unsuppored types)
} type_complexity_e;

/* Store type and dimensions of MATLAB vectors/arrays in BLOBs 
 * native and free of matlab types, to provide data sharing 
 * with other applications
 */
typedef enum {
  TYBLOB_NO = 0,     // no typed blobs
  TYBLOB_ARRAY,      // storage of multidimensional non-complex arrays as typed blobs
  TYBLOB_BYSTREAM    // storage of complex data structures as byte stream in a typed blob
} typed_blobs_e;

static typed_blobs_e g_typed_blobs_mode = TYBLOB_NO; 

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static const int TBH_MAGIC_MAXLEN                   = 14;
static const char TBH_MAGIC[TBH_MAGIC_MAXLEN]       = "mkSQLite.tbh\0";
static const int PLATFORM_MAXLEN                    = 11;
static char g_platform[PLATFORM_MAXLEN]             = {0};
static char g_endian[2]                             = {0};
static const int COMPRID_MAXLEN                     = 12;
static const char BLOSC_LZ4_ID[COMPRID_MAXLEN]      = BLOSC_LZ4_COMPNAME;
static const char BLOSC_LZ4HC_ID[COMPRID_MAXLEN]    = BLOSC_LZ4HC_COMPNAME;
static const char BLOSC_DEFAULT_ID[COMPRID_MAXLEN]  = BLOSC_BLOSCLZ_COMPNAME;
static const char QLIN16_ID[COMPRID_MAXLEN]         = "QLIN16";
static const char QLOG16_ID[COMPRID_MAXLEN]         = "QLOG16";

// typed BLOB header agreement
// typed_BLOB_header_base is the unique and mandatory header prelude for typed blob headers
struct GCC_PACKED_STRUCT TypedBLOBHeaderBase 
{
  char m_magic[TBH_MAGIC_MAXLEN];   // + 14 small fail-safe header check
  int16_t m_ver;                    // +  2 Struct size as kind of header version number for later backwards compatibility (may increase only!)
  int32_t m_clsid;                  // +  4 Matlab ClassID of variable (see mxClassID)
  char m_platform[PLATFORM_MAXLEN]; // + 11 Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
  char m_endian;                    // +  1 Byte order: 'L'ittle endian or 'B'ig endian
                                    // = 32 Bytes (+4 bytes for int32_t m_nDims[1] later)
  
  void init( mxClassID clsid )
  {
    strcpy( m_magic,    TBH_MAGIC );
    strcpy( m_platform, g_platform );
    
    m_ver       = (int16_t)sizeof( *this );
    m_clsid     = (int32_t)clsid; 
    m_endian    = g_endian[0];
  }
  
  
  bool validMagic()
  {
    return 0 == strncmp( m_magic, TBH_MAGIC, TBH_MAGIC_MAXLEN );
  }
  
  
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
      case mxUNKNOWN_CLASS:
        // serialized complex data types are marked as "unknown"
        // these types are valid only, if serializing of object is available.
        return TYBLOB_BYSTREAM == g_typed_blobs_mode;
      default:
        // no other types supported
        return false;
    }
  }
  

  // check a data type given
  static
  bool validClsid( const mxArray* pItem )
  {
    return pItem && validClsid( mxGetClassID( pItem ) );
  }
  
  
  // check self data type
  bool validClsid()
  {
    return validClsid( (mxClassID)m_clsid );
  }
  
  
  // check if originate platform equals to running one
  bool validPlatform()
  {
    return g_endian[0] == m_endian && 0 == strncmp( g_platform, m_platform, PLATFORM_MAXLEN );
  }
  
  
  // get data size of an item in bytes
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
};


// 2nd version with compression feature
/* 
 * IMPORTANT: NEVER ADD VIRTUAL FUNCTIONS TO HEADER CLASSES DERIVED FROM BASE!
 * Reason: Size of struct wouldn't match since a hidden vtable pointer 
 *         would be attached then!
 */
struct GCC_PACKED_STRUCT TypedBLOBHeaderCompressed : public TypedBLOBHeaderBase 
{
  /* name of the compression algorithm used. Other algorithms
   * possible in future..?
   */
  char m_compression[12];
  
  void init( mxClassID clsid )
  {
    TypedBLOBHeaderBase::init( clsid );
    setCompressor( "" );
  }

  void setCompressor( const char* strCompressorType )
  {
      if( !strCompressorType || !strlen( strCompressorType ) )
      {
          strCompressorType = BLOSC_DEFAULT_ID;
      }

      strncpy( m_compression, strCompressorType, sizeof( strCompressorType ) );
  }
  
  // for now, all compressor types should be valid
  bool validCompression()
  {
    return 1;
  }
};


/* Template to append data and its dimensions uniquely to a typed BLOB header */
template< typename HeaderBaseType >
struct GCC_PACKED_STRUCT TBHData : public HeaderBaseType
{
  // Number of dimensions, followed by sizes of each dimension
  int32_t m_nDims[1];  
  // (BLOB data follows after last dimension size...)
  
  void init( mxClassID clsid, mwSize nDims, const mwSize* pSize )
  {
    HeaderBaseType::init( clsid );
    HeaderBaseType::m_ver = sizeof( *this );
    
    assert( nDims >= 0 );
    assert( !nDims || pSize );
    
    m_nDims[0] = nDims;
    for( int i = 0; i < nDims; i++ )
    {
      m_nDims[i+1] = (int32_t)pSize[i];
    }
  }
  
  
  // set class id and dimension information of an item
  void init( const mxArray* pItem )
  {
    assert( pItem );
    mxClassID clsid = mxGetClassID( pItem );
    mwSize nDims = mxGetNumberOfDimensions( pItem );
    const mwSize* dimensions = mxGetDimensions( pItem );
    
    init( clsid, nDims, dimensions );
  }
  
  
  // version information is stored as struct size. compare if
  // current size equals to assumed
  bool validVer()
  {
      return sizeof(*this) == (size_t)HeaderBaseType::m_ver;
  }
  
  
  // get a pointer to data begin with number of dimensions given
  // first data byte starts after last dimension
  void* getData( mwSize nDims )
  {
    return (void*)&m_nDims[ nDims + 1 ];
  }
  
  
  // get a pointer to self data begin
  void* getData()
  {
    return getData( m_nDims[0] );
  }
  
  
  // get the offset from structure begin to data begin with given
  // number of dimensions
  static
  size_t dataOffset( mwSize nDims )
  {
    //return offsetof( TBHData, m_nDims[nDims+1] ); /* doesn't work on linux gcc 4.1.2 */
    TBHData* p = (TBHData*)1024;
    return (char*)&p->m_nDims[nDims+1] - (char*)p;
  }
  
  
  // get the offset from structure begin to self data begin
  size_t dataOffset()
  {
    return dataOffset( m_nDims[0] );
  }
  
  
  // get data size in bytes
  size_t getDataSize()
  {
    mwSize nDims = (mwSize)m_nDims[0];
    mxArray* pItem = NULL;
    size_t data_size = 0;
    mxClassID clsid = (mxClassID)this->m_clsid;
    
    // serialized data is marked as unknown
    // data is stored as byte stream then
    if( clsid == mxUNKNOWN_CLASS )
    {
        assert( TYBLOB_BYSTREAM == g_typed_blobs_mode );
        clsid = mxUINT8_CLASS;
    }
    
    // dummy item to check size of one data element
    pItem = nDims ? mxCreateNumericMatrix( 1, 1, clsid, mxREAL ) : NULL;
    
    if( pItem )
    {
      data_size = mxGetElementSize( pItem );
    
      // calculate the size of the entire array in bytes
      for( mwSize i = 0; i < nDims; i++ )
      {
          data_size *= (mwSize)m_nDims[i+1];
      }
      
      // release dummy item
      DestroyArray( pItem );
    }
    
    return data_size;
  }
  
  
  // create a numeric array suitable for self item data
  // if doCopyData is true, item data is copied into array
  mxArray* createNumericArray( bool doCopyData )
  {
    mwSize nDims = m_nDims[0];
    mwSize* dimensions = new mwSize[nDims];
    mxArray* pItem = NULL;
    mxClassID clsid = (mxClassID)this->m_clsid;
    
    if( clsid == mxUNKNOWN_CLASS )
    {
        assert( TYBLOB_BYSTREAM == g_typed_blobs_mode );
        clsid = mxUINT8_CLASS;
    }
    
    for( int i = 0; i < nDims; i++ )
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
  // Disabling create instances! 
  // Scheme represented by this template, and others derived, do only shadow 
  // memory got from allocators (malloc)
  TBHData();
  TBHData( const TBHData& );
  TBHData& operator=( const TBHData& );

};



typedef TBHData<TypedBLOBHeaderBase>       TypedBLOBHeaderV1;
typedef TBHData<TypedBLOBHeaderCompressed> TypedBLOBHeaderV2;

/* Static assertion: Ensure backward compatibility */
static char SA_TBH_BASE[ sizeof( TypedBLOBHeaderV1 ) == 36 ? 1 : -1 ];


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* Test backward compatibility */

namespace old_version {
    /* Store type and dimensions of MATLAB vectors/arrays in BLOBs */
    static bool use_typed_blobs   = false;
    static const char TBH_MAGIC[] = "mkSQLite.tbh";
    static char g_platform[11]    = {0};
    static char g_endian[2]       = {0};

    // typed BLOB header agreement
    // native and free of matlab types, to provide data sharing with other applications

#ifdef _WIN32
#pragma pack( push )
#pragma pack()
#endif

    typedef struct 
    {
      char magic[sizeof(TBH_MAGIC)];  // small fail-safe header check
      int16_t ver;                    // Struct size as kind of header version number for later backwards compatibility (may increase only!)
      int32_t clsid;                  // Matlab ClassID of variable (see mxClassID)
      char platform[11];              // Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
      char endian;                    // Byte order: 'L'ittle endian or 'B'ig endian
      int32_t sizeDims[1];            // Number of dimensions, followed by sizes of each dimension
                                      // First byte after header at &tbh->sizeDims[tbh->sizeDims[0]+1]
    } TypedBLOBHeader;

#ifdef _WIN32
#pragma pack( pop )
#endif

#define TBH_DATA(tbh)            ((void*)&tbh->sizeDims[tbh->sizeDims[0]+1])
#define TBH_DATA_OFFSET(nDims)   ((ptrdiff_t)&((TypedBLOBHeader*) 0)->sizeDims[nDims+1])

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



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* SQLite function extensions by mksqlite */
void pow_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void regex_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_ratio_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_pack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDC_unpack_time_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void MD5_func( sqlite3_context *ctx, int argc, sqlite3_value **argv );

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* further forward declarations */

// (un-)packing functions
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_size, double* pProcess_time, double* pdRatio );
int blob_unpack( const void* pBlob, size_t blob_size, mxArray** ppItem, double* pProcess_time, double* pdRatio );

// (de-)serializing functions
// References:
// https://www.mathworks.com/matlabcentral/fileexchange/29457-serializedeserialize (Tim Hutt)
// https://www.mathworks.com/matlabcentral/fileexchange/34564-fast-serializedeserialize (Christian Kothe)
// http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data (Christian Kothe)
// getByteStreamFromArray(), getArrayFromByteStream() (undocumented Matlab functions)
bool have_serialize();
bool can_serialize();
bool serialize( const mxArray* pItem, mxArray*& pByteStream );
bool deserialize( const mxArray* pByteStream, mxArray*& pItem );

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/*
 * a poor man localization.
 * every language have a table of messages.
 */

/* Number of message table to use */
static int Language = -1;

#define MSG_HELLO               messages[Language][ 0]
#define MSG_INVALIDDBHANDLE     messages[Language][ 1]
#define MSG_IMPOSSIBLE          messages[Language][ 2]
#define MSG_USAGE               messages[Language][ 3]
#define MSG_INVALIDARG          messages[Language][ 4]
#define MSG_CLOSINGFILES        messages[Language][ 5]
#define MSG_CANTCOPYSTRING      messages[Language][ 6]
#define MSG_NOOPENARG           messages[Language][ 7]
#define MSG_NOFREESLOT          messages[Language][ 8]
#define MSG_CANTOPEN            messages[Language][ 9]
#define MSG_DBNOTOPEN           messages[Language][10]
#define MSG_INVQUERY            messages[Language][11]
#define MSG_CANTCREATEOUTPUT    messages[Language][12]
#define MSG_UNKNWNDBTYPE        messages[Language][13]
#define MSG_BUSYTIMEOUTFAIL     messages[Language][14]
#define MSG_MSGUNIQUEWARN       messages[Language][15]
#define MSG_UNEXPECTEDARG       messages[Language][16]
#define MSG_MISSINGARGL         messages[Language][17]
#define MSG_ERRMEMORY           messages[Language][18]
#define MSG_UNSUPPVARTYPE       messages[Language][19]
#define MSG_UNSUPPTBH           messages[Language][20]   
#define MSG_ERRPLATFORMDETECT   messages[Language][21]
#define MSG_WARNDIFFARCH        messages[Language][22]
#define MSG_BLOBTOOBIG          messages[Language][23]
#define MSG_ERRCOMPRESSION      messages[Language][24]
#define MSG_UNKCOMPRESSOR       messages[Language][25]
#define MSG_ERRCOMPRARG         messages[Language][26]
#define MSG_ERRCOMPRLOGMINVALS  messages[Language][27]
#define MSG_ERRUNKOPENMODE      messages[Language][28]
#define MSG_ERRUNKTHREADMODE    messages[Language][29]
#define MSG_ERRCANTCLOSE        messages[Language][30]
#define MSG_ERRCLOSEDBS         messages[Language][31]
#define MSG_ERRNOTSUPPORTED     messages[Language][32]
#define MSG_EXTENSION_EN        messages[Language][33]
#define MSG_EXTENSION_DIS       messages[Language][34]
#define MSG_EXTENSION_FAIL      messages[Language][35]
#define MSG_MISSINGARG          messages[Language][36]


/* 0 = english message table */
static const char* messages_0[] = 
{
    "mksqlite Version " MKSQLITE_VERSION_STRING " " SVNREV ", an interface from MATLAB to SQLite\n"
    "(c) 2008-2014 by Martin Kortmann <mail@kortmann.de>\n"
    "based on SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite uses further:\n"
    " - DEELX perl compatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " compression algorithm (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) implementation by Alexander Peslyak\n"
    "   \n"
    "UTF-8, parameter binding, regex and (compressed) typed BLOBs: A.Martin, 2014-01-23\n\n",
    
    "invalid database handle",
    "function not possible",
    "Usage: mksqlite([dbid,] command [, databasefile])\n",
    "no or wrong argument",
    "mksqlite: closing open databases",
    "Can\'t copy string in getstring()",
    "Open without Databasename",
    "No free databasehandle available",
    "cannot open database (check access privileges and existance of database)",
    "database not open",
    "invalid query string (Semicolon?)",
    "cannot create output matrix",
    "unknown SQLITE data type",
    "cannot set busytimeout",
    "could not build unique fieldname for %s",
    "unexpected arguments passed",
    "missing argument list",
    "memory allocation error",
    "unsupported variable type",
    "unknown/unsupported typed blob header",
    "error while detecting the type of computer you are using",
    "BLOB stored on different type of computer",
    "BLOB exceeds maximum allowed size",
    "error while compressing data",
    "unknown compressor",
    "choosed compressor accepts 'double' type only",
    "choosed compressor accepts positive values only",
    "unknown open modus (only 'ro', 'rw' or 'rwc' accepted)",
    "unknown threading mode (only 'single', 'multi' or 'serial' accepted)",
    "cannot close connection",
    "not all connections could be closed",
    "this Matlab version doesn't support this feature",
    "extension loading enabled for this db",
    "extension loading disabled for this db",
    "failed to set extension loading feature",
    "missing argument",
};


/* 1 = german message table */
static const char* messages_1[] = 
{
    "mksqlite Version " MKSQLITE_VERSION_STRING " " SVNREV ", ein MATLAB Interface zu SQLite\n"
    "(c) 2008-2014 by Martin Kortmann <mail@kortmann.de>\n"
    "basierend auf SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite verwendet darüberhinaus:\n"
    " - DEELX perl kompatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " zur Datenkompression (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) Implementierung von Alexander Peslyak\n"
    "   \n"
    "UTF-8, parameter binding, regex und (komprimierte) typisierte BLOBs: A.Martin, 2014-01-23\n\n",
    
    "ungültiger Datenbankhandle",
    "Funktion nicht möglich",
    "Verwendung: mksqlite([dbid,] Befehl [, Datenbankdatei])\n",
    "kein oder falsches Argument übergeben",
    "mksqlite: Die noch geöffneten Datenbanken wurden geschlossen",
    "getstring() kann keine neue Zeichenkette erstellen",
    "Open Befehl ohne Datenbanknamen",
    "Kein freier Datenbankhandle verfügbar",
    "Datenbank konnte nicht geöffnet werden (ggf. Zugriffsrechte oder Existenz der Datenbank prüfen)",
    "Datenbank nicht geöffnet",
    "ungültiger query String (Semikolon?)",
    "Kann Ausgabematrix nicht erstellen",
    "unbek. SQLITE Datentyp",
    "busytimeout konnte nicht gesetzt werden",
    "konnte keinen eindeutigen Bezeichner für Feld %s bilden",
    "Argumentliste zu lang",
    "keine Argumentliste angegeben",
    "Fehler bei Speichermanagement", 
    "Nicht unterstützter Variablentyp",
    "Unbekannter oder nicht unterstützter typisierter BLOB Header",
    "Fehler beim Identifizieren der Computerarchitektur",
    "BLOB wurde mit abweichender Computerarchitektur erstellt",
    "BLOB ist zu groß",
    "Fehler während der Kompression aufgetreten",
    "unbekannte Komprimierung",
    "gewähler Kompressor erlaubt nur Datentyp 'double'",
    "gewähler Kompressor erlaubt nur positive Werte",
    "unbekannzer Zugriffmodus (nur 'ro', 'rw' oder 'rwc' möglich)",
    "unbekannter Threadingmodus (nur 'single', 'multi' oder 'serial' möglich)",
    "die Datenbank kann nicht geschlossen werden",
    "nicht alle Datenbanken konnten geschlossen werden",
    "Feature wird von dieser Matlab Version nicht unterstützt",
    "DLL Erweiterungen für diese db aktiviert",
    "DLL Erweiterungen für diese db deaktiviert",
    "Einstellung für DLL Erweiterungen nicht möglich",
    "Parameter fehlt",
};


/*
 * Message Tables
 */
static const char **messages[] = 
{
    messages_0,   /* English messages */
    messages_1    /* German messages  */
};


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* helper functions */

/*
 * Converto UTF-8 Strings to 8Bit and vice versa
 */
static int utf2latin( const unsigned char *s, unsigned char *buffer )
{
    int cnt = 0;
    unsigned char ch, *p = buffer ? buffer : &ch;

    if( s ) 
    {
        while( *s ) 
        {
            if( *s < 128 ) 
            {
                *p = *s++;
            }
            else 
            {
                *p = ( s[0] << 6 ) | ( s[1] & 63 );
                s += 2;
            }
            if( buffer ) 
            {
                p++;
            }
            cnt++;
        }
        *p = 0;
        cnt++;
    }

    return cnt;
}


static int latin2utf( const unsigned char *s, unsigned char *buffer )
{
    int cnt = 0;
    unsigned char ch[2], *p = buffer ? buffer : ch;

    if( s ) 
    {
        while( *s ) 
        {
            if( *s < 128 ) 
            {
                *p++ = *s++;
                cnt++;
            }
            else 
            {
                *p++ = 128 + 64 + ( *s >> 6 );
                *p++ = 128 + ( *s++ & 63 );
                cnt += 2;
            }
            if( !buffer ) 
            {
                p = ch;
            }
        }
        *p = 0;
        cnt++;
    }

    return cnt;
}


/*
 * duplicate a string, 
 */
static char* strnewdup(const char* s)
{
    char *newstr = 0;
    
    if( g_convertUTF8 )
    {
        if( s )
        {
            int buflen = utf2latin( (unsigned char*)s, NULL );

            newstr = new char [buflen];
            if( newstr ) 
            {
                utf2latin( (unsigned char*)s, (unsigned char*)newstr );
            }
        }
    }
    else
    {
        if( s )
        {
            newstr = new char [strlen(s) +1];
            if( newstr )
            {
                strcpy( newstr, s );
            }
        }

    }
    
    return newstr;
}


/*
 * a single Value of an database row, including data type information
 */
class Value
{
public:
    int         m_Type;
    int         m_Size;

    char*       m_StringValue;
    double      m_NumericValue;
    
                Value()  : m_Type(0), m_Size(0), 
                            m_StringValue(0), m_NumericValue(0.0) {}
    virtual    ~Value()    { delete [] m_StringValue; } 
};


/*
 * all values of an database row
 */
class Values
{
public:
    int         m_Count;
    Value*      m_Values;
    
    Values*     m_NextValues;
    
                Values(int n)   : m_Count(n), m_NextValues(0)
                                  { m_Values = new Value[n]; }
            
    virtual    ~Values()          { delete [] m_Values; }
};


// Matlab documentation is missing the issue, if mxFree and mxDestroyArray
// accept NULL pointers. Tested without crash, but what will be in further 
// Matlab versions...
// So we do it on our own:

void destroy_array( mxArray *&pmxarr )
{
    if( pmxarr )
    {
        mxDestroyArray( pmxarr );
        pmxarr = NULL;
    }
}


template <class T>
void free_ptr( T *&pmxarr )
{
    if( pmxarr )
    {
        mxFree( (void*)pmxarr );
        pmxarr = NULL;
    }
}


/*
 * close left over databases.
 */
static void close_DBs( void )
{
    /*
     * Is there any database left?
     */
    bool dbsClosed = false;
    bool failed = false;
    for( int i = 0; i < MaxNumOfDbs; i++ )
    {
        /*
         * close it
         */
        if( g_dbs[i] )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[i] ) )
            {
                g_dbs[i] = 0;
                dbsClosed = true;
            } 
            else 
            {
                failed = true;
            }
        }
    }
    
    if( dbsClosed )
    {
        /*
         * Set the language to english if something
         * goes wrong before the language could been set
         */
        if( Language < 0 )
        {
            Language = 0;
        }
        /*
         * and inform the user
         */
        mexWarnMsgTxt( MSG_CLOSINGFILES );
    }
    
    sqlite3_shutdown();
    blosc_destroy();
    
    if( failed )
    {
        mexErrMsgTxt( MSG_ERRCLOSEDBS );
    }
}


/*
 * Get the last SQLite Error Code as an Error Identifier
 */
static const char* trans_err_to_ident( sqlite3 *db )
{
    static char dummy[32];

    int errorcode = sqlite3_errcode( db );
    
    switch( errorcode )
     {    
        case SQLITE_OK:         return( "SQLITE:OK" );
        case SQLITE_ERROR:      return( "SQLITE:ERROR" );
        case SQLITE_INTERNAL:   return( "SQLITE:INTERNAL" );
        case SQLITE_PERM:       return( "SQLITE:PERM" );
        case SQLITE_ABORT:      return( "SQLITE:ABORT" );
        case SQLITE_BUSY:       return( "SQLITE:BUSY" );
        case SQLITE_LOCKED:     return( "SQLITE:LOCKED" );
        case SQLITE_NOMEM:      return( "SQLITE:NOMEM" );
        case SQLITE_READONLY:   return( "SQLITE:READONLY" );
        case SQLITE_INTERRUPT:  return( "SQLITE:INTERRUPT" );
        case SQLITE_IOERR:      return( "SQLITE:IOERR" );
        case SQLITE_CORRUPT:    return( "SQLITE:CORRUPT" );
        case SQLITE_NOTFOUND:   return( "SQLITE:NOTFOUND" );
        case SQLITE_FULL:       return( "SQLITE:FULL" );
        case SQLITE_CANTOPEN:   return( "SQLITE:CANTOPEN" );
        case SQLITE_PROTOCOL:   return( "SQLITE:PROTOCOL" );
        case SQLITE_EMPTY:      return( "SQLITE:EMPTY" );
        case SQLITE_SCHEMA:     return( "SQLITE:SCHEMA" );
        case SQLITE_TOOBIG:     return( "SQLITE:TOOBIG" );
        case SQLITE_CONSTRAINT: return( "SQLITE:CONSTRAINT" );
        case SQLITE_MISMATCH:   return( "SQLITE:MISMATCH" );
        case SQLITE_MISUSE:     return( "SQLITE:MISUSE" );
        case SQLITE_NOLFS:      return( "SQLITE:NOLFS" );
        case SQLITE_AUTH:       return( "SQLITE:AUTH" );
        case SQLITE_FORMAT:     return( "SQLITE:FORMAT" );
        case SQLITE_RANGE:      return( "SQLITE:RANGE" );
        case SQLITE_NOTADB:     return( "SQLITE:NOTADB" );
        case SQLITE_ROW:        return( "SQLITE:ROW" );
        case SQLITE_DONE:       return( "SQLITE:DONE" );

        default:
            _snprintf( dummy, sizeof( dummy ), "SQLITE:%d", errorcode );
            return dummy;
     }
}


/*
 * Convert a string to char *
 */
static char *get_string( const mxArray *a )
{
    size_t count = mxGetM( a ) * mxGetN( a ) + 1;
    char *c = (char *)mxCalloc( count, sizeof(char) );

    if( !c || mxGetString( a, c, (int)count ) )
    {
        mexErrMsgTxt( MSG_CANTCOPYSTRING );
    }

    if( g_convertUTF8 )
    {
        char *buffer = NULL;
        int buflen;

        buflen = latin2utf( (unsigned char*)c, (unsigned char*)buffer );
        buffer = (char *) mxCalloc( buflen, sizeof(char) );

        if( !buffer )
        {
            free_ptr( c ); // Needless due to mexErrMsgTxt(), but clean
            mexErrMsgTxt( MSG_CANTCOPYSTRING );
        }

        latin2utf( (unsigned char*)c, (unsigned char*)buffer );

        free_ptr( c );

        return buffer;
    }
   
    return c;
}


/*
 * get an integer value from an numeric
 */
static int get_integer( const mxArray* a )
{
    switch( mxGetClassID( a ) )
    {
        case mxINT8_CLASS  : return (int) *( (int8_t*)   mxGetData(a) );
        case mxUINT8_CLASS : return (int) *( (uint8_t*)  mxGetData(a) );
        case mxINT16_CLASS : return (int) *( (int16_t*)  mxGetData(a) );
        case mxUINT16_CLASS: return (int) *( (uint16_t*) mxGetData(a) );
        case mxINT32_CLASS : return (int) *( (int32_t*)  mxGetData(a) );
        case mxUINT32_CLASS: return (int) *( (uint32_t*) mxGetData(a) );
        case mxSINGLE_CLASS: return (int) *( (float*)    mxGetData(a) );
        case mxDOUBLE_CLASS: return (int) *( (double*)   mxGetData(a) );
    }
    
    return 0;
}


// check if item is a vector (one dimension must be 1)
static bool is_vector( const mxArray* item )
{
    assert( item );
    return    mxGetNumberOfDimensions( item ) == 2
           && min( mxGetM( item ), mxGetN( item ) ) == 1;
}


// check if item is scalar (any dimension must be 1)
static bool is_scalar( const mxArray* item )
{
    assert( item );
    return mxGetNumberOfElements( item ) == 1;
}


// get the complexitity of an item (salar, vector, ...)
// (to consider how to pack)
static type_complexity_e get_type_complexity( const mxArray* pItem )
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
      return ( TYBLOB_BYSTREAM == g_typed_blobs_mode ) ? TC_COMPLEX : TC_UNSUPP;
    case  mxSTRUCT_CLASS:
    case    mxCELL_CLASS:
      return TC_COMPLEX;
    default:
      return TC_UNSUPP;
  }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* the main routine */

/*
 * This ist the Entry Function of this Mex-DLL
 */
void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] )
{
    mxArray* pArgs       = NULL;  // Cell array of parameters behind command, in case of an SQL command only
    sqlite3_stmt *st     = NULL;  // SQL statement (sqlite bridge)
    char *command        = NULL;  // the SQL command (superseeded by query)
    g_finalize_msg       = NULL;  // pointer to actual error message
    int FirstArg         = 0;
    
    /*
     * Get the current Language
     */
    if( Language == -1 )
    {
#ifdef _WIN32        
        switch( PRIMARYLANGID( GetUserDefaultLangID() ) )
        {
            case LANG_GERMAN:
                Language = 1;
                break;
                
            default:
                Language = 0;
        }
#else
        Language = 0;
#endif
    }
    
    /*
     * Print Version Information, initializing, ...
     */
    if( !g_is_initialized )
    {
        mxArray *plhs[3] = {0};
        
        if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
        {
            assert( old_version::check_compatibility() );

            mexPrintf( MSG_HELLO, sqlite3_libversion() );
            
            mxGetString( plhs[0], g_platform, PLATFORM_MAXLEN );
            mxGetString( plhs[2], g_endian, 2 );

            mexPrintf( "Platform: %s, %s\n\n", g_platform, g_endian[0] == 'L' ? "little endian" : "big endian" );
            
            destroy_array( plhs[0] );
            destroy_array( plhs[1] );
            destroy_array( plhs[2] );
            
            mexAtExit( close_DBs );
            sqlite3_initialize();
            blosc_init();
            g_compression_type = BLOSC_DEFAULT_ID;
            g_is_initialized = true;
        }
        else
        {
            mexErrMsgTxt( MSG_ERRPLATFORMDETECT );
        }
    }
    
    int db_id = 0;
    int CommandPos = 0;
    int NumArgs = nrhs;
    
    /*
     * Check if the first argument is a number, then we have to use
     * this number as an database id.
     */
    if( nrhs >= 1 && mxIsNumeric( prhs[0] ) )
    {
        db_id = get_integer( prhs[0] );
        if( db_id < 0 || db_id > MaxNumOfDbs )
        {
            FINALIZE( MSG_INVALIDDBHANDLE );
        }
        db_id--;
        CommandPos++;
        NumArgs--;
    }

    /*
     * no argument -> fail
     */
    if( NumArgs < 1 )
    {
        mexPrintf( "%s", MSG_USAGE );
        FINALIZE( MSG_INVALIDARG );
    }
    
    /*
     * The next (or first if no db number available) is the command,
     * it has to be a string.
     * This fails also, if the first arg is a db-id and there is no 
     * further argument
     */
    if( !mxIsChar( prhs[CommandPos] ) )
    {
        mexPrintf( "%s", MSG_USAGE );
        FINALIZE( MSG_INVALIDARG );
    }
    
    /*
     * Get the command string
     */
    command = get_string( prhs[CommandPos] );
    
    /*
     * Adjust the Argument pointer and counter
     */
    FirstArg = CommandPos + 1;
    NumArgs--;
    
    if( !strcmp( command, "version mex" ) )
    {
        if( nlhs == 0 )
        {
            mexPrintf( "mksqlite Version %s\n", MKSQLITE_VERSION_STRING );
        } 
        else
        {
            plhs[0] = mxCreateString( MKSQLITE_VERSION_STRING );
        }
    } 
    else if( !strcmp( command, "version sql" ) )
    {
        if( nlhs == 0 )
        {
            mexPrintf( "SQLite Version %s\n", SQLITE_VERSION_STRING );
        } 
        else 
        {
            plhs[0] = mxCreateString( SQLITE_VERSION_STRING );
        }
    } 
    else if( !strcmp( command, "open" ) )
    {
        int flags = 0;
        /*
         * open a database. There has to be one string argument,
         * the database filename
         */
        if( NumArgs < 1 || !mxIsChar( prhs[FirstArg] ) )
        {
            FINALIZE( MSG_NOOPENARG );
        }
        
        // No Memoryleak 'command not freed' when get_string fails
        // Matlab Help:
        // "If your application called mxCalloc or one of the 
        // mxCreate* routines to allocate memory, mexErrMsgTxt 
        // automatically frees the allocated memory."
        char* dbname = get_string( prhs[FirstArg] );

        /*
         * Is there an database ID? The close the database with the same id 
         */
        if( db_id > 0 && g_dbs[db_id] )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", MSG_ERRCANTCLOSE );
                FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
            }
        }

        /*
         * If there isn't an database id, then try to get one
         */
        if( db_id < 0 )
        {
            for( int i = 0; i < MaxNumOfDbs; i++ )
            {
                if( g_dbs[i] == 0 )
                {
                    db_id = i;
                    break;
                }
            }
        }
        /*
         * no database id? sorry, database id table full
         */
        if( db_id < 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            
            free_ptr( dbname );  // Needless due to mexErrMsgTxt(), but clean
            FINALIZE( MSG_NOFREESLOT );
        }
        
        
        /*
         * Open mode (optional)
         */
        if( NumArgs >= 2 )
        {
            char* iomode = get_string( prhs[FirstArg+1] );
            
            if( 0 == _strcmpi( iomode, "ro" ) )
            {
                flags |= SQLITE_OPEN_READONLY;
            } 
            else if( 0 == _strcmpi( iomode, "rw" ) )
            {
                flags |= SQLITE_OPEN_READWRITE;
            } 
            else if( 0 == _strcmpi( iomode, "rwc" ) )
            {
                flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            } 
            else 
            {
                free_ptr( iomode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_ERRUNKOPENMODE );
            }
            free_ptr( iomode ); 
        } 
        else 
        {
            flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        }
        
        
        /*
         * Threading mode (optional)
         */
        if( NumArgs >= 3 )
        {
            char* threadmode = get_string( prhs[FirstArg+2] );
            
            if( 0 == _strcmpi( threadmode, "single" ) )
            {
                /* default */
            } 
            else if( 0 == _strcmpi( threadmode, "multi" ) )
            {
                flags |= SQLITE_OPEN_NOMUTEX;
            } 
            else if( 0 == _strcmpi( threadmode, "serial" ) )
            {
                flags |= SQLITE_OPEN_FULLMUTEX;
            } 
            else 
            {
                free_ptr( threadmode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_ERRUNKTHREADMODE );
            }
            free_ptr( threadmode );
        } 
        
       
        /*
         * Open the database
         * (g_dbs[db_id] is assigned by sqlite3_open_v2(), even if an error
         * occures
         */
        int rc = sqlite3_open_v2( dbname, &g_dbs[db_id], flags, NULL );
        
        if( SQLITE_OK != rc )
        {
            /*
             * Database could not opened
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            
            free_ptr( dbname );   // Needless due to mexErrMsgTxt(), but clean
            mexPrintf( "%s\n", MSG_CANTOPEN );
            FINALIZE( SQL_ERR_CLOSE );
        }
        
        /*
         * Set Default Busytimeout
         */
        rc = sqlite3_busy_timeout( g_dbs[db_id], DEFAULT_BUSYTIMEOUT );
        if( SQLITE_OK != rc )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            free_ptr( dbname );   // Needless due to mexErrMsgTxt(), but clean
            mexPrintf( "%s\n", MSG_BUSYTIMEOUTFAIL );
            FINALIZE( SQL_ERR_CLOSE );
        }
        
        // attach new SQL commands to opened database
        sqlite3_create_function( g_dbs[db_id], "pow", 2, SQLITE_UTF8, NULL, pow_func, NULL, NULL );     // power function (math)
        sqlite3_create_function( g_dbs[db_id], "regex", 2, SQLITE_UTF8, NULL, regex_func, NULL, NULL ); // regular expressions (MATCH mode)
        sqlite3_create_function( g_dbs[db_id], "regex", 3, SQLITE_UTF8, NULL, regex_func, NULL, NULL ); // regular expressions (REPLACE mode)
        sqlite3_create_function( g_dbs[db_id], "bdcratio", 1, SQLITE_UTF8, NULL, BDC_ratio_func, NULL, NULL );     // compression ratio (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcpacktime", 1, SQLITE_UTF8, NULL, BDC_pack_time_func, NULL, NULL );     // compression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcunpacktime", 1, SQLITE_UTF8, NULL, BDC_unpack_time_func, NULL, NULL );     // decompression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "md5", 1, SQLITE_UTF8, NULL, MD5_func, NULL, NULL );     // Message-Digest (RSA)
        
        /*
         * return value will be the used database id
         */
        plhs[0] = mxCreateDoubleScalar( (double)db_id + 1 );
        free_ptr( dbname );
    }
    else if( !strcmp( command, "close" ) )
    {
        /*
         * close a database
         */

        /*
         * There should be no Argument to close
         */
        FINALIZE_IF( NumArgs > 0, MSG_INVALIDARG );
        
        /*
         * if the database id is < 0 than close all open databases
         */
        if( db_id < 0 )
        {
            for( int i = 0; i < MaxNumOfDbs; i++ )
            {
                if( g_dbs[i] )
                {
                    if( SQLITE_OK == sqlite3_close( g_dbs[i] ) )
                    {
                        g_dbs[i] = 0;
                    } 
                    else 
                    {
                        mexPrintf( "%s\n", MSG_ERRCANTCLOSE );
                        FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
                    }
                }
            }
        }
        else
        {
            /*
             * If the database is open, then close it. Otherwise
             * inform the user
             */
            FINALIZE_IF( !g_dbs[db_id], MSG_DBNOTOPEN );

            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", MSG_ERRCANTCLOSE );
                FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
            }
        }
    }
    else if( !strcmp( command, "enable extension" ) )
    {
        /*
         * There should be one Argument to enable extension
         */
        FINALIZE_IF( NumArgs < 1, MSG_MISSINGARG );
        FINALIZE_IF( NumArgs > 1, MSG_UNEXPECTEDARG );
        FINALIZE_IF( !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
        FINALIZE_IF( !g_dbs[db_id], MSG_DBNOTOPEN );
        
        int flagOnOff = get_integer( prhs[FirstArg] );
        
        if( SQLITE_OK == sqlite3_enable_load_extension( g_dbs[db_id], flagOnOff ) )
        {
            if( flagOnOff )
            {
                mexPrintf( "%s\n", MSG_EXTENSION_EN );
            } 
            else 
            {
                mexPrintf( "%s\n", MSG_EXTENSION_DIS );
            }
        } 
        else 
        {
            mexPrintf( "%s\n", MSG_EXTENSION_FAIL );
        }
    }
    else if( !strcmp( command, "status" ) )
    {
        /*
         * There should be no Argument to status
         */
        FINALIZE_IF( NumArgs > 0, MSG_INVALIDARG );
        
        for( int i = 0; i < MaxNumOfDbs; i++ )
        {
            mexPrintf( "DB Handle %d: %s\n", i, g_dbs[i] ? "OPEN" : "CLOSED" );
        }
    }
    else if( !_strcmpi( command, "setbusytimeout" ) )
    {
        /*
         * There should be one Argument, the Timeout in ms
         */
        FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
        FINALIZE_IF( !g_dbs[db_id], MSG_DBNOTOPEN );

        /*
         * Set Busytimeout
         */
        int TimeoutValue = get_integer( prhs[FirstArg] );

        int rc = sqlite3_busy_timeout( g_dbs[db_id], TimeoutValue );
        if( SQLITE_OK != rc )
        {
            /*
             * Anything wrong? free the database id and inform the user
             */
            plhs[0] = mxCreateDoubleScalar( (double)0 );
            mexPrintf( "%s\n", MSG_BUSYTIMEOUTFAIL );
            FINALIZE( SQL_ERR_CLOSE );
        }
    }
    else if( !_strcmpi( command, "check4uniquefields" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_check4uniquefields );
        }
        else 
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_check4uniquefields = ( get_integer( prhs[FirstArg] ) ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "convertUTF8" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_convertUTF8 );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_convertUTF8 = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "typedBLOBs" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_typed_blobs_mode );
        }
        else
        {
            int iValue;
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            iValue = get_integer( prhs[FirstArg] );
            FINALIZE_IF( iValue < /*0*/ TYBLOB_NO || iValue > /*2*/ TYBLOB_BYSTREAM, MSG_INVALIDARG );
            if( TYBLOB_BYSTREAM == iValue && !can_serialize() )
            {
                FINALIZE( MSG_ERRNOTSUPPORTED );
            }
            g_typed_blobs_mode = (typed_blobs_e)iValue;
        }
    }
    else if( !_strcmpi( command, "NULLasNaN" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_NULLasNaN );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_NULLasNaN = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "compression" ) )
    {
        if( NumArgs == 0 )
        {
            mxArray* cell = mxCreateCellMatrix( 2, 1 );
            mxArray* compressor = mxCreateString( g_compression_type ? g_compression_type : "" );
            mxArray* level = mxCreateDoubleScalar( (double)g_compression_level );
            mxSetCell( cell, 0, compressor );
            mxSetCell( cell, 1, level );
            plhs[0] = cell;
        }
        else
        {
            int new_compression_level = 0;
            const char* new_compressor = NULL;
            
            FINALIZE_IF(     NumArgs != 2 
                        || !mxIsChar( prhs[FirstArg] )
                        || !mxIsNumeric( prhs[FirstArg+1] ), MSG_INVALIDARG );
            
            new_compressor = get_string( prhs[FirstArg] );
            new_compression_level = ( get_integer( prhs[FirstArg+1] ) );
            
            FINALIZE_IF( new_compression_level < 0 || new_compression_level > 9, MSG_INVALIDARG );

            if( 0 == _strcmpi( new_compressor, BLOSC_LZ4_ID ) )
            {
                g_compression_type = BLOSC_LZ4_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, BLOSC_LZ4HC_ID ) )
            {
                g_compression_type = BLOSC_LZ4HC_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, BLOSC_DEFAULT_ID ) )
            {
                g_compression_type = BLOSC_DEFAULT_ID;
            } 
            else if( 0 == _strcmpi( new_compressor, QLIN16_ID ) )
            {
                g_compression_type = QLIN16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else if( 0 == _strcmpi( new_compressor, QLOG16_ID ) )
            {
                g_compression_type = QLOG16_ID;
                new_compression_level = ( new_compression_level > 0 ); // only 0 or 1
            } 
            else {
                free_ptr( new_compressor );
                FINALIZE( MSG_INVALIDARG );
            }
            
            free_ptr( new_compressor );
            g_compression_level = new_compression_level;
        }
    }
    else if( !_strcmpi( command, "compression_check" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_compression_check );
        }
        else
        {
            FINALIZE_IF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_compression_check = get_integer( prhs[FirstArg] ) ? true : false;
        }
    }
    else
    {
        /*
         * database id < 0? That's an error...
         */
        if( db_id < 0 )
        {
            FINALIZE( MSG_INVALIDDBHANDLE );
        }
        
        /*
         * database not open? -> error
         */
        FINALIZE_IF( !g_dbs[db_id], MSG_DBNOTOPEN );
        
        /*
         * Every unknown command is treated as an sql query string
         */
        const char* query = command;

        /*
         * emulate the "show tables" sql query
         */
        if( !_strcmpi( query, "show tables" ) )
        {
            query = "SELECT name as tablename FROM sqlite_master "
                    "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                    "UNION ALL "
                    "SELECT name as tablename FROM sqlite_temp_master "
                    "WHERE type IN ('table','view') "
                    "ORDER BY 1";
        }

        /*
         * complete the query
         */
        // TODO: What does this test? sqlite3_complete() returns 1 if the string is complete and valid...
        FINALIZE_IF( sqlite3_complete( query ) != 0, MSG_INVQUERY );
        
        /*
         * and prepare it
         * if anything is wrong with the query, than complain about it.
         */
        FINALIZE_IF( SQLITE_OK != sqlite3_prepare_v2( g_dbs[db_id], query, -1, &st, 0 ), SQL_ERR );
        
        /*
         * Parameter binding 
         */
        int bind_names_count = sqlite3_bind_parameter_count( st );
        
        // the number of arguments may not exceed the number of placeholders
        // in the sql statement
        FINALIZE_IF( NumArgs > bind_names_count, MSG_UNEXPECTEDARG );

        // collect input arguments in a cell array
        if(    !NumArgs                                // results in an empty cell array
            || !mxIsCell( prhs[FirstArg] )             // mxIsCell() not called when NumArgs==0 !
            || TYBLOB_BYSTREAM == g_typed_blobs_mode ) // if serialized items are allowed, no encapsulated parameters are
        {
            // Collect parameter list in one cell array
            pArgs = mxCreateCellMatrix( NumArgs, 1 );
            for( int i = 0; pArgs && i < NumArgs; i++ )
            {
                // deep copy into cell array
                mxSetCell( pArgs, i, mxDuplicateArray( prhs[FirstArg+i] ) );
            }
        }
        else
        {
            // Parameters may be (and are) packed in only one single 
            // cell array
            FINALIZE_IF( NumArgs > 1, MSG_UNEXPECTEDARG );

            // Make a deep copy
            pArgs = mxDuplicateArray( prhs[FirstArg] );
            
            FINALIZE_IF( (int)mxGetNumberOfElements( pArgs ) > bind_names_count, MSG_UNEXPECTEDARG );
        }
        
        // if parameters needed for parameter binding, 
        // at least one parameter has to be passed 
        FINALIZE_IF( !pArgs, MSG_MISSINGARGL );

        for( int iParam = 0; iParam < bind_names_count; iParam++ )
        {
            // item must not be destroyed! (Matlab would crash)
            const mxArray *pItem = NULL;
            
            if( iParam + 1 <= (int)mxGetNumberOfElements( pArgs ) )
            {
                pItem = mxGetCell( pArgs, iParam );
            }
                

            if( !pItem || mxIsEmpty( pItem ) )
            {
                if( SQLITE_OK != sqlite3_bind_null( st, iParam+1 ) )
                {
                    FINALIZE( SQL_ERR );
                }
                continue;
            }

            size_t    szElement         = mxGetElementSize( pItem );      // size of one element in bytes
            size_t    cntElements       = mxGetNumberOfElements( pItem ); // number of elements in cell array
            mxClassID clsid             = mxGetClassID( pItem );
            int       iTypeComplexity   = get_type_complexity( pItem );
            char*     str_value         = NULL;

            switch( iTypeComplexity )
            {
              case TC_COMPLEX:
                // structs, cells, complex data (SQLite typed ByteStream BLOB)
                // can only be stored as undocumented byte stream
                if( TYBLOB_BYSTREAM != g_typed_blobs_mode )
                {
                    FINALIZE( MSG_INVALIDARG );
                }
                /* otherwise fallthrough */
              case TC_SIMPLE_ARRAY:
                // multidimensional non-complex numeric or char arrays (SQLite typed BLOB)
                // can only be stored in typed BLOBs
                if( TYBLOB_NO == g_typed_blobs_mode )
                {
                    FINALIZE( MSG_INVALIDARG );
                }
                /* otherwise fallthrough */
              case TC_SIMPLE_VECTOR:
                // non-complex numeric vectors (SQLite BLOB)
                if( TYBLOB_NO == g_typed_blobs_mode )
                {
                    // matrix arguments are omitted as blobs, except string arguments
                    // data is attached as is, no header information here
                    const void* blob = mxGetData( pItem );
                    // SQLite makes a lokal copy of the blob (thru SQLITE_TRANSIENT)
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, 
                                                        (int)(cntElements * szElement),
                                                        SQLITE_TRANSIENT ) )
                    {
                        FINALIZE( SQL_ERR );
                    }
                } 
                else 
                {
                    void*  blob          = NULL;
                    size_t blob_size     = 0;
                    double process_time  = 0.0;
                    double ratio         = 0.0;
                    
                    /* blob_pack() modifies g_finalize_msg */
                    FINALIZE_IF( !blob_pack( pItem, &blob, &blob_size, &process_time, &ratio ), g_finalize_msg );
                    
                    // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, (int)blob_size, sqlite3_free ) )
                    {
                        FINALIZE( SQL_ERR );
                    }  
                }
                break;
              case TC_SIMPLE:
                // 1-byte non-complex scalar, char or simple string (SQLite simple types)
                switch( clsid )
                {
                    case mxLOGICAL_CLASS:
                    case mxINT8_CLASS:
                    case mxUINT8_CLASS:
                    case mxINT16_CLASS:
                    case mxINT32_CLASS:
                    case mxUINT16_CLASS:
                    case mxUINT32_CLASS:
                        // scalar integer value
                        if( SQLITE_OK != sqlite3_bind_int( st, iParam+1, (int)mxGetScalar( pItem ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxDOUBLE_CLASS:
                    case mxSINGLE_CLASS:
                        // scalar floating point value
                        if( SQLITE_OK != sqlite3_bind_double( st, iParam+1, mxGetScalar( pItem ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxCHAR_CLASS:
                    {
                        // string argument
                        char* str_value = mxArrayToString( pItem );
                        if( str_value && g_convertUTF8 )
                        {
                            int len = latin2utf( (unsigned char*)str_value, NULL ); // get the size only
                            unsigned char *temp = (unsigned char*)mxCalloc( len, sizeof(char) ); // allocate memory
                            if( temp )
                            {
                                latin2utf( (unsigned char*)str_value, temp );
                                free_ptr( str_value );
                                str_value = (char*)temp;
                            }
                            else
                            {
                                free_ptr( str_value );  // Needless due to mexErrMsgTxt(), but clean
                                FINALIZE( MSG_ERRMEMORY );
                            }
                        }
                        // SQLite makes a lokal copy of the blob (thru SQLITE_TRANSIENT)
                        if( SQLITE_OK != sqlite3_bind_text( st, iParam+1, str_value, -1, SQLITE_TRANSIENT ) )
                        {
                            free_ptr( str_value );  // Needless due to mexErrMsgTxt(), but clean
                            FINALIZE( SQL_ERR );
                        }
                        free_ptr( str_value );
                        break;
                    }
                } // end switch
                break;
              case TC_EMPTY:
                // Empty parameters are omitted as NULL by sqlite
                continue;
              default:
                // all other (unsuppored types)
                FINALIZE( MSG_INVALIDARG );
                break;
            }
        }

        /*
         * Any results?
         */
        int ncol = sqlite3_column_count( st );
        if( ncol > 0 )
        {
            char **fieldnames = new char *[ncol];   /* Column names */
            Values* allrows = 0;                    /* All query results */
            Values* lastrow = 0;                    /* pointer to the last result row */
            int rowcount = 0;                       /* number of result rows */
            
            /*
             * Get the column names of the result set
             */
            for( int iCol=0; iCol<ncol; iCol++ )
            {
                const char *cname = sqlite3_column_name( st, iCol );
                
                fieldnames[iCol] = new char [strlen(cname) +1];
                strcpy( fieldnames[iCol], cname );
                /*
                 * replace invalid chars by '_', so we can build
                 * valid MATLAB structs
                 */
                char *mk_c = fieldnames[iCol];
                while( *mk_c )
                {
//                    if ((*mk_c == ' ') || (*mk_c == '*') || (*mk_c == '?') || !isprint(*mk_c))
                    if( !isalnum( *mk_c ) )
                        *mk_c = '_';
                    mk_c++;
                }
                
                if( fieldnames[iCol][0] == '_' )
                {
                    fieldnames[iCol][0] = 'X';  // fieldnames must not start with an underscore
                                          // TODO: Any other ideas?
                }
            }
            /*
             * Check for duplicate colum names
             */
            if( g_check4uniquefields )
            {
                for( int iCol = 0; iCol < (ncol -1); iCol++ )
                {
                    for( int jCol = iCol+1; jCol < ncol; jCol++ )
                    {
                        if( !strcmp( fieldnames[iCol], fieldnames[jCol] ) )
                        {
                            /*
                             * Change the duplicate colum name to be unique
                             * by adding _x to it. x counts from 1 to 99
                             */
                            size_t fnamelen = strlen( fieldnames[jCol] ) + 4;
                            char *newcolumnname = new char [fnamelen];
                            int k;
                            
                            for( k = 1; k < 100; k++ )
                            {
                                /*
                                 * Build new name
                                 */
                                _snprintf( newcolumnname, fnamelen, "%s_%d", fieldnames[jCol], k );

                                /*
                                 * is it already unique? */
                                bool unique = true;
                                for( int lCol = 0; lCol < ncol; lCol++ )
                                {
                                    if( !strcmp( fieldnames[lCol], newcolumnname ) )
                                    {
                                        unique = false;
                                        break;
                                    }
                                }
                                if( unique ) break;
                            }
                            if( k == 100 )
                            {
                                mexWarnMsgTxt( MSG_MSGUNIQUEWARN );
                            }
                            else
                            {
                                /*
                                 * New unique Identifier found, assign it
                                 */
                                delete [] fieldnames[jCol];
                                fieldnames[jCol] = newcolumnname;
                            }
                        }
                    }
                }
            }
            /*
             * get the result rows from the engine
             *
             * We cannot get the number of result lines, so we must
             * read them in a loop and save them into an temporary list.
             * Later, we can transfer this List into an MATLAB array of structs.
             * This way, we must allocate enough memory for two result sets,
             * but we save time by allocating the MATLAB Array at once.
             */
            for(;;)
            {
                /*
                 * Advance to the next row
                 */
                int step_res = sqlite3_step( st );

                if( step_res == SQLITE_ERROR )
                {
                    g_finalize_msg = SQL_ERR;
                }

                /*
                 * no row left? break out of the loop
                 */
                if( step_res != SQLITE_ROW ) break;

                /*
                 * get new memory for the result
                 */
                Values* RecordValues = new Values( ncol );
                
                Value *v = RecordValues->m_Values;
                for( int jCol = 0; jCol < ncol; jCol++, v++ )
                {
                     int fieldtype = sqlite3_column_type( st, jCol );

                     v->m_Type = fieldtype;
                     v->m_Size = 0;
                     
                     switch( fieldtype )
                     {
                         case SQLITE_NULL:      v->m_NumericValue = g_NaN;                                                       break;
                         case SQLITE_INTEGER:   v->m_NumericValue = (double)sqlite3_column_int64( st, jCol );                    break;
                         case SQLITE_FLOAT:     v->m_NumericValue = (double)sqlite3_column_double( st, jCol );                   break;
                         case SQLITE_TEXT:      v->m_StringValue  = strnewdup( (const char*)sqlite3_column_text( st, jCol ) );   break;
                         case SQLITE_BLOB:      
                            {
                                v->m_Size = sqlite3_column_bytes( st, jCol );
                                if( v->m_Size > 0 )
                                {
                                    v->m_StringValue = new char[v->m_Size];
                                    memcpy( v->m_StringValue, sqlite3_column_blob( st, jCol ), v->m_Size );
                                }
                                else
                                {
                                    v->m_Size = 0;
                                }
                            }
                            break;
                         default:
                            FINALIZE( MSG_UNKNWNDBTYPE );
                     }
                }
                /*
                 * and add this row to the list of all result rows
                 */
                if( !lastrow )
                {
                    allrows = lastrow = RecordValues;
                }
                else
                {
                    lastrow->m_NextValues = RecordValues;
                    lastrow = lastrow->m_NextValues;
                }
                /*
                 * we have one more...
                 */
                rowcount++;
            }
            
            /*
             * end the sql engine
             */
            sqlite3_finalize( st );
            st = NULL;

            /*
             * got nothing? return an empty result to MATLAB
             */
            if( rowcount == 0 || ! allrows )
            {
                plhs[0] = mxCreateDoubleMatrix( 0, 0, mxREAL );
                FINALIZE_IF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
            }
            else
            {
                /*
                 * Allocate an array of MATLAB structs to return as result
                 */
                int ndims[2];
                int index;
                
                ndims[0] = rowcount;
                ndims[1] = 1;
                
                plhs[0] = mxCreateStructArray( 2, ndims, ncol, (const char**)fieldnames );
                FINALIZE_IF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
                
                /*
                 * transfer the result rows from the temporary list into the result array
                 */
                lastrow = allrows;
                index = 0;
                while( lastrow )
                {
                    Value* recordvalue = lastrow->m_Values;
                    
                    for( int fieldnr = 0; fieldnr < ncol; fieldnr++, recordvalue++ )
                    {
                        if( recordvalue -> m_Type == SQLITE_TEXT )
                        {
                            mxArray* c = mxCreateString( recordvalue->m_StringValue );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, c );
                        }
                        else if( recordvalue->m_Type == SQLITE_NULL && !g_NULLasNaN )
                        {
                            mxArray* out_double = mxCreateDoubleMatrix( 0, 0, mxREAL );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                        }
                        else if( recordvalue->m_Type == SQLITE_BLOB )
                        {
                            if( recordvalue->m_Size > 0 )
                            {
                                if( TYBLOB_NO == g_typed_blobs_mode )
                                {
                                    mwSize NumDims[2]={1,1};
                                    NumDims[1] = recordvalue->m_Size;
                                    mxArray* out_uchar8 = mxCreateNumericArray( 2, NumDims, mxUINT8_CLASS, mxREAL );
                                    unsigned char *v = (unsigned char *) mxGetData( out_uchar8 );
                                   
                                    memcpy( v, recordvalue->m_StringValue, recordvalue->m_Size );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, out_uchar8 );
                                } 
                                else 
                                {
                                    void*    blob         = (void*)recordvalue->m_StringValue;
                                    size_t   blob_size    = (size_t)recordvalue->m_Size;
                                    mxArray* pFieldItem;
                                    double   process_time = 0.0;
                                    double   ratio = 0.0;
                                    
                                    /* blob_unpack() modifies g_finalize_msg */
                                    FINALIZE_IF( !blob_unpack( blob, blob_size, &pFieldItem, &process_time, &ratio ), g_finalize_msg );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, pFieldItem );
                                }
                            } 
                            else 
                            {
                                // empty BLOB
                                mxArray* out_double = mxCreateDoubleMatrix( 0, 0, mxREAL );
                                mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                            }
                        } 
                        else 
                        {
                            mxArray* out_double = mxCreateDoubleScalar( recordvalue->m_NumericValue );
                            mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                        }
                    }
                    allrows = lastrow;
                    lastrow = lastrow->m_NextValues;
                    delete allrows;
                    index++;
                }
            }
            
            for( int iCol=0; iCol<ncol; iCol++ )
            {
                delete [] fieldnames[iCol];
            }
            delete [] fieldnames;
        }
        else
        {
            /*
             * no result, cleanup the sqlite engine
             */
            int res = sqlite3_step( st );
            sqlite3_finalize( st );
            st = NULL;

            plhs[0] = mxCreateDoubleMatrix( 0, 0, mxREAL );
            FINALIZE_IF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
            FINALIZE_IF( SQLITE_DONE != res, SQL_ERR );
        }
    }
        
finalize:        
    if( st )
    {
        sqlite3_clear_bindings( st );
        sqlite3_finalize( st );
    }
    
    free_ptr( command );  
    destroy_array( pArgs );

    // mexErrMsg*() functions automatically free all 
    // allocated memory by mxCalloc() ans mxCreate*() functions.

    if( g_finalize_msg == SQL_ERR || g_finalize_msg == SQL_ERR_CLOSE )
    {
        const char *msg_id = trans_err_to_ident( g_dbs[db_id] );
        char msg_err[1024];
        
        _snprintf( msg_err, 1024, "SQLite: \"%s\"", sqlite3_errmsg( g_dbs[db_id] ) );
        
        if( g_finalize_msg == SQL_ERR_CLOSE )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } 
            else 
            {
                mexPrintf( "%s\n", MSG_ERRCANTCLOSE );
            }
        }
        
        mexErrMsgIdAndTxt( msg_id, msg_err );
    }
    
    if( g_finalize_msg )
    {
        mexErrMsgTxt( g_finalize_msg );
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* sqlite function extensions, implementations */

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
    
    if( g_endian[0] != 'L' )
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
            
            destroy_array( pItem );
            sqlite3_result_double( ctx, ratio );
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

            blob_unpack( (void*)tbh2, blob_size, &pItem, &process_time, &ratio );

            destroy_array( pItem );
        }

        sqlite3_result_double( ctx, process_time );
        
    } 
    else 
    {
        sqlite3_result_error( ctx, "BDCUnpackTime(): only BLOB type supported!", -1 );
    }
}


// Time measuring functions
// Windows
#ifdef _WIN32
#include <windows.h>

double get_wall_time()
{
    LARGE_INTEGER time,freq;
    
    if( !QueryPerformanceFrequency( &freq ) )
    {
        // Handle error
        return 0;
    }
    
    if( !QueryPerformanceCounter( &time ) )
    {
        // Handle error
        return 0;
    }
    
    return (double)time.QuadPart / freq.QuadPart;
}


double get_cpu_time()
{
    FILETIME a,b,c,d;
    if( GetProcessTimes( GetCurrentProcess(), &a, &b, &c, &d ) != 0 )
    {
        //  Returns total user time.
        //  Can be tweaked to include kernel times as well.
        return
            (double)( d.dwLowDateTime |
            ( (unsigned long long)d.dwHighDateTime << 32 ) ) * 0.0000001;
    } 
    else 
    {
        //  Handle error
        return 0;
    }
}

//  Posix/Linux
#else
#include <time.h>
#include <sys/time.h>
double get_wall_time()
{
    struct timeval time;
    
    if( gettimeofday( &time, NULL ) )
    {
        //  Handle error
        return 0;
    }
    
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

double get_cpu_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}
#endif


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* compressor class */

class Compressor 
{
public:
    // categories
    typedef enum
    {
        E_NONE = 0,
        E_BLOSC,
        E_QLIN16,
        E_QLOG16,
    } compressor_type_e;
    
    const char*             m_strCompressorType;  // name of compressor to use
    compressor_type_e       m_eCompressorType;    // enum type of compressor to use
    int                     m_iCompressionLevel;  // compression level (0 to 9)
    mxArray*                m_pItem;              // data to compress (may be a copy)
    bool                    m_bIsByteStream;      // flag, true if it's a byte stream
    size_t                  m_szElement;          // size of one element in bytes
    size_t                  m_cntElements;        // number of elements
    mwSize                  m_nDims;              // number of data dimensions
    void*                   m_rdata;              // raw data, pointing tp pItemCopy data
    size_t                  m_rdata_size;         // size of raw data in bytes
    void*                   m_cdata;              // compressed data for blob storage
    size_t                  m_cdata_size;         // size of compressed data in bytes  

private:
    // inhibit copy constructor and assignment operator
    Compressor( const Compressor& );
    Compressor& operator=( const Compressor& );
    
public:
    // constructor
    Compressor()
    {
        m_strCompressorType   = NULL;
        m_eCompressorType     = E_NONE;
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
        if( 0 == iCompressionLevel || NULL == strCompressorType )
        {
            m_eCompressorType = E_NONE;
            m_strCompressorType = NULL;
            m_iCompressionLevel = 0;
        }
        else
        {
            if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4_ID ) )
            {
                m_eCompressorType = E_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, BLOSC_LZ4HC_ID ) )
            {
                m_eCompressorType = E_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, BLOSC_DEFAULT_ID ) )
            {
                m_eCompressorType = E_BLOSC;
            }
            else if( 0 == _strcmpi( strCompressorType, QLIN16_ID ) )
            {
                m_eCompressorType = E_QLIN16;
            }
            else if( 0 == _strcmpi( strCompressorType, QLOG16_ID ) )
            {
                m_eCompressorType = E_QLOG16;
            } 
            else
            {
                m_strCompressorType = NULL;
                return false;
            }
            
            m_strCompressorType = strCompressorType;
            m_iCompressionLevel = iCompressionLevel;
        }

        if( m_eCompressorType == E_BLOSC )
        {
            blosc_set_compressor( m_strCompressorType );
        }
        
        return true;
    }
    
    
    // adopt a Matlab variable (item) and its properties
    bool attachItem( const mxArray* pItem )
    {
        m_pItem = const_cast<mxArray*>( pItem );
        
        // save item properties
        if( NULL != m_pItem )
        {
            m_szElement   = mxGetElementSize( m_pItem );        
            m_cntElements = mxGetNumberOfElements( m_pItem );   
            m_nDims       = mxGetNumberOfDimensions( m_pItem ); 
            m_rdata       = const_cast<void*>( mxGetData( m_pItem ) );  // allow inplace modification
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
    bool attachStreamableCopy( const mxArray* pItem )
    {
        assert( NULL != pItem && NULL == m_pItem );

        m_bIsByteStream = false;

        // structural data must be converted into byte stream
        if( TC_COMPLEX == get_type_complexity( pItem ) )
        {
            mxArray* pByteStream = NULL;

            // typed BLOBs only support numeric (non-complex) arrays or strings,
            // but if the user allows undocumented ByteStreams, they can be stored 
            // either.
            if( TYBLOB_BYSTREAM == g_typed_blobs_mode )
            {
                serialize( pItem, pByteStream );
            }

            if( !pByteStream )
            {
                SETERR( MSG_UNSUPPVARTYPE );
                return false;
            }

            attachItem( pByteStream );
            m_bIsByteStream = true;
        } 
        else 
        {
            // Check if data type is supported by typed blobs
            if( !TypedBLOBHeaderBase::validClsid( pItem ) )
            {
                SETERR( MSG_UNSUPPVARTYPE );
                return false;
            }

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
            SETERR( MSG_ERRMEMORY );
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
        if( blosc_nbytes != TypedBLOBHeaderBase::getDataSize( m_pItem ) )
        {
            SETERR( MSG_ERRCOMPRESSION );
            return false;
        }
        
        // decompress directly into item
        if( blosc_decompress( m_cdata, m_rdata, m_rdata_size ) <= 0 )
        {
            SETERR( MSG_ERRCOMPRESSION );
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
            SETERR( MSG_ERRCOMPRARG );
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
            SETERR( MSG_ERRCOMPRLOGMINVALS );
            return false;
        }

        // compressor converts each value to uint16_t
        // 2 additional floats for offset and scale
        m_cdata_size = 2 * sizeof( float ) + m_cntElements * sizeof( uint16_t );  
        m_cdata      = mxMalloc( m_cdata_size );

        if( NULL == m_cdata )
        {
            SETERR( MSG_ERRMEMORY );
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
            SETERR( MSG_ERRCOMPRARG );
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
          case E_BLOSC:
            return bloscCompress();
            
          case E_QLIN16:
            return linlogQuantizerCompress( /* bDoLog*/ false );
            
          case E_QLOG16:
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
          case E_BLOSC:
            return bloscDecompress();
            
          case E_QLIN16:
            return linlogQuantizerDecompress( /* bDoLog*/ false );
            
          case E_QLOG16:
            return linlogQuantizerDecompress( /* bDoLog*/ true );
            
          default:
            return false;
        }
    }
};


// create a compressed typed blob from a Matlab variable (deep copy)
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_size, double *pdProcess_time, double* pdRatio )
{
    assert( NULL != ppBlob && NULL != pBlob_size && NULL != pdProcess_time && NULL != pdRatio );
    *ppBlob         = NULL;
    *pBlob_size     = 0;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    Compressor bag;
    
    /* 
     * create a typed blob. Header information is generated
     * according to value and type of the matrix and the machine
     */
    // setCompressor() always returns true, since parameters had been checked already
    (void)bag.setCompressor( g_compression_type, g_compression_level );
    FINALIZE_IF( !bag.attachStreamableCopy( pItem ), g_finalize_msg );

    if( g_compression_level )
    {
        double start_time = get_wall_time();
        
        bag.pack(); // allocates m_cdata
        
        *pdProcess_time = get_wall_time() - start_time;
        
        // did the compressor ommits compressed data?
        if( bag.m_cdata_size > 0 )
        {
            size_t blob_size_uncompressed;
            
            *pBlob_size = 
                TypedBLOBHeaderV2::dataOffset( bag.m_nDims ) +
                bag.m_cdata_size;
            
            blob_size_uncompressed =
                TypedBLOBHeaderV1::dataOffset( bag.m_nDims ) +
                bag.m_rdata_size;
            
            // calculate the compression ratio
            if( blob_size_uncompressed > 0 )
            {
                *pdRatio = (double)*pBlob_size / (double)blob_size_uncompressed;
            }
            else
            {
                *pdRatio = 0.0;
            }
            
            if( *pBlob_size >= blob_size_uncompressed )
            {
                bag.m_cdata_size = 0; // Switch zu uncompressed blob, it's not worth the efford
            }
        }

        // is there compressed data to store in the blob?
        if( bag.m_cdata_size >  0 )
        {
            TypedBLOBHeaderV2* tbh2 = NULL;
            
            // discard data if it exeeds max allowd size by sqlite
            if( *pBlob_size > MKSQLITE_MAX_BLOB_SIZE )
            {
                FINALIZE( MSG_BLOBTOOBIG );
            }

            // allocate space for a typed blob containing compressed data
            tbh2 = (TypedBLOBHeaderV2*)sqlite3_malloc( (int)*pBlob_size );
            if( NULL == tbh2 )
            {
                FINALIZE( MSG_ERRMEMORY );
            }

            // blob typing...
            tbh2->init( bag.m_pItem );
            tbh2->setCompressor( bag.m_strCompressorType );

            // ...and copy compressed data
            // TODO: Do byteswapping here if big endian? 
            // (Most platforms use little endian)
            memcpy( (char*)tbh2->getData(), bag.m_cdata, bag.m_cdata_size );

            // check if compressed data equals to original?
            if( g_compression_check )
            {
                int check_id = 0xdeadbeef;
                size_t rdata_size   = bag.m_rdata_size;
                void* rdata         = mxMalloc( rdata_size + 2 * sizeof( check_id ) );
                char* pData         = (char*)rdata;
                int ok              = 0;

                if( NULL == rdata )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( MSG_ERRMEMORY );
                }

                // mark data bounds to check for range violation
                memcpy( pData, &check_id, sizeof( check_id ) );
                pData += sizeof( check_id );
                memcpy( pData + rdata_size, &check_id, sizeof( check_id ) );
                
                // unpacking data to pData
                bag.m_rdata = pData;
                
                // inflate compressed data again
                if( !bag.unpack() )
                {
                    sqlite3_free( tbh2 );

                    free_ptr( rdata );  // Needless due to mexErrMsgTxt(), but clean
                    FINALIZE( MSG_ERRCOMPRESSION );
                }

                // check if data is equal and bounds are not violated
                ok = (    rdata_size == bag.m_rdata_size 
                       && 0 == memcmp( bag.m_rdata, pData, rdata_size ) 
                       && 0 == memcmp( rdata,               &check_id, sizeof( check_id ) )
                       && 0 == memcmp( pData + rdata_size, &check_id, sizeof( check_id ) ) );

                free_ptr( rdata );

                if( !ok )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( MSG_ERRCOMPRESSION );
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
        *pBlob_size = TypedBLOBHeaderV1::dataOffset( bag.m_nDims ) +
                       bag.m_rdata_size;

        FINALIZE_IF( *pBlob_size > MKSQLITE_MAX_BLOB_SIZE, MSG_BLOBTOOBIG );

        tbh1 = (TypedBLOBHeaderV1*)sqlite3_malloc( (int)*pBlob_size );
        if( NULL == tbh1 )
        {
            FINALIZE( MSG_ERRMEMORY );
        }

        // blob typing...
        tbh1->init( bag.m_pItem );

        // and copy uncompresed data
        // TODO: Do byteswapping here if big endian? 
        // (Most platforms use little endian)
        memcpy( tbh1->getData(), bag.m_rdata, bag.m_rdata_size );

        *ppBlob = (void*)tbh1;
    }
    
    // mark data type as "unknown", means that it's a serialized item as byte stream
    if( bag.m_bIsByteStream )
    {
        ((TypedBLOBHeaderV1*)*ppBlob)->m_clsid = mxUNKNOWN_CLASS;
    }
    
finalize:
  
    // cleanup
    // m_rdata points to m_pItem data structure and may not be freed!
    bag.m_rdata = NULL;
    free_ptr( bag.m_cdata );
    destroy_array( bag.m_pItem );
    
    return NULL != *ppBlob;
}


// uncompress a typed blob and return its matlab variable
int blob_unpack( const void* pBlob, size_t blob_size, mxArray** ppItem, double* pdProcess_time, double* pdRatio )
{
    typedef TypedBLOBHeaderV1 tbhv1_t;
    typedef TypedBLOBHeaderV2 tbhv2_t;
    
    assert( NULL != ppItem && NULL != pdProcess_time && NULL != pdRatio );
    *ppItem         = NULL;
    *pdProcess_time = 0.0;
    *pdRatio        = 1.0;
    
    tbhv1_t* tbh1 = (tbhv1_t*)pBlob;
    tbhv2_t* tbh2 = (tbhv2_t*)pBlob;
    
    Compressor bag;

    /* test valid platform */
    if( !tbh1->validPlatform() )
    {
        mexWarnMsgIdAndTxt( "MATLAB:MKSQLITE:BlobDiffArch", MSG_WARNDIFFARCH );
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
        FINALIZE( MSG_UNSUPPTBH );
    }

    FINALIZE_IF( !tbh1->validClsid(), MSG_UNSUPPVARTYPE );
    
    // serialized item marked as "unknown" is a byte stream
    bag.m_bIsByteStream = ( tbh1->m_clsid == mxUNKNOWN_CLASS );

    switch( tbh1->m_ver )
    {
      // typed blob with uncompressed data
      case sizeof( tbhv1_t ):
      {
          // space allocation (check later)
          bag.attachItem( tbh1->createNumericArray( /* doCopyData */ true ) );
          break;
      }

      // typed blob with compressed data
      case sizeof( tbhv2_t ):
      {
          // space allocation (check later)
          bag.attachItem( tbh2->createNumericArray( /* doCopyData */ false ) );
          
          bag.m_cdata        = tbh2->getData();
          bag.m_cdata_size   = blob_size - tbh2->dataOffset();
          
          // space allocated?
          if( bag.m_pItem )
          {
              if( !tbh2->validCompression() )
              {
                  FINALIZE( MSG_UNKCOMPRESSOR );
              }

              bag.setCompressor( tbh2->m_compression );

              double start_time = get_wall_time();
              
              if( !bag.unpack() )
              {
                  FINALIZE( MSG_ERRCOMPRESSION );
              } 
              else 
              {
                  *pdProcess_time = get_wall_time() - start_time;
                  
                  if( bag.m_rdata_size > 0 )
                  {
                      *pdRatio = (double)bag.m_cdata_size / (double)bag.m_rdata_size;
                  }
                  else
                  {
                      *pdRatio = 0.0;
                  }
                  
                  // TODO: Do byteswapping here if needed, depend on endian?
              }
          }
          break;
      }

      default:
          FINALIZE( MSG_UNSUPPTBH );
    }

    FINALIZE_IF( NULL == bag.m_pItem, MSG_ERRMEMORY );
    
    if( bag.m_bIsByteStream  )
    {
        mxArray* pDeStreamed = NULL;
        
        if( !deserialize( bag.m_pItem, pDeStreamed ) )
        {
            FINALIZE( MSG_ERRMEMORY );
        }
        
        destroy_array( bag.m_pItem );
        bag.attachItem( pDeStreamed );
    }

    *ppItem = bag.m_pItem;
    bag.m_pItem = NULL;
      
finalize:
    
    // cleanup
    // rdata and cdata points to memory allocated outside
    bag.m_rdata = NULL;
    bag.m_cdata = NULL;

    // bag.m_pItem is non-NULL when an error occured only
    destroy_array( bag.m_pItem );

    return NULL != *ppItem;
}


// convert Matlab variable to byte stream
bool serialize( const mxArray* pItem, mxArray*& pByteStream )
{
    assert( NULL == pByteStream && NULL != pItem );
    
    if( have_serialize() )
    {
        mexCallMATLAB( 1, &pByteStream, 1, const_cast<mxArray**>( &pItem ), "getByteStreamFromArray" ) ;
    }
    
#if EARLY_BIND_SERIALIZE
    pByteStream = mxSerialize( pItem );
#endif
    
    return NULL != pByteStream;
}


// convert byte stream to Matlab variable
bool deserialize( const mxArray* pByteStream, mxArray*& pItem )
{
    assert( NULL != pByteStream && NULL == pItem );
    
    if( have_serialize() )
    {
        mexCallMATLAB( 1, &pItem, 1, const_cast<mxArray**>( &pByteStream ), "getArrayFromByteStream" );
        return NULL != pItem;
    }
    
#if EARLY_BIND_SERIALIZE
    pItem = mxDeserialize( mxGetData( pByteStream ), mxGetNumberOfElements( pByteStream ) );
#endif

    return NULL != pItem;
}


// return true, if Matlab version have function getByteStreamFromArray()
bool have_serialize()
{
    static int flagHaveSerialize = -1;
    
    if( flagHaveSerialize < 0 ) 
    {
        mxArray* pResult = NULL;
        mxArray* pFuncName = mxCreateString( "getByteStreamFromArray" );

        flagHaveSerialize =    pFuncName
                            && 0 == mexCallMATLAB( 1, &pResult, 1, &pFuncName, "exist" )
                            && pResult
                            && 5 == get_integer( pResult );

        destroy_array( pFuncName );
        destroy_array( pResult );
    }
    
    return flagHaveSerialize > 0;
}


// return true, if Matlab version supports serialization
bool can_serialize()
{
#if EARLY_BIND_SERIALIZE
    return true
#endif
    return have_serialize();
}

/*
 *
 * Formatierungsanweisungen für den Editor vim
 *
 * vim:ts=4:ai:sw=4
 */

