/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      serialize.hpp
 *  @brief     MATLAB hidden (officially undocumented) feature of serializing data
 *  @details   MATLAB arrays can be serialized into a byte stream for "streaming".
 *             With this feature any variable type can be stored as BLOB into a SQL database.
 *  @see       http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data
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
#include "value.hpp"


// (de-)serializing functions
// References:
// https://www.mathworks.com/matlabcentral/fileexchange/29457-serializedeserialize (Tim Hutt)
// https://www.mathworks.com/matlabcentral/fileexchange/34564-fast-serializedeserialize (Christian Kothe)
// http://undocumentedmatlab.com/blog/serializing-deserializing-matlab-data (Christian Kothe)
// getByteStreamFromArray(), getArrayFromByteStream() (undocumented Matlab functions)

bool have_serialize   ();
bool can_serialize    ();
bool serialize        ( const mxArray* pItem, mxArray*& pByteStream );
bool deserialize      ( const mxArray* pByteStream, mxArray*& pItem );


#ifdef MAIN_MODULE

/// Converts MATLAB variable of any complexity into byte stream
bool serialize( const mxArray* pItem, mxArray*& pByteStream )
{
    assert( NULL == pByteStream && NULL != pItem );
    
#if CONFIG_EARLY_BIND_SERIALIZE
    pByteStream = mxSerialize( pItem );
    return NULL != pByteStream;
#endif
    
    if( have_serialize() )
    {
        mexCallMATLAB( 1, &pByteStream, 1, const_cast<mxArray**>( &pItem ), "getByteStreamFromArray" ) ;
    }
    
    return NULL != pByteStream;
}


/// Converts byte stream back into originally MATLAB variable
bool deserialize( const mxArray* pByteStream, mxArray*& pItem )
{
    assert( NULL != pByteStream && NULL == pItem );
    
#if CONFIG_EARLY_BIND_SERIALIZE
    pItem = mxDeserialize( mxGetData( pByteStream ), mxGetNumberOfElements( pByteStream ) );
    return NULL != pItem;
#endif
    
    if( have_serialize() )
    {
        mexCallMATLAB( 1, &pItem, 1, const_cast<mxArray**>( &pByteStream ), "getArrayFromByteStream" );
        return NULL != pItem;
    }
    
    return NULL != pItem;
}


/// Returns true, if current MATLAB version supports serialization
bool have_serialize()
{
#if CONFIG_EARLY_BIND_SERIALIZE
    static int flagHaveSerialize = 1;
#else
    static int flagHaveSerialize = -1;
#endif
        
    if( flagHaveSerialize < 0 ) 
    {
        mxArray* pResult = NULL;
        mxArray* pFuncName = mxCreateString( "getByteStreamFromArray" );

        flagHaveSerialize =    pFuncName
                            && 0 == mexCallMATLAB( 1, &pResult, 1, &pFuncName, "exist" )
                            && pResult
                            && 5 == ValueMex( pResult ).GetInt();  // getByteStreamFromArray must be a build-in function

        ::utils_destroy_array( pFuncName );
        ::utils_destroy_array( pResult );
    }
    
    return flagHaveSerialize > 0;
}

/// Returns true, if streaming is switched on (user setting) and serialization is accessible.
bool can_serialize()
{
    return g_streaming && have_serialize();
}


#endif