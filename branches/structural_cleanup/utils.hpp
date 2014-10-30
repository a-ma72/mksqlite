/*
 * mksqlite: A MATLAB Interface to SQLite
 *
 * (c) 2008-2014 by M. Kortmann <mail@kortmann.de>
 *               and A.Martin
 * distributed under LGPL
 */

#pragma once

#include "config.h"
#include "global.hpp"
#include "locale.hpp"

/* helper functions, declaration */

                  int     utils_utf2latin         ( const unsigned char *s, unsigned char *buffer );
                  int     utils_latin2utf         ( const unsigned char *s, unsigned char *buffer );
                  char*   utils_strnewdup         ( const char* s );
                  void    utils_destroy_array     ( mxArray *&pmxarr );
template<class T> void    utils_free_ptr          ( T *&pmxarr );



/*
 * class Value never takes custody of a MATLAB memory object!
 */
class Value
{
private:
    
    const mxArray* m_pcItem;   // MATLAB variable (item)
    
public:
    /* Complexity information about a MATLAB variable.
     * For testing if a variable is able to be packed or not.
     */
    typedef enum {
        TC_EMPTY = 0,       // Empty
        TC_SIMPLE,          // single non-complex value, char or simple string (SQLite simple types)
        TC_SIMPLE_VECTOR,   // non-complex numeric vectors (SQLite BLOB)
        TC_SIMPLE_ARRAY,    // multidimensional non-complex numeric or char arrays (SQLite typed BLOB)
        TC_COMPLEX,         // structs, cells, complex data (SQLite typed ByteStream BLOB)
        TC_UNSUPP = -1      // all other (unsuppored types)
    } type_complexity_e;
    
    
    explicit 
    Value( const mxArray* pcItem )
    {
        m_pcItem = pcItem;
    }
    
    
    inline
    const mxArray* Item() const
    {
        return m_pcItem;
    }
    
    
    /* returns true when value is NULL or empty ([]) */
    inline
    bool IsEmpty() const
    {
        return m_pcItem ? mxIsEmpty( m_pcItem ) : true;
    }

    /* returns true when value is a cell array */
    inline
    bool IsCell() const
    {
        return m_pcItem ? mxIsCell( m_pcItem ) : false;
    }

    /* returns true when value is neither NULL nor complex */
    inline
    bool IsComplex() const
    {
        return m_pcItem ? mxIsComplex( m_pcItem ) : false;
    }

    /* returns true when value consists of exact 1 element */
    inline
    bool IsScalar() const
    {
        return NumElements() == 1;
    }

    /* returns true when value is of type 1xN or Mx1 */
    inline
    bool IsVector() const
    {
        return NumDims() == 2 && min( mxGetM( m_pcItem ), mxGetN( m_pcItem ) ) == 1;
    }
    
    inline
    bool IsDoubleClass() const
    {
        return mxDOUBLE_CLASS == ClassID();
    }

    /* returns the number of elements in value */
    inline
    size_t NumElements() const
    {
        return m_pcItem ? mxGetNumberOfElements( m_pcItem ) : 0;
    }
    
    /* returns the size in bytes of one element */
    inline
    size_t ByElement() const
    {
        return m_pcItem ? mxGetElementSize( m_pcItem ) : 0;
    }
    
    /* returns the number of dimensions for value */
    inline
    int NumDims() const
    {
        return m_pcItem ? mxGetNumberOfDimensions( m_pcItem ) : 0;
    }
      
    /* returns the data size of a MATLAB variable in bytes */
    inline
    size_t ByData() const
    {
       return NumElements() * ByElement();
    }
    
    /* returns the MATLAB class ID for value */
    inline
    mxClassID ClassID() const
    {
        return m_pcItem ? mxGetClassID( m_pcItem ) : mxUNKNOWN_CLASS;
    }

