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

int     utf2latin         ( const unsigned char *s, unsigned char *buffer );
int     latin2utf         ( const unsigned char *s, unsigned char *buffer );
char*   strnewdup         ( const char* s );
void    destroy_array     ( mxArray *&pmxarr );
char*   get_string        ( const mxArray *a );
int     get_integer       ( const mxArray* a );
size_t  get_data_size     ( const mxArray* a );

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

template <class T>
void free_ptr( T *&pmxarr )
{
    if( pmxarr )
    {
        mxFree( (void*)pmxarr );
        pmxarr = NULL;
    }
}


#ifdef MAIN_MODULE

/* Implementations */

/*
 * Convert UTF-8 Strings to 8Bit and vice versa
 */
int utf2latin( const unsigned char *s, unsigned char *buffer )
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


int latin2utf( const unsigned char *s, unsigned char *buffer )
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
char* strnewdup(const char* s)
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


/*
 * Convert a string to char *
 */
char *get_string( const mxArray *a )
{
    size_t count = mxGetM( a ) * mxGetN( a ) + 1;
    char *c = (char *)mxCalloc( count, sizeof(char) );

    if( !c || mxGetString( a, c, (int)count ) )
    {
        mexErrMsgTxt( getMsg( MSG_CANTCOPYSTRING ) );
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
            mexErrMsgTxt( getMsg( MSG_CANTCOPYSTRING ) );
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
int get_integer( const mxArray* a )
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


/* returns the data size of a MATLAB variable in bytes */
size_t get_data_size( const mxArray* a )
{
    size_t data_size = 0;
    
    if( a )
    {
      size_t szElement   = mxGetElementSize( a );
      size_t cntElements = mxGetNumberOfElements( a );
      
      data_size = szElement * cntElements;
    }
    
    return data_size;
}


#endif