/**
 *  <!-- mksqlite: A MATLAB interface to SQLite -->
 * 
 *  @file      utils.hpp
 *  @brief     Utilities used in all files.
 *  @details   Common utilities.
 *             (freeing mex memory, utf<->latin conversion, time measurement)
 *  @see       http://note.sonots.com/Mex/Matrix.html
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
#include "global.hpp"
//#include "locale.hpp"

/* helper functions, formard declarations */
#if defined( MATLAB_MEX_FILE)
                  char*   utils_getString         ( const mxArray* str );
                  size_t  utils_elbytes           ( mxClassID classID );
                  void    utils_destroy_array     ( mxArray *&pmxarr );
template<class T> void    utils_free_ptr          ( T *&pmxarr );
#endif
                  int     utils_utf2latin         ( const unsigned char *s, unsigned char *buffer );
                  int     utils_latin2utf         ( const unsigned char *s, unsigned char *buffer );
                  char*   utils_strnewdup         ( const char* s, int flagConvertUTF8 );
                  double  utils_get_wall_time     ();
                  double  utils_get_cpu_time      ();
                  char*   utils_strlwr            ( char* );


#ifdef MAIN_MODULE

#if defined( MATLAB_MEX_FILE)
/* Implementations */

/**
 * @brief Copy string characters into allocated memory
 * @details Not to use with multibyte strings!
 * 
 * @param str MATLAB char array
 * @return Pointer to allocated string
 */
char* utils_getString( const mxArray* str )
{
    char* result = NULL;

    if( str && mxCHAR_CLASS == mxGetClassID( str ) )
    {
        size_t size = mxGetNumberOfElements( str ) + 1;
        result = (char*)MEM_ALLOC( size, 1 );
        if( result )
        {
            mxGetString( str, result, (int)size );
        }
    }

    return result;
}

/**
 * @brief Get the size of one element in bytes
 *
 * @param [in]  classID class ID
 * @returns size of one element in bytes
 */
size_t utils_elbytes( mxClassID classID )
{
    size_t result = 0;

    switch (classID)
    {

    case mxCHAR_CLASS:
        result = sizeof(mxChar);
        break;

    case mxDOUBLE_CLASS:
        result = sizeof(double);
        break;

    case mxSINGLE_CLASS:
        result = sizeof(real32_T);
        break;

    case mxINT8_CLASS:
        result = sizeof(int8_T);
        break;

    case mxUINT8_CLASS:
        result = sizeof(uint8_T);
        break;

    case mxINT16_CLASS:
        result = sizeof(int16_T);
        break;

    case mxUINT16_CLASS:
        result = sizeof(uint16_T);
        break;

    case mxINT32_CLASS:
        result = sizeof(int32_T);
        break;

    case mxUINT32_CLASS:
        result = sizeof(uint32_T);
        break;

    default:
        assert( false );
    }

    return result;
}
#endif

/**
 * @brief Convert UTF-8 string to char string
 *
 * @param [in]  s input string UTF8 encoded
 * @param [out] buffer optional pointer to where the string should be written (NULL allowed)
 * @returns always the count of bytes written (or needed) to convert input string (including NUL)
 */
int utils_utf2latin( const unsigned char *s, unsigned char *buffer = NULL )
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

/**
 * @brief Convert char string to UTF-8 string
 *
 * @param [in]  s input string 
 * @param [out] buffer optional pointer to where the string should be written (NULL allowed)
 * @returns always the count of bytes written (or needed) to convert input string (including NUL)
 */
int utils_latin2utf( const unsigned char *s, unsigned char *buffer = NULL )
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


/**
 * @brief duplicate a string and recode from UTF8 to char due to flag \p flagConvertUTF8
 *
 * @param [in] s input string
 * @param [in] flagConvertUTF8 String \p s expected UTF8 encoded, if flag is set
 * @returns pointer to created duplicate (allocator @ref MEM_ALLOC) and must be freed with @ref MEM_FREE
 */
char* utils_strnewdup(const char* s, int flagConvertUTF8 )
{
    char *newstr = 0;
    
    if( flagConvertUTF8 )
    {
        if( s )
        {
            /* get memory space needed */
            int buflen = utils_utf2latin( (unsigned char*)s, NULL );

            newstr = (char*)MEM_ALLOC( buflen, sizeof(char) );
            if( newstr ) 
            {
                utils_utf2latin( (unsigned char*)s, (unsigned char*)newstr );
            }
        }
    }
    else
    {
        if( s )
        {
            newstr = (char*)MEM_ALLOC( strlen(s) + 1, sizeof(char) );
            if( newstr )
            {
                strcpy( newstr, s );
            }
        }

    }
    
    return newstr;
}


/**
 * @brief      Change string to lowercase (inplace)
 *
 * @param      str   String
 *
 * @return     String
 */
char* utils_strlwr( char* str )
{
    char *p = str;

    while( p && ( *p = ::tolower(*p) ) )
    {
       p++;
    }

    return str;
}



/** 
 * @file
 * @note
 * <HR>
 * From the Matlab documentation: \n
 *
 * mxDestroyArray deallocates the memory occupied by the specified mxArray. 
 * This includes: \n
 * - Characteristics fields of the mxArray, such as size, (m and n), and type.
 * - Associated data arrays, such as pr and pi for complex arrays, and ir and jc for sparse arrays.
 * - Fields of structure arrays.
 * - Cells of cell arrays.
 *
 * @note
 * Do not call mxDestroyArray on an mxArray: \n
 * - you return in a left-side argument of a MEX-file.
 * - returned by the mxGetField or mxGetFieldByNumber functions.
 * - returned by the mxGetCell function.
 *
 * @note
 * <HR>
 * Poorly the documentation is missing the issue, whether a NULL pointer may
 * be passed or not. For sure a self implementation will be used.
 */



#if defined( MATLAB_MEX_FILE)
/** 
 * @brief Freeing memory allocated by mxCreateNumericMatrix() or mxCreateNumericArray().
 *
 * @param [in] pmxarr Memory pointer or NULL
 *
 * Memory pointer \p pmxarr is set to NULL after deallocation.
 */
void utils_destroy_array( mxArray *&pmxarr )
{
    if( pmxarr )
    {
        // Workaround to avoid MATLAB crash with persistent arrays ("Case 02098404", Lucas Lebert, MathWorks Technical Support Department 
        mxArray* tmp = pmxarr;
        pmxarr = NULL;
        mxDestroyArray( tmp );
    }
}
#endif

/**
 * @brief Freeing memory allocated by mxAlloc() or mxRealloc()
 *
 * @param [in] pmxarr Memory pointer or NULL
 *
 * Memory pointer \p pmxarr is set to NULL after deallocation.
 */
template <class T>
void utils_free_ptr( T *&pmxarr )
{
    if( pmxarr )
    {
        T* tmp = pmxarr;
        pmxarr = NULL;
        MEM_FREE( tmp );
    }
}



// Time measuring functions

/**
 * @fn utils_get_wall_time
 * @brief Returns current counter time in seconds 
 * @returns Time in seconds
 *
 * @fn utils_get_cpu_time
 * @brief Returns user mode time of current process in seconds 
 * @returns Time in seconds
 */

// Windows
#ifdef _WIN32
#include <windows.h>

double utils_get_wall_time()
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


double utils_get_cpu_time()
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
double utils_get_wall_time()
{
    struct timeval time;
    
    if( gettimeofday( &time, NULL ) )
    {
        //  Handle error
        return 0;
    }
    
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

double utils_get_cpu_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}
#endif


#endif  /* MAIN_MODULE */