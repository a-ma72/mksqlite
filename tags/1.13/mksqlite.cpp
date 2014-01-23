/*
 * mksqlite: A MATLAB Interface To SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
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
#else
  #include <string.h>
  #define _strcmpi strcasecmp
  #define _snprintf snprintf
  #include <ctype.h>
  #include "mex.h"
#endif

#include <math.h>
#include <assert.h>
#include <limits.h>
#include "sqlite/sqlite3.h"
#define SQLITE_VERSION_STRING SQLITE_VERSION
#include "deelx/deelx.h"
#define DEELX_VERSION_STRING "1.2"
extern "C"
{
  #include "blosc/blosc.h"
  #include "md5/md5.h"  /* little endian only! */
}

/* Versionstring */
#define MKSQLITE_VERSION_STRING "1.13"

/* Default Busy Timeout */
#define DEFAULT_BUSYTIMEOUT 1000

/* get the SVN Revisionnumber */
#include "svn_revision.h"

// Data representation 
//(ref http://www.agner.org/optimize/calling_conventions.pdf, chapter 3)
// #include <stdint.h> doesn't compile with current MSVC and older matlab versions
#ifndef int32_t
#if UINT_MAX > 65535
  typedef signed   int         int32_t;  // int is usually 32 bits long
  typedef unsigned int         uint32_t; 
#else
  typedef signed   long        int32_t;  // except on 16-Bit Windows platform
  typedef unsigned long        uint32_t; 
#endif 
#endif
#ifndef int8_t
  typedef signed   char        int8_t;
  typedef unsigned char        uint8_t;
#endif
#ifndef int16_t
  typedef signed   short int   int16_t;
  typedef unsigned short int   uint16_t;
#endif
#ifndef INT32_MAX
#define INT32_MAX (0x3FFFFFFF)
#endif

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
#define FINALIZE( msg ) { g_finalize_msg = msg; goto finalize; }
#define FINALIZEIF( cond, msg ) { if(cond) FINALIZE( msg ) }

/* declare the MEX Entry function as pure C */
extern "C" void mexFunction( int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[] );

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
static int g_compression_level = 0;           // no compression by default
static const char* g_compression_type = NULL; // compressor type
static int g_compression_check = 1;           // Flag: check compressed against original data

/* Convert UTF-8 to ascii, otherwise set slCharacterEncoding('UTF-8') */
static bool g_convertUTF8 = true;

/* Store type and dimensions of MATLAB vectors/arrays in BLOBs 
 * native and free of matlab types, to provide data sharing 
 * with other applications
 */
static bool g_use_typed_blobs = false;

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

// typed BLOB header agreement
// typed_BLOB_header_base is the unique and mandatory header prelude for typed blob headers
struct typed_BLOB_header_base 
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
  
  bool valid_magic()
  {
    return 0 == strncmp( m_magic, TBH_MAGIC, TBH_MAGIC_MAXLEN );
  }
  
  static
  bool valid_clsid( mxClassID clsid )
  {
    switch( clsid )
    {
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
        return false;
    }
  }
  
  static
  bool valid_clsid( const mxArray* pItem )
  {
    return pItem && valid_clsid( mxGetClassID( pItem ) );
  }
  
  bool valid_clsid()
  {
    return valid_clsid( (mxClassID)m_clsid );
  }
  
  bool valid_platform()
  {
    return g_endian[0] == m_endian && 0 == strncmp( g_platform, m_platform, PLATFORM_MAXLEN );
  }
  
  static
  size_t data_bytes( const mxArray* pItem )
  {
    size_t data_bytes = 0;
    
    if( pItem )
    {
      size_t szElement   = mxGetElementSize( pItem );
      size_t cntElements = mxGetNumberOfElements( pItem );
      
      data_bytes = szElement * cntElements;
    }
    
    return data_bytes;
  }
};


// 2nd version with compression feature
/* 
 * IMPORTANT: NEVER ADD VIRTUAL FUNCTIONS TO HEADER CLASSES DERIVED FROM BASE!
 * Reason: Size of struct wouldn't match since a hidden vtable pointer 
 *         would be attached then!
 */
struct typed_BLOB_header_blosc : public typed_BLOB_header_base {
  /* name of the compression algorithm used. Other algorithms
   * possible in future..?
   */
  char m_compression[12];
  
  void init( mxClassID clsid )
  {
    typed_BLOB_header_base::init( clsid );
    strcpy( m_compression, "" );
  }
  
  bool valid_compression()
  {
    return 1;
  }
};


/* Template to append data and its dimensions uniquely to a typed BLOB header */
template< typename header_base >
struct TBH_data : public header_base
{
  // Number of dimensions, followed by sizes of each dimension
  int32_t m_nDims[1];  
  // (BLOB data follows after last dimension size...)
  
  void init( mxClassID clsid, mwSize nDims, const mwSize* pSize )
  {
    header_base::init( clsid );
    header_base::m_ver = sizeof( *this );
    
    assert( nDims >= 0 );
    assert( !nDims || pSize );
    
    m_nDims[0] = nDims;
    for( int i = 0; i < nDims; i++ )
    {
      m_nDims[i+1] = (int32_t)pSize[i];
    }
  }
  
