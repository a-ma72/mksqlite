/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      global.hpp
 *  @brief     Global definitions.
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

#ifndef _GLOBAL_H
#define _GLOBAL_H   /**< include guard (redundant) */
#endif

/* Common global definitions */

#if defined( MATLAB_MEX_FILE ) /* MATLAB MEX file */
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
  #include "mex.h"
#endif
      
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define STRICT
  #define NOMINMAX
  #include <windows.h>
  #undef min
  #undef max
#else  // linux
  #include <string.h>
  #include <ctype.h>
  #define _strcmpi    strcasecmp
  #define _strnicmp   strncasecmp
  #define _snprintf   snprintf
  #define _vsnprintf  vsnprintf
  #define _strdup     strdup
  #define _copysign   copysign  ///< alias (win/linux compatibility)
#endif

#include "config.h"
#include <cstddef>
#include <cmath>
#include <cassert>
#include <climits>

// Patch for Mac:
// Tested on Mac OSX 10.9.2, Malab R2014a, 64 bit (Stefan Balke)
#if defined(__APPLE__) || defined(TARGET_OS_X)
    // todo: Which header is needed on mac?
    //#include <tr1/cstdint>
    #include <cstdint>
#else
    #include <cstdint>
#endif

/**
 * \def MEM_ALLOC
 * \brief standard memory allocator
 *
 * \def MEM_REALLOC
 * \brief standard memory deallocator
 *
 * \def MEM_FREE(ptr)
 * \brief standard memory free function
 * 
 * \def MAT_ARRAY_TYPE
 * \brief standard type for internally handled MATLAB array types (not used yet)
 * 
 * \def MAT_ALLOC(m,n,typeID)
 * \brief standard allocator for MAT_ARRAY_TYPE (not used yet)
 * 
 * \def MAT_FREE(ptr)
 * \brief standard deallocator for MAT_ARRAY_TYPE (not used yet)
 * 
 * Use ::utils_free_ptr() instead of MEM_FREE, when a NULL check
 * must be performed on \p ptr and &ptr must be set to NULL after freeing.
 *
 * \def WALKHEAP
 * When heap checking is on (\ref CONFIG_USE_HEAP_CHECK), reports 
 * the monitored heap allocation state (MATLAB command window).
 *
 * \def FREEHEAP
 * When heap checking is on (\ref CONFIG_USE_HEAP_CHECK), frees 
 * heap space where allocation was monitored.
 * 
 */

// define standard memory handlers used by mksqlite
#undef  MEM_ALLOC
#undef  MEM_REALLOC
#undef  MEM_FREE

#if 0
    // mxCalloc() and mxFree() are extremely slow!
    #define MEM_ALLOC( count, bytes )   ((void*)mxMalloc( (count) * (bytes) ))
    #define MEM_FREE( ptr )             mxFree( (void*)ptr )
    #define MEM_REALLOC( ptr, bytes )   ((void*)mxRealloc( (void*)ptr, bytes ))
#else
    // Global memory allocator
    #define MEM_ALLOC( count, bytes )   ( (void*)new char[(count) * (bytes)] )
    // Global memory deallocator
    #define MEM_FREE( ptr )             ( delete[] ptr )
    // Global memory deallocator
    #define MEM_REALLOC( ptr, size )    HC_ASSERT_ERROR
#endif

// definition how matrix arrays are managed
#if 1 && defined( MATLAB_MEX_FILE )
    #define MAT_ARRAY_TYPE              mxArray
    #define MAT_ALLOC( m, n, typeID )   mxCreateNumericMatrix( m, n, typeID, mxREAL )
    #define MAT_FREE( ptr )             mxDestroyArray( ptr )
#else
    // \todo not supported yet
    struct tagNumericArray;
    #define MAT_ARRAY_TYPE              tagNumericArray
    #define MAT_ALLOC( m, n, typeID )   tagNumericArray::Create( m, n, typeID )
    #define MAT_FREE( ptr )             tagNumericArray::FreeArray( ptr )
#endif

/** 
 * \def CONFIG_USE_HEAP_CHECK
 * When defined, MEM_ALLOC, MEM_REALLOC and MEM_FREE actions were monitored
 * and some simple boundary checks will be made.
 */ 
#if CONFIG_USE_HEAP_CHECK
    // redefine memory (de-)allocators, if heap checking is on
    // memory macros MEM_ALLOC, MEM_REALLOC and MEM_FREE were used by heap_check.hpp
    #include "heap_check.hpp"
        
    // Now redirect memory macros to heap checking functions
    #undef  MEM_ALLOC
    #undef  MEM_REALLOC
    #undef  MEM_FREE
   
    #define MEM_ALLOC(n,s)      (HeapCheck.New( (n) * (s), __FILE__, __FUNCTION__, /*notes*/ "", __LINE__ ))
    #define MEM_REALLOC(p,s)    (HeapCheck.Realloc( (void*)(p), (s), __FILE__, __FUNCTION__, /*notes*/ "", __LINE__ ))
    #define MEM_FREE(p)         (HeapCheck.Free( (void*)(p) ))
    #define WALKHEAP            (HeapCheck.Walk())
    #define FREEHEAP            (HeapCheck.Release())
