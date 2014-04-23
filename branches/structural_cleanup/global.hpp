/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

#pragma once

/* Common global definitions */

#ifdef _WIN32
  /* solve the 'error C2371: 'char16_t' : redefinition; different basic types' problem */
  /* ref: http://www.mathworks.com/matlabcentral/newsreader/view_thread/281754 */
  /* ref: http://connect.microsoft.com/VisualStudio/feedback/details/498952/vs2010-iostream-is-incompatible-with-matlab-matrix-h */
  #ifdef _WIN32
    #include <yvals.h>
    #if (_MSC_VER >= 1600)
      #define __STDC_UTF_16__
    #endif
/* Following solution doesn't work with Matlab R2011b, MSVC2010, Win7 64bit
    #ifdef _CHAR16T
      #define CHAR16_T
    #endif
*/
  #endif
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include "mex.h"
  #define copysign _copysign
#else
  #include <cstring>
  #include <ctype.h>
  #define _strcmpi strcasecmp
  #define _snprintf snprintf
  #include "mex.h"
#endif

#include <cmath>
#include <cassert>
#include <climits>

// Patch for Mac:
// Tested on Mac OSX 10.9.2, Malab R2014a, 64 bit (Stefan Balke)
#if defined(__APPLE__) || defined(TARGET_OS_X)
    #include <tr1/cstdint>
#else
    #include <cstdint>
#endif
        
/* Versionstrings */
#define SQLITE_VERSION_STRING     SQLITE_VERSION
#define DEELX_VERSION_STRING      "1.2"
#define MKSQLITE_VERSION_STRING   "1.15candidate"


#if (CONFIG_EARLY_BIND_SERIALIZE)
extern "C" mxArray* mxSerialize(const mxArray*);
extern "C" mxArray* mxDeserialize(const void*, size_t);
#endif

// (un-)packing functions (definition in mksqlite.cpp)
int blob_pack( const mxArray* pItem, void** ppBlob, size_t* pBlob_size, double* pProcess_time, double* pdRatio );
int blob_unpack( const void* pBlob, size_t blob_size, mxArray** ppItem, double* pProcess_time, double* pdRatio );


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
#define SETERR( msg )             ( g_finalize_msg = msg )
#define FINALIZE( msg )           { SETERR(msg); goto finalize; }
#define FINALIZE_IF( cond, msg )  { if(cond) FINALIZE( msg ) }

/* common global states */
        
extern const double g_NaN;
extern bool         g_NULLasNaN;
extern bool         g_is_initialized;
extern bool         g_convertUTF8;
extern bool         g_check4uniquefields;
extern int          g_compression_level;
extern const char*  g_compression_type;
extern int          g_compression_check;
extern const char*  g_finalize_msg;


#ifdef MAIN_MODULE


// Used by SETERR:
// if assigned, function returns with an appropriate error message
const char*     g_finalize_msg          = NULL;              

/* Flag: Show the welcome message, initializing... */
bool            g_is_initialized        = false;

/* Flag: return NULL as NaN  */
bool            g_NULLasNaN             = CONFIG_NULL_AS_NAN;
const double    g_NaN                   = mxGetNaN();

/* Flag: Check for unique fieldnames */
bool            g_check4uniquefields    = CONFIG_CHECK_4_UNIQUE_FIELDS;

int             g_compression_level     = CONFIG_COMPRESSION_LEVEL;    
const char*     g_compression_type      = CONFIG_COMPRESSION_TYPE; 
int             g_compression_check     = CONFIG_COMPRESSION_CHECK;

bool            g_convertUTF8           = CONFIG_CONVERT_UTF8;

#endif