  void init( const mxArray* pItem )
  {
    assert( pItem );
    mxClassID clsid = mxGetClassID( pItem );
    mwSize nDims = mxGetNumberOfDimensions( pItem );
    const mwSize* dimensions = mxGetDimensions( pItem );
    
    init( clsid, nDims, dimensions );
  }
  
  bool valid_ver()
  {
      return sizeof(*this) == (size_t)header_base::m_ver;
  }
  
  void* data( mwSize nDims )
  {
    return (void*)&m_nDims[ nDims + 1 ];
  }
  
  void* data()
  {
    return data( m_nDims[0] );
  }
  
  static
  size_t data_offset( mwSize nDims )
  {
    //return offsetof( TBH_data, m_nDims[nDims+1] ); /* doesn't work on linux gcc 4.1.2 */
    TBH_data* p = (TBH_data*)1024;
    return (char*)&p->m_nDims[nDims+1] - (char*)p;
  }
  
  size_t data_offset()
  {
    return data_offset( m_nDims[0] );
  }
  
  size_t data_bytes()
  {
    mwSize nDims = (mwSize)m_nDims[0];
    mxArray* pItem = NULL;
    size_t data_bytes = 0;
    
    pItem = nDims ? mxCreateNumericMatrix( 1, 1, this->m_clsid, mxREAL ) : NULL;
    
    if( pItem )
    {
      size_t data_bytes = mxGetElementSize( pItem );
    
      for( mwSize i = 0; i < nDims; i++ )
      {
          data_bytes *= (mwSize)m_nDims[i+1];
      }
      
      mxDestroyArray( pItem );
    }
    
    return data_bytes;
  }
  
  mxArray* CreateNumericArray( bool doCopyData )
  {
    mwSize nDims = m_nDims[0];
    mwSize* dimensions = new mwSize[nDims];
    mxArray* pItem = NULL;
    
    for( int i = 0; i < nDims; i++ )
    {
        dimensions[i] = (mwSize)m_nDims[i+1];
    }
    
    pItem = mxCreateNumericArray( nDims, dimensions, (mxClassID)this->m_clsid, mxREAL );
    delete[] dimensions;
    
    if( pItem && doCopyData )
    {
      memcpy( mxGetData( pItem ), data(), typed_BLOB_header_base::data_bytes( pItem ) );
    }
    
    return pItem;
  }
  
private: 
  // Disabling create instances! 
  // Scheme represented by this template, and others derived, do only shadow 
  // memory got from allocators (malloc)
  TBH_data();
  TBH_data( const TBH_data& );
  TBH_data& operator=( const TBH_data& );

};



typedef TBH_data<typed_BLOB_header_base>  typed_BLOB_header_v1;
typedef TBH_data<typed_BLOB_header_blosc> typed_BLOB_header_v2;

/* Static assertion: Ensure backward compatibility */
static char SA_TBH_BASE[ sizeof( typed_BLOB_header_v1 ) == 36 ? 1 : -1 ];


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
    typedef struct {
      char magic[sizeof(TBH_MAGIC)];  // small fail-safe header check
      int16_t ver;                    // Struct size as kind of header version number for later backwards compatibility (may increase only!)
      int32_t clsid;                  // Matlab ClassID of variable (see mxClassID)
      char platform[11];              // Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
      char endian;                    // Byte order: 'L'ittle endian or 'B'ig endian
      int32_t sizeDims[1];            // Number of dimensions, followed by sizes of each dimension
                                      // First byte after header at &tbh->sizeDims[tbh->sizeDims[0]+1]
    } typed_BLOB_header;

#define TBH_DATA(tbh)            ((void*)&tbh->sizeDims[tbh->sizeDims[0]+1])
#define TBH_DATA_OFFSET(nDims)   ((ptrdiff_t)&((typed_BLOB_header*) 0)->sizeDims[nDims+1])

    int check_compatibility()
    {
        typed_BLOB_header_v1* tbh1         = (typed_BLOB_header_v1*)1024;
        typed_BLOB_header*    old_struct   = (typed_BLOB_header*)1024;

        if(     (void*)&tbh1->m_ver          == (void*)&old_struct->ver
            &&  (void*)&tbh1->m_clsid        == (void*)&old_struct->clsid
            &&  (void*)&tbh1->m_platform[0]  == (void*)&old_struct->platform[0]
            &&  (void*)&tbh1->m_endian       == (void*)&old_struct->endian
            &&  (void*)&tbh1->m_nDims        == (void*)&old_struct->sizeDims
            &&   TBH_DATA_OFFSET(2)          == typed_BLOB_header_v1::data_offset(2) )
        {
            return 1;
        } else {
            return 0;
        }
    }

#undef TBH_DATA
#undef TBH_DATA_OFFSET
};



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* SQLite function extensions by mksqlite */
void powFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void regexFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDCRatioFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDCPackTimeFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void BDCUnpackTimeFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv );
void MD5Func( sqlite3_context *ctx, int argc, sqlite3_value **argv );

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* further forward declarations */