#else
    #define HC_COMP_ASSERT(exp)
    #define HC_ASSERT(exp)
    #define HC_ASSERT_ERROR
    #define HC_NOTES(ptr,notes)
    #define WALKHEAP
    #define FREEHEAP            
#endif // CONFIG_USE_HEAP_CHECK
        
    
/**
 * \name IEEE representation functions
 *
 * @{
 */
#if defined( MATLAB_MEX_FILE ) 
    #define DBL_ISFINITE        mxIsFinite
    #define DBL_ISINF           mxIsInf
    #define DBL_ISNAN           mxIsNaN
    #define DBL_INF             mxGetInf()
    #define DBL_NAN             mxGetNaN()
    #define DBL_EPS             mxGetEps()
#else
  #if defined( _WIN32 )
    // MSVC2010
    #define DBL_INF             (_Inf._Double) /* <ymath.h> -?-/double */
    #define DBL_NAN             (_Nan._Double) /* <ymath.h> -?-/double */
//  #define DBL_EPS             DBL_EPS
    #define DBL_ISFINITE(x)     _finite(x)     /* <float.h> -?-/double */
    #define DBL_ISNAN(x)        _isnan(x)      /* <float.h> -?-/double */
    #define DBL_ISINF(x)        (!DBL_ISFINITE(x) && !DBL_ISNAN(x))
  #else
    // gcc
    #define DBL_INF             INFINITY       /* <cmath> implementation defined */
    #define DBL_NAN             NAN            /* <cmath> implementation defined */
//  #define DBL_EPS             DBL_EPS
    #if 0
      #define DBL_ISFINITE(x)     (((x)-(x)) == 0.0)
      #define DBL_ISNAN(x)        ((x)!=(x))
      #define DBL_ISINF(x)        (!DBL_ISFINITE(x) && !DBL_ISNAN(x))
    #else
      #define DBL_ISFINITE(x)     isfinite(x)  /* <cmath> float/double */
      #define DBL_ISNAN(x)        isnan(x)     /* <cmath> float/double */
      #define DBL_ISINF(x)        isinf(x)     /* <cmath> float/double */
    #endif
  #endif
#endif
/** @} */
    
        
/**
 * \name Versionstrings
 *
 * mksqlite version string is defined in config.h
 * (\ref CONFIG_MKSQLITE_VERSION_STRING)
 *
 * @{
 */
#define SQLITE_VERSION_STRING     SQLITE_VERSION
#define DEELX_VERSION_STRING      "1.3"
/** @} */

/* early bind of serializing functions (earlier MATLAB versions only) */
#if defined( MATLAB_MEX_FILE )
  #if defined( CONFIG_EARLY_BIND_SERIALIZE )
    extern "C" mxArray* mxSerialize(const mxArray*);          ///< Serialize a MATLAB array
    extern "C" mxArray* mxDeserialize(const void*, size_t);   ///< Deserialize a MATLAB array
  #endif
  extern mxArray *mxCreateSharedDataCopy(const mxArray *pr);  ///< Create a "shadowed" MATLAB array
  #define PRINTF mexPrintf                                    ///< Global text output function
#endif

typedef unsigned char byte;  ///< byte type


    
#if defined( MAIN_MODULE )


/* common global states */

/**
 * \name Compression settings for typed BLOBs
 *
 * @{
 */
int             g_compression_level     = CONFIG_COMPRESSION_LEVEL;    
const char*     g_compression_type      = CONFIG_COMPRESSION_TYPE; 
int             g_compression_check     = CONFIG_COMPRESSION_CHECK;
/** @} */

/// Flag: String representation (utf8 or ansi)
int             g_convertUTF8           = CONFIG_CONVERT_UTF8;
/// global NaN representation
const double    g_NaN                   = DBL_NAN;

/// MATALAB specific globals
#if defined( MATLAB_MEX_FILE )
    /// Max. length for fieldnames in MATLAB 
    int             g_namelengthmax         = 63;  // (mxMAXNAM-1)

    /// Flag: return NULL as NaN
    int             g_NULLasNaN             = CONFIG_NULL_AS_NAN;

    /// Flag: Check for unique fieldnames
    int             g_check4uniquefields    = CONFIG_CHECK_4_UNIQUE_FIELDS;

    /// Flag: Allow streaming
    int             g_streaming             = CONFIG_STREAMING;

    /// Data organisation of returning query results
    int             g_result_type           = CONFIG_RESULT_TYPE;

    /// Wrap parameters
    int             g_param_wrapping        = CONFIG_PARAM_WRAPPING;

#endif  // defined( MATLAB_MEX_FILE )

#endif  // defined( MAIN_MODULE )

