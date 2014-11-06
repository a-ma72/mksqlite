/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      global.hpp
 *  @brief     Global definitions.
 *  @details   
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
        
/* MATLAB IEEE representation functions */
#define DBL_ISFINITE mxIsFinite
#define DBL_ISINF    mxIsInf
#define DBL_ISNAN    mxIsNaN
#define DBL_INF      mxGetInf()
#define DBL_NAN      mxGetNaN()
        
/* Versionstrings */
#define SQLITE_VERSION_STRING     SQLITE_VERSION
#define DEELX_VERSION_STRING      "1.2"
#define MKSQLITE_VERSION_STRING   "2.0 candidate"

/* early bind of serializing functions (earlier MATLAB versions only) */
#if (CONFIG_EARLY_BIND_SERIALIZE)
extern "C" mxArray* mxSerialize(const mxArray*);
extern "C" mxArray* mxDeserialize(const void*, size_t);
#endif

typedef unsigned char byte;

#undef MEM_ALLOC
#undef MEM_FREE 

#if 0
    // mxCalloc() and mxFree() are extremely slow!
    #define MEM_ALLOC( count, bytes )   ( (void*)mxMalloc( count, bytes ) )
    #define MEM_FREE( ptr )             mxFree( ptr )
    #define MEM_REALLOC( ptr, bytes )   mxRealloc( ptr, bytes )
#else
    /// Global memory allocator
    #define MEM_ALLOC( count, bytes )   ( (void*)new char[count*bytes] )
    /// Global memory deallocator
    #define MEM_FREE( ptr )             ( delete[] ptr )
    /// Global memory deallocator
    #define MEM_REALLOC( ptr, size )    HC_ASSERT_ERROR
#endif

#if CONFIG_USE_HEAP_CHECK
// memory macros (MEM_ALLOC, MEM_REALLOC and MEM_FREE) used by heap_check.hpp
#include "heap_check.hpp"
        
// Now redirect memory macros to heap checking functions
#undef MEM_ALLOC
#undef MEM_FREE 
        
#define MEM_ALLOC( count, bytes )   (HeapCheck.New( count * bytes, __FUNCTION__, /*notes*/ "", __LINE__ ))
#define MEM_FREE( ptr )             (HeapCheck.Free( ptr ))
#endif // CONFIG_USE_HEAP_CHECK

    
#ifdef MAIN_MODULE


/* common global states */

/// Flag: Show the welcome message, initializing...
int             g_is_initialized        = 0;

/// Max. length for fieldnames in MATLAB 
int             g_namelengthmax         = 63;

/// Flag: return NULL as NaN
int             g_NULLasNaN             = CONFIG_NULL_AS_NAN;
const double    g_NaN                   = mxGetNaN();

/// Flag: Check for unique fieldnames
int             g_check4uniquefields    = CONFIG_CHECK_4_UNIQUE_FIELDS;

/// Compression settings for typed BLOBs
int             g_compression_level     = CONFIG_COMPRESSION_LEVEL;    
const char*     g_compression_type      = CONFIG_COMPRESSION_TYPE; 
int             g_compression_check     = CONFIG_COMPRESSION_CHECK;

/// Flag: String representation (utf8 or ansi)
int             g_convertUTF8           = CONFIG_CONVERT_UTF8;

/// Flag: Allow streaming
int             g_streaming             = CONFIG_STREAMING;

/// Data organisation of returning query results
int             g_result_type           = CONFIG_RESULT_TYPE;

/// Wrap parameters
int             g_wrap_parameters       = CONFIG_WRAP_PARAMETERS;

#endif