// (un-)packing functions
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_bytes, double* pProcess_time );
int blob_unpack( const void* pBlob, size_t blob_bytes, mxArray** ppItem, double* pProcess_time );

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/*
 * a poor man localization.
 * every language have an table of messages.
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
#define MSG_MISSINGARG          messages[Language][17]
#define MSG_ERRMEMORY           messages[Language][18]
#define MSG_UNSUPPVARTYPE       messages[Language][19]
#define MSG_UNSUPPTBH           messages[Language][20]   
#define MSG_ERRPLATFORMDETECT   messages[Language][21]
#define MSG_WARNDIFFARCH        messages[Language][22]
#define MSG_BLOBTOOBIG          messages[Language][23]
#define MSG_ERRCOMPRESSION      messages[Language][24]
#define MSG_UNKCOMPRESSOR       messages[Language][25]
#define MSG_ERRUNKOPENMODE      messages[Language][26]
#define MSG_ERRUNKTHREADMODE    messages[Language][27]
#define MSG_ERRCANTCLOSE        messages[Language][28]
#define MSG_ERRCLOSEDBS         messages[Language][29]


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
    "unknown open modus (only 'ro', 'rw' or 'rwc' accepted)",
    "unknown threading mode (only 'single', 'multi' or 'serial' accepted)",
    "cannot close connection",
    "not all connections could be closed"
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
    "unbekannzer Zugriffmodus (nur 'ro', 'rw' oder 'rwc' möglich)",
    "unbekannter Threadingmodus (nur 'single', 'multi' oder 'serial' möglich)",
    "die Datenbank kann nicht geschlossen werden",
    "nicht alle Datenbanken konnten geschlossen werden"
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
    virtual    ~Value()    { if( m_StringValue ) delete [] m_StringValue; } 
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

void DestroyArray( mxArray *&pmxarr )
{
    if( pmxarr )
    {
        mxDestroyArray( pmxarr );
        pmxarr = NULL;
    }
}

template <class T>
void Free( T *&pmxarr )
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
static void CloseDBs( void )
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
            } else {
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
static const char* TransErrToIdent( sqlite3 *db )
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
static char *getstring( const mxArray *a )
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
            Free( c ); // Needless due to mexErrMsgTxt(), but clean
            mexErrMsgTxt( MSG_CANTCOPYSTRING );
        }

        latin2utf( (unsigned char*)c, (unsigned char*)buffer );

        Free( c );

        return buffer;
    }
   
    return c;
}

/*
 * get an integer value from an numeric
 */
