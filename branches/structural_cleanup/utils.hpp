/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file
 *  @brief     Utilities used in all files.
 *  @details   Value class and string support.
 *  @author    Martin Kortmann <mail@kortmann.de>
 *  @author    Andreas Martin
 *  @version   2.0
 *  @date      2008-2014
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

// http://note.sonots.com/Mex/Matrix.html

#pragma once

#include "config.h"
#include "global.hpp"
#include "locale.hpp"

/* helper functions, declaration */

                  size_t  utils_elbytes( mxClassID classID );
                  int     utils_utf2latin         ( const unsigned char *s, unsigned char *buffer );
                  int     utils_latin2utf         ( const unsigned char *s, unsigned char *buffer );
                  char*   utils_strnewdup         ( const char* s );
                  void    utils_destroy_array     ( mxArray *&pmxarr );
template<class T> void    utils_free_ptr          ( T *&pmxarr );



#ifdef MAIN_MODULE

/* Implementations */

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

/**
 * @brief Convert UTF-8 string to char string
 *
 * @param [in]  s input string UTF8 encoded
 * @param [out] buffer optional pointer to where the string should be written (NULL allowed)
 * @returns always the count of bytes written (or needed) to convert input string
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
 * @returns always the count of bytes written (or needed) to convert input string
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
 * @brief duplicate a string and recode from UTF8 to char due to global flag @ref g_convertUTF8
 *
 * @param [in] s input string
 * @returns pointer to created duplicate (allocator @ref MEM_ALLOC)
 */
char* utils_strnewdup(const char* s)
{
    char *newstr = 0;
    
    if( g_convertUTF8 )
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
 * Matlab documentation is missing the issue, if mxFree and mxDestroyArray
 * accept NULL pointers. Tested without crash, but what will be in further 
 * Matlab versions...
 * So we do it on our own:
 */

/*
 * utils_destroy_array() deallocates memory reserved by 
 * mxCreateNumericMatrix() or
 * mxCreateNumericArray()
 */
void utils_destroy_array( mxArray *&pmxarr )
{
    if( pmxarr )
    {
        mxDestroyArray( pmxarr );
        pmxarr = NULL;
    }
}


/*
 * utils_free_ptr() can handle casted pointers allocated from 
 * MEM_ALLOC
 */
template <class T>
void utils_free_ptr( T *&pmxarr )
{
    if( pmxarr )
    {
        MEM_FREE( pmxarr );
        pmxarr = NULL;
    }
}



// Time measuring functions
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

double get_cpu_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}
#endif




#endif