    /* returns the complexity of the value (for storage issues) */
    type_complexity_e Complexity( bool bCanSerialize = false ) const
    {
        if( IsEmpty() ) return TC_EMPTY;

        switch( ClassID() )
        {
            case  mxDOUBLE_CLASS:
            case  mxSINGLE_CLASS:
                if( mxIsComplex( m_pcItem ) )
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
                if( IsScalar() ) return TC_SIMPLE;
                return IsVector() ? TC_SIMPLE_VECTOR : TC_SIMPLE_ARRAY;
            case    mxCHAR_CLASS:
                return ( IsScalar() || IsVector() ) ? TC_SIMPLE : TC_SIMPLE_ARRAY;
            case mxUNKNOWN_CLASS:
                // serialized data is marked as "unknown" type by mksqlite
                return bCanSerialize ? TC_COMPLEX : TC_UNSUPP;
            case  mxSTRUCT_CLASS:
            case    mxCELL_CLASS:
                return TC_COMPLEX;
            default:
                return TC_UNSUPP;
        }
    }
    
    
    inline
    void* Data() const
    {
        return mxGetData( m_pcItem );
    }
    
    /*
     * Convert a string to char, due flagUTF converted to utf8
     */
    char *GetString( bool flagUTF = false, const char* format = NULL ) const
    {
        size_t      count;
        char*       result          = NULL;
        mxArray*    new_string      = NULL;
        mxArray*    org_string      = const_cast<mxArray*>(m_pcItem);
        
        if( format )
        {
            mxArray* args[2] = { mxCreateString( format ), org_string };

            mexCallMATLAB( 1, &new_string, 2, args, "sprintf" );
            mxDestroyArray( args[0] );  // destroy format string
            
            org_string = new_string;
        }
        
        if( org_string )
        {
            count   = mxGetM( org_string ) * mxGetN( org_string ) + 1;
            result  = (char*) MEM_ALLOC( count, sizeof(char) );
        }

        if( !result || mxGetString( org_string, result, (int)count ) )
        {
            utils_destroy_array(new_string );
            mexErrMsgTxt( getLocaleMsg( MSG_CANTCOPYSTRING ) );
        }
        
        utils_destroy_array(new_string );

        if( flagUTF )
        {
            char *buffer = NULL;
            int buflen;

            /* get only the buffer size needed */
            buflen = utils_latin2utf( (unsigned char*)result, (unsigned char*)buffer );
            buffer = (char*) MEM_ALLOC( buflen, sizeof(char) );

            if( !buffer )
            {
                utils_free_ptr( result ); // Needless due to mexErrMsgTxt(), but clean
                mexErrMsgTxt( getLocaleMsg( MSG_CANTCOPYSTRING ) );
            }

            /* encode string to utf now */
            utils_latin2utf( (unsigned char*)result, (unsigned char*)buffer );

            utils_free_ptr( result );

            result = buffer;
        }

        return result;
    }
    
    
    /*
     * Convert a string to char, due to global flag converted to utf
     */
    char* GetEncString() const
    {
        return GetString( g_convertUTF8 ? true : false );
    }


    /*
     * get the integer value from value
     */
    int GetInt( int errval = 0 ) const
    {
        switch( mxGetClassID( m_pcItem ) )
        {
            case mxINT8_CLASS  : return (int) *( (int8_t*)   mxGetData( m_pcItem ) );
            case mxUINT8_CLASS : return (int) *( (uint8_t*)  mxGetData( m_pcItem ) );
            case mxINT16_CLASS : return (int) *( (int16_t*)  mxGetData( m_pcItem ) );
            case mxUINT16_CLASS: return (int) *( (uint16_t*) mxGetData( m_pcItem ) );
            case mxINT32_CLASS : return (int) *( (int32_t*)  mxGetData( m_pcItem ) );
            case mxUINT32_CLASS: return (int) *( (uint32_t*) mxGetData( m_pcItem ) );
            case mxSINGLE_CLASS: return (int) *( (float*)    mxGetData( m_pcItem ) );
            case mxDOUBLE_CLASS: return (int) *( (double*)   mxGetData( m_pcItem ) );
        }

        return errval;
    }
    
    
    double GetScalar() const
    {
        return IsScalar() ? mxGetScalar( m_pcItem ) : DBL_NAN;
    }
};



#ifdef MAIN_MODULE

/* Implementations */

/*
 * Convert UTF-8 strings to char strings and vice versa
 */
int utils_utf2latin( const unsigned char *s, unsigned char *buffer )
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


int utils_latin2utf( const unsigned char *s, unsigned char *buffer )
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



/*
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
        MEM_FREE( (void*)pmxarr );
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