static int getinteger( const mxArray* a )
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
            
            DestroyArray( plhs[0] );
            DestroyArray( plhs[1] );
            DestroyArray( plhs[2] );
            
            mexAtExit( CloseDBs );
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
        db_id = getinteger( prhs[0] );
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
    command = getstring( prhs[CommandPos] );
    
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
        } else {
            plhs[0] = mxCreateString( MKSQLITE_VERSION_STRING );
        }
    } else if( !strcmp( command, "version sql" ) )
    {
        if( nlhs == 0 )
        {
            mexPrintf( "SQLite Version %s\n", SQLITE_VERSION_STRING );
        } else {
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
        
        // No Memoryleak 'command not freed' when getstring fails
        // Matlab Help:
        // "If your application called mxCalloc or one of the 
        // mxCreate* routines to allocate memory, mexErrMsgTxt 
        // automatically frees the allocated memory."
        char* dbname = getstring( prhs[FirstArg] );

        /*
         * Is there an database ID? The close the database with the same id 
         */
        if( db_id > 0 && g_dbs[db_id] )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } else {
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
            
            Free( dbname );  // Needless due to mexErrMsgTxt(), but clean
            FINALIZE( MSG_NOFREESLOT );
        }
        
        
        /*
         * Open mode (optional)
         */
        if( NumArgs >= 2 )
        {
            char* iomode = getstring( prhs[FirstArg+1] );
            
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
            } else {
                Free( iomode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_ERRUNKOPENMODE );
            }
            Free( iomode ); 
        } else {
            flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        }
        
        
        /*
         * Threading mode (optional)
         */
        if( NumArgs >= 3 )
        {
            char* threadmode = getstring( prhs[FirstArg+2] );
            
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
            } else {
                Free( threadmode );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_ERRUNKTHREADMODE );
            }
            Free( threadmode );
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
            
            Free( dbname );   // Needless due to mexErrMsgTxt(), but clean
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
            Free( dbname );   // Needless due to mexErrMsgTxt(), but clean
            mexPrintf( "%s\n", MSG_BUSYTIMEOUTFAIL );
            FINALIZE( SQL_ERR_CLOSE );
        }
        
        // attach new SQL commands to opened database
        sqlite3_create_function( g_dbs[db_id], "pow", 2, SQLITE_UTF8, NULL, powFunc, NULL, NULL );     // power function (math)
        sqlite3_create_function( g_dbs[db_id], "regex", 2, SQLITE_UTF8, NULL, regexFunc, NULL, NULL ); // regular expressions (MATCH mode)
        sqlite3_create_function( g_dbs[db_id], "regex", 3, SQLITE_UTF8, NULL, regexFunc, NULL, NULL ); // regular expressions (REPLACE mode)
        sqlite3_create_function( g_dbs[db_id], "bdcratio", 1, SQLITE_UTF8, NULL, BDCRatioFunc, NULL, NULL );     // compression ratio (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcpacktime", 1, SQLITE_UTF8, NULL, BDCPackTimeFunc, NULL, NULL );     // compression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "bdcunpacktime", 1, SQLITE_UTF8, NULL, BDCUnpackTimeFunc, NULL, NULL );     // decompression time (blob data compression)
        sqlite3_create_function( g_dbs[db_id], "md5", 1, SQLITE_UTF8, NULL, MD5Func, NULL, NULL );     // Message-Digest (RSA)
        
        /*
         * return value will be the used database id
         */
        plhs[0] = mxCreateDoubleScalar( (double)db_id + 1 );
        Free( dbname );
    }
    else if( !strcmp( command, "close" ) )
    {
        /*
         * close a database
         */

        /*
         * There should be no Argument to close
         */
        FINALIZEIF( NumArgs > 0, MSG_INVALIDARG );
        
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
                    } else {
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
            FINALIZEIF( !g_dbs[db_id], MSG_DBNOTOPEN );

            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } else {
                mexPrintf( "%s\n", MSG_ERRCANTCLOSE );
                FINALIZE( SQL_ERR );  /* not SQL_ERR_CLOSE */
            }
        }
    }
    else if( !strcmp( command, "status" ) )
    {
        /*
         * There should be no Argument to status
         */
        FINALIZEIF( NumArgs > 0, MSG_INVALIDARG );
        
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
        FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
        FINALIZEIF( !g_dbs[db_id], MSG_DBNOTOPEN );

        /*
         * Set Busytimeout
         */
        int TimeoutValue = getinteger( prhs[FirstArg] );

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
            FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_check4uniquefields = ( getinteger( prhs[FirstArg] ) ) ? true : false;
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
            FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_convertUTF8 = getinteger( prhs[FirstArg] ) ? true : false;
        }
    }
    else if( !_strcmpi( command, "typedBLOBs" ) )
    {
        if( NumArgs == 0 )
        {
            plhs[0] = mxCreateDoubleScalar( (double)g_use_typed_blobs );
        }
        else
        {
            FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_use_typed_blobs = getinteger( prhs[FirstArg] ) ? true : false;
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
            FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_NULLasNaN = getinteger( prhs[FirstArg] ) ? true : false;
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
            
            FINALIZEIF(     NumArgs != 2 
                        || !mxIsChar( prhs[FirstArg] )
                        || !mxIsNumeric( prhs[FirstArg+1] ), MSG_INVALIDARG );
            
            new_compressor = getstring( prhs[FirstArg] );
            new_compression_level = ( getinteger( prhs[FirstArg+1] ) );
            
            FINALIZEIF( new_compression_level < 0 || new_compression_level > 9, MSG_INVALIDARG );

            if( 0 == _strcmpi( new_compressor, BLOSC_LZ4_ID ) )
            {
                g_compression_type = BLOSC_LZ4_ID;
            } else if( 0 == _strcmpi( new_compressor, BLOSC_LZ4HC_ID ) )
            {
                g_compression_type = BLOSC_LZ4HC_ID;
            } else if( 0 == _strcmpi( new_compressor, BLOSC_DEFAULT_ID ) )
            {
                g_compression_type = BLOSC_DEFAULT_ID;
            } else {
                Free( new_compressor );
                FINALIZE( MSG_INVALIDARG );
            }
            
            Free( new_compressor );
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
            FINALIZEIF( NumArgs != 1 || !mxIsNumeric( prhs[FirstArg] ), MSG_INVALIDARG );
            g_compression_check = getinteger( prhs[FirstArg] ) ? true : false;
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
        FINALIZEIF( !g_dbs[db_id], MSG_DBNOTOPEN );
        
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
        FINALIZEIF( sqlite3_complete( query ) != 0, MSG_INVQUERY );
        
        /*
         * and prepare it
         * if anything is wrong with the query, than complain about it.
         */
        FINALIZEIF( SQLITE_OK != sqlite3_prepare_v2( g_dbs[db_id], query, -1, &st, 0 ), SQL_ERR );
        
        /*
         * Parameter binding 
         */
        int bind_names_count = sqlite3_bind_parameter_count( st );
        
        // If there are no placeholdes in the SQL statement, no
        // arguments are allowed. There must be at least one
        // placeholder.
        FINALIZEIF( !bind_names_count && NumArgs > 0, MSG_UNEXPECTEDARG );

        if( !NumArgs || !mxIsCell( prhs[FirstArg] ) )  // mxIsCell() not called when NumArgs==0 !
        {
            // Arguments passed as list, or no arguments
            // More parameters than needed is not allowed
            FINALIZEIF( NumArgs > bind_names_count, MSG_UNEXPECTEDARG ); 

            // Collect parameter list in one cell array
            pArgs = mxCreateCellMatrix( bind_names_count, 1 );
            for( int i = 0; pArgs && i < bind_names_count; i++ )
            {
                if( i < NumArgs )
                {
                    // deep copy into cell array
                    mxSetCell( pArgs, i, mxDuplicateArray( prhs[FirstArg+i] ) );
                }
                else
                {
                    // not passed arguments result in empty arrays
                    mxSetCell( pArgs, i, mxCreateLogicalMatrix( 0, 0 ) );
                }
            }
        }
        else
        {
            // Parameters may be (and are) packed in only one single 
            // cell array
            FINALIZEIF( NumArgs > 1, MSG_UNEXPECTEDARG );

            // Make a deep copy
            pArgs = mxDuplicateArray( prhs[FirstArg] );
        }
        
        // if parameters needed for parameter binding, 
        // at least one parameter has to be passed 
        FINALIZEIF( !pArgs, MSG_MISSINGARG );

        for( int iParam = 0; iParam < bind_names_count; iParam++ )
        {
            // item must not be destroyed! (Matlab crashes then)
            const mxArray *item = mxGetCell( pArgs, iParam );

            if( !item || mxIsEmpty( item ) )
                continue; // Empty parameters are omitted as NULL by sqlite

            if( mxIsComplex( item ) || mxIsCell( item ) || mxIsStruct( item ) )
            {
                // No complex values, nested cells or structs allowed
                FINALIZE( MSG_UNSUPPVARTYPE );
            }

            size_t    szElement   = mxGetElementSize( item );      // size of one element in bytes
            size_t    cntElements = mxGetNumberOfElements( item ); // number of elements in cell array
            mxClassID clsid       = mxGetClassID( item );
            char*     str_value   = NULL;

            if( cntElements > 1 && clsid != mxCHAR_CLASS )
            {
                if( !g_use_typed_blobs ){
                    // matrix arguments are omitted as blobs, except string arguments
                    // data is attached as is, no header information here
                    const void* blob = mxGetData( item );
                    // SQLite makes a lokal copy of the blob (thru SQLITE_TRANSIENT)
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, 
                                                        (int)(cntElements * szElement),
                                                        SQLITE_TRANSIENT ) )
                    {
                        FINALIZE( SQL_ERR );
                    }
                } else {
                    void*  blob       = NULL;
                    size_t blob_bytes = 0;
                    double pProcess_time;
                    
                    /* blob_pack() modifies g_finalize_msg */
                    FINALIZEIF( !blob_pack( item, &blob, &blob_bytes, &pProcess_time ), g_finalize_msg );
                    
                    // sqlite takes custody of the blob, even if sqlite3_bind_blob() fails
                    if( SQLITE_OK != sqlite3_bind_blob( st, iParam+1, blob, (int)blob_bytes, sqlite3_free ) )
                    {
                        FINALIZE( SQL_ERR );
                    }  
                }
            } else {
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
                        if( SQLITE_OK != sqlite3_bind_int( st, iParam+1, (int)mxGetScalar( item ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxDOUBLE_CLASS:
                    case mxSINGLE_CLASS:
                        // scalar floating point value
                        if( SQLITE_OK != sqlite3_bind_double( st, iParam+1, mxGetScalar( item ) ) )
                        {
                            FINALIZE( SQL_ERR );
                        }
                        break;
                    case mxCHAR_CLASS:
                    {
                        // string argument
                        char* str_value = mxArrayToString( item );
                        if( str_value && g_convertUTF8 )
                        {
                            int len = latin2utf( (unsigned char*)str_value, NULL ); // get the size only
                            unsigned char *temp = (unsigned char*)mxCalloc( len, sizeof(char) ); // allocate memory
                            if( temp )
                            {
                                latin2utf( (unsigned char*)str_value, temp );
                                Free( str_value );
                                str_value = (char*)temp;
                            }
                            else
                            {
                                Free( str_value );  // Needless due to mexErrMsgTxt(), but clean
                                FINALIZE( MSG_ERRMEMORY );
                            }
                        }
                        // SQLite makes a lokal copy of the blob (thru SQLITE_TRANSIENT)
                        if( SQLITE_OK != sqlite3_bind_text( st, iParam+1, str_value, -1, SQLITE_TRANSIENT ) )
                        {
                            Free( str_value );  // Needless due to mexErrMsgTxt(), but clean
                            FINALIZE( SQL_ERR );
                        }
                        Free( str_value );
                        break;
                    }
                    default:
                        // other variable classed are invalid here
                        FINALIZE( MSG_INVALIDARG );
                }
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
                FINALIZEIF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
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
                FINALIZEIF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
                
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
                                if( !g_use_typed_blobs )
                                {
                                    mwSize NumDims[2]={1,1};
                                    NumDims[1] = recordvalue->m_Size;
                                    mxArray* out_uchar8 = mxCreateNumericArray( 2, NumDims, mxUINT8_CLASS, mxREAL );
                                    unsigned char *v = (unsigned char *) mxGetData( out_uchar8 );
                                   
                                    memcpy( v, recordvalue->m_StringValue, recordvalue->m_Size );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, out_uchar8 );
                                } else {
                                    void*    blob       = (void*)recordvalue->m_StringValue;
                                    size_t   blob_bytes = (size_t)recordvalue->m_Size;
                                    mxArray* pItem;
                                    double   pProcess_time;
                                    
                                    /* blob_unpack() modifies g_finalize_msg */
                                    FINALIZEIF( !blob_unpack( blob, blob_bytes, &pItem, &pProcess_time ), g_finalize_msg );
                                    mxSetFieldByNumber( plhs[0], index, fieldnr, pItem );
                                }
                            } else {
                                // empty BLOB
                                mxArray* out_double = mxCreateDoubleMatrix( 0, 0, mxREAL );
                                mxSetFieldByNumber( plhs[0], index, fieldnr, out_double );
                            }
                        } else {
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
            FINALIZEIF( NULL == plhs[0], MSG_CANTCREATEOUTPUT );
            FINALIZEIF( SQLITE_DONE != res, SQL_ERR );
        }
    }
        
finalize:        
    if( st )
    {
        sqlite3_clear_bindings( st );
        sqlite3_finalize( st );
    }
    
    Free( command );  
    DestroyArray( pArgs );

    // mexErrMsg*() functions automatically free all 
    // allocated memory by mxCalloc() ans mxCreate*() functions.

    if( g_finalize_msg == SQL_ERR || g_finalize_msg == SQL_ERR_CLOSE )
    {
        const char *msg_id = TransErrToIdent( g_dbs[db_id] );
        char msg_err[1024];
        
        _snprintf( msg_err, 1024, "SQLite: \"%s\"", sqlite3_errmsg( g_dbs[db_id] ) );
        
        if( g_finalize_msg == SQL_ERR_CLOSE )
        {
            if( SQLITE_OK == sqlite3_close( g_dbs[db_id] ) )
            {
                g_dbs[db_id] = 0;
            } else {
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
void powFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
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
void regexFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
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
                Free( str_value );
                str_value = temp;
            }
        }
        
        if( str_value )
        {
            sqlite3_result_text( ctx, str_value, -1, SQLITE_TRANSIENT );
            Free( str_value );
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
void MD5Func( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef typed_BLOB_header_v1 tbhv1_t;
    typedef typed_BLOB_header_v2 tbhv2_t;
    
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
          if( !tbh1->valid_magic() )
          {
              MD5_Init( &md5_ctx );
              MD5_Update( &md5_ctx, (void*)tbh1, bytes );
              MD5_Final( digest, &md5_ctx );
              break;
          }
          
          /* uncompressed typed header? */
          if( tbh1->valid_ver() )
          {
              MD5_Init( &md5_ctx );
              MD5_Update( &md5_ctx, tbh1->data(), (int)(bytes - tbh1->data_offset()) );
              MD5_Final( digest, &md5_ctx );
              break;
          }
          
          /* compressed typed header? Decompress first */
          if( tbh2->valid_ver() && tbh2->valid_compression() )
          {
              mxArray* pItem = NULL;
              double process_time;
              
              if( blob_unpack( tbh2, (int)bytes, &pItem, &process_time ) && pItem )
              {
                  size_t data_bytes = typed_BLOB_header_base::data_bytes( pItem );
                  
                  MD5_Init( &md5_ctx );
                  MD5_Update( &md5_ctx, mxGetData( pItem ), (int)data_bytes );
                  MD5_Final( digest, &md5_ctx );
                  
                  DestroyArray( pItem );
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
        Free( str_result );
    }
}

// BDCRatio function. Calculates the compression ratio for a blob
void BDCRatioFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef typed_BLOB_header_v2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv2_t* tbh2 = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        double ratio = 1.0;
        
        if( tbh2 && tbh2->valid_magic() && tbh2->valid_ver() && tbh2->valid_compression() )
        {
            size_t blosc_nbytes, blosc_cbytes, blosc_blocksize; 
            blosc_cbytes = sqlite3_value_bytes( argv[0] ) - tbh2->data_offset();
            
            blosc_set_compressor( tbh2->m_compression );
            blosc_cbuffer_sizes( tbh2->data(), &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );
            if( blosc_nbytes > 0 )
            {
                ratio = (double)blosc_cbytes / (double)blosc_nbytes;
            }
        }

        sqlite3_result_double( ctx, ratio );
        
    } else {
        sqlite3_result_error( ctx, "BDCRatio(): only BLOB type supported!", -1 );
    }
}

// BDCPackTime function. Calculates the compression time on a blob
void BDCPackTimeFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef typed_BLOB_header_v2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv2_t* tbh2 = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        double process_time = 0.0;
        
        if( tbh2 && tbh2->valid_magic() && tbh2->valid_ver() && tbh2->valid_compression() )
        {
            size_t blosc_nbytes, blosc_cbytes, blosc_blocksize;
            blosc_cbytes = sqlite3_value_bytes( argv[0] ) - tbh2->data_offset();

            blosc_set_compressor( tbh2->m_compression );
            blosc_cbuffer_sizes( tbh2->data(), &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );
            if( blosc_nbytes > 0 )
            {
                mxArray *pItem = NULL;;
                size_t blob_bytes = 0;
                void* dummy_blob = NULL;
                
                blob_bytes = (size_t)sqlite3_value_bytes( argv[0] );
                
                blob_unpack( (void*)tbh2, blob_bytes, &pItem, &process_time );
                blob_pack( pItem, &dummy_blob, &blob_bytes, &process_time );
                    
                sqlite3_free( dummy_blob );
                DestroyArray( pItem );
            }
        }

        sqlite3_result_double( ctx, process_time );
        
    } else {
        sqlite3_result_error( ctx, "BDCPackTime(): only BLOB type supported!", -1 );
    }
}

// BDCUnpackTime function. Calculates the decompression time on a blob
void BDCUnpackTimeFunc( sqlite3_context *ctx, int argc, sqlite3_value **argv ){
    assert( argc == 1 );
    
    typedef typed_BLOB_header_v2 tbhv2_t;
    
    sqlite3_result_null( ctx );

    if( SQLITE_BLOB == sqlite3_value_type( argv[0] ) )
    {
        tbhv2_t* tbh2 = (tbhv2_t*)sqlite3_value_blob( argv[0] );
        double process_time = 0.0;
        
        if( tbh2 && tbh2->valid_magic() && tbh2->valid_ver() && tbh2->valid_compression() )
        {
            size_t blosc_nbytes, blosc_cbytes, blosc_blocksize;
            blosc_cbytes = sqlite3_value_bytes( argv[0] ) - tbh2->data_offset();

            blosc_set_compressor( tbh2->m_compression );
            blosc_cbuffer_sizes( tbh2->data(), &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );
            if( blosc_nbytes > 0 )
            {
                mxArray *pItem = NULL;;
                size_t blob_bytes = 0;
                    
                blob_bytes = (size_t)sqlite3_value_bytes( argv[0] );
                    
                blob_unpack( (void*)tbh2, blob_bytes, &pItem, &process_time );
                    
                DestroyArray( pItem );
            }
        }

        sqlite3_result_double( ctx, process_time );
        
    } else {
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
    } else {
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


// create a compressed blob from a matlab variable
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_bytes, double *pProcess_time )
{
    /* 
     * create a typed blob. Header information is generated
     * according to value and type of the matrix and the machine
     */
    size_t        szElement   = mxGetElementSize( pItem );        // size of one element in bytes
    size_t        cntElements = mxGetNumberOfElements( pItem );   // number of elements
    mwSize        nDims       = mxGetNumberOfDimensions( pItem ); // number of data dimensions
    const void*   odata       = mxGetData( pItem );               // raw data 
    size_t        odata_bytes = cntElements * szElement;          // size of raw data in bytes
    void*         cdata       = NULL;                             // compressed data
    size_t        cdata_bytes = 0;                                // size of compressed data in bytes    
    
    assert( NULL != ppBlob && NULL != pBlob_bytes && NULL != pProcess_time );
    
    *ppBlob = NULL;
    *pProcess_time = 0.0;

    FINALIZEIF( !typed_BLOB_header_base::valid_clsid( pItem ), MSG_UNSUPPVARTYPE );

    if( g_compression_level )
    {
        /* Firstly compress raw data, then copy into blob structure */
        typed_BLOB_header_v2* tbh2 = NULL;
        cdata_bytes = odata_bytes + BLOSC_MAX_OVERHEAD;  
        cdata = mxMalloc( cdata_bytes );
        int blosc_clevel = g_compression_level;
        int blosc_cdata_bytes;

        FINALIZEIF( NULL == cdata, MSG_ERRMEMORY );   
        
        blosc_set_compressor( g_compression_type );
        double start_time = get_wall_time();
        
        blosc_cdata_bytes = blosc_compress( 
          /*clevel*/     blosc_clevel, 
          /*doshuffle*/  BLOSC_DOSHUFFLE, 
          /*typesize*/   szElement, 
          /*nbytes*/     odata_bytes, 
          /*src*/        odata, 
          /*dest*/       cdata, 
          /*destsize*/   cdata_bytes );
        
        *pProcess_time = get_wall_time() - start_time;
        
        if( blosc_cdata_bytes > 0 )
        {
            size_t blob_bytes_uncompressed;
            
            *pBlob_bytes = 
                typed_BLOB_header_v2::data_offset( nDims ) +
                blosc_cdata_bytes;
            
            blob_bytes_uncompressed =
                typed_BLOB_header_v1::data_offset( nDims ) +
                odata_bytes;
            
            if( *pBlob_bytes >= blob_bytes_uncompressed )
            {
                blosc_cdata_bytes = -1; // Switch zu uncompressed blob, it's not worth the efford
            }
        }

        if( blosc_cdata_bytes <= 0 )
        {
            // compressed data needs more space than original data
            // data will be stored uncompressed...
            Free( cdata );  // Needless due to mexErrMsgTxt(), but clean
        } else {

            if( *pBlob_bytes > MKSQLITE_MAX_BLOB_SIZE )
            {
                Free( cdata );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_BLOBTOOBIG );
            }

            tbh2 = (typed_BLOB_header_v2*)sqlite3_malloc( (int)*pBlob_bytes );
            if( NULL == tbh2 )
            {
                Free( cdata );  // Needless due to mexErrMsgTxt(), but clean
                FINALIZE( MSG_ERRMEMORY );
            }

            tbh2->init( pItem );

            // TODO: Do byteswapping here if big endian? 
            // (Most platforms use little endian)
            memcpy( (char*)tbh2->data(), cdata, blosc_cdata_bytes );
            Free( cdata );

            if( g_compression_check )
            {
                int check_id = 0xdeadbeef;
                size_t dec_data_bytes = odata_bytes;
                void* dec_data = mxMalloc( dec_data_bytes + 2 * sizeof( check_id ) );
                char* pData = (char*)dec_data;
                int ok = 0;

                if( NULL == dec_data )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( MSG_ERRMEMORY );
                }

                memcpy( pData, &check_id, sizeof( check_id ) );
                pData += sizeof( check_id );
                memcpy( pData + dec_data_bytes, &check_id, sizeof( check_id ) );
                
                blosc_set_compressor( tbh2->m_compression );
                if( blosc_decompress( 
                    /*src*/      tbh2->data(), 
                    /*dest*/     pData, 
                    /*destsize*/ dec_data_bytes ) <= 0 )
                {
                    sqlite3_free( tbh2 );

                    Free( dec_data );  // Needless due to mexErrMsgTxt(), but clean
                    FINALIZE( MSG_ERRCOMPRESSION );
                }

                ok = (    dec_data_bytes == odata_bytes 
                       && 0 == memcmp( odata, pData, dec_data_bytes ) 
                       && 0 == memcmp( dec_data,               &check_id, sizeof( check_id ) )
                       && 0 == memcmp( pData + dec_data_bytes, &check_id, sizeof( check_id ) ) );

                Free( dec_data );

                if( !ok )
                {
                    sqlite3_free( tbh2 );
                    FINALIZE( MSG_ERRCOMPRESSION );
                }
            }

            *ppBlob = (void*)tbh2;

        }
    }

    if( !*ppBlob )
    {
        typed_BLOB_header_v1* tbh1 = NULL;

        /* Without compression, raw data is copied into blob structure as is */
        *pBlob_bytes = typed_BLOB_header_v1::data_offset( nDims ) +
                       odata_bytes;

        FINALIZEIF( *pBlob_bytes > MKSQLITE_MAX_BLOB_SIZE, MSG_BLOBTOOBIG );

        tbh1 = (typed_BLOB_header_v1*)sqlite3_malloc( (int)*pBlob_bytes );
        if( NULL == tbh1 )
        {
            FINALIZE( MSG_ERRMEMORY );
        }

        tbh1->init( pItem );

        // TODO: Do byteswapping here if big endian? 
        // (Most platforms use little endian)
        memcpy( tbh1->data(), odata, odata_bytes );

        *ppBlob = (void*)tbh1;
    }
    
    return 1;
    
finalize:
  
    return 0;
}

// uncompress a blob and return its matlab variable
int blob_unpack( const void* pBlob, size_t blob_bytes, mxArray** ppItem, double* pProcess_time )
{
    typedef typed_BLOB_header_v1 tbhv1_t;
    typedef typed_BLOB_header_v2 tbhv2_t;
    
    assert( NULL != ppItem && NULL != pProcess_time );
    *ppItem = NULL;
    *pProcess_time = 0.0;

    tbhv1_t*    tbh1          = (tbhv1_t*)pBlob;
    tbhv2_t*    tbh2          = (tbhv2_t*)pBlob;

    mxArray*    pItem         = NULL;

    /* test valid platform */
    if( !tbh1->valid_platform() )
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
    if( !tbh1->valid_magic() )
    {
        FINALIZE( MSG_UNSUPPTBH );
    }

    FINALIZEIF( !tbh1->valid_clsid(), MSG_UNSUPPVARTYPE );

    switch( tbh1->m_ver )
    {
      case sizeof( tbhv1_t ):
      {
          pItem       = tbh1->CreateNumericArray( /* doCopyData */ true );
          break;
      }

      case sizeof( tbhv2_t ):
      {
          pItem                     = tbh2->CreateNumericArray( /* doCopyData */ false );
          void*  packed_data        = tbh2->data();
          size_t packed_data_bytes  = blob_bytes - tbh2->data_offset();
          
          if( pItem )
          {
              size_t blosc_nbytes, blosc_cbytes, blosc_blocksize; 
              blosc_cbytes = packed_data_bytes;

              if( !tbh2->valid_compression() )
              {
                  DestroyArray( pItem );  // Needless due to mexErrMsgTxt(), but clean
                  FINALIZE( MSG_UNKCOMPRESSOR );
              }

              blosc_set_compressor( tbh2->m_compression );
              blosc_cbuffer_sizes( packed_data, &blosc_nbytes, &blosc_cbytes, &blosc_blocksize );

              if( blosc_nbytes != typed_BLOB_header_base::data_bytes( pItem ) )
              {
                  DestroyArray( pItem );  // Needless due to mexErrMsgTxt(), but clean
                  FINALIZE( MSG_ERRCOMPRESSION );
              }

              double start_time = get_wall_time();

              if( blosc_decompress( packed_data, (void*)mxGetData( pItem ), blosc_nbytes ) <= 0 )
              {
                  DestroyArray( pItem );  // Needless due to mexErrMsgTxt(), but clean
                  FINALIZE( MSG_ERRCOMPRESSION );
              } else {
                  *pProcess_time = get_wall_time() - start_time;
                  // TODO: Do byteswapping here if needed, depend on endian?
              }
          }
          break;
      }

      default:
          FINALIZE( MSG_UNSUPPTBH );
    }

    FINALIZEIF( NULL == pItem, MSG_ERRMEMORY );

    *ppItem = pItem;
    return 1;
      
finalize:
  
    return 0;
}



/*
 *
 * Formatierungsanweisungen für den Editor vim
 *
 * vim:ts=4:ai:sw=4
 */
