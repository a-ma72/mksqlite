#pragma once 

#include "config.h"
#include "global.hpp"
#include "sqlite/sqlite3.h"
#include <string>
#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <memory>

using namespace std;

#ifdef _MSC_VER
#pragma warning( disable: 4521 4522 )  // multiple copy constructors and assignment operators specified
#endif

class   ValueBase;
class   ValueMex;
class   ValueSQL;
class   ValueSQLCol;
struct  tagNativeArray;



class ValueBase
{
public:
    bool                m_isConst;        ///< if flagged as non-const, class has memory ownership (custody over m_text and m_blob)
    union
    {
      double            m_float;          ///< floating point representation
      long long         m_integer;        ///< integer representation
      const char*       m_text;           ///< text representation
      const mxArray*    m_blob;           ///< binary large object representation
      const mxArray*    m_pcItem;         ///< MATLAB variable representation
      tagNativeArray*   m_array;          ///< self allocated variable representation
      
      long long         m_largest_field;  ///< lagest member used to copy entire union
    };
    
    ~ValueBase()
    {
        assert( m_isConst || !m_largest_field );
    }
    
// Only derived classes may create Value instances
protected:
    
    ValueBase()
    : m_isConst(true),
      m_largest_field(0)
    {
    }
    
    // Copy ctor
    ValueBase( const ValueBase& other )
    {
        *this       = other;
        m_isConst   = true;
    }
    
    // Move ctor for lvalues
    ValueBase( ValueBase& other )
    {
        *this           = other;
        other.m_isConst = true;
    }
    
    // Move ctor for rvalues (temporary objects)
    ValueBase( ValueBase&& other )
    {
        *this           = std::move(other);
        other.m_isConst = true;
    }
    
    // assignment operator 
    ValueBase& operator=( const ValueBase& other )
    {
        if( this != &other )
        {
            m_isConst       = true;
            m_largest_field = other.m_largest_field;
        }
        
        return *this;
    }
    
    // assignment operator for lvalues
    ValueBase& operator=( ValueBase& other )
    {
        if( this != &other )
        {
            m_isConst       = other.m_isConst;
            m_largest_field = other.m_largest_field;

            other.m_isConst = true;
        }
        
        return *this;
    }
    
    // assignment operator for rvalues (temporary objects)
    ValueBase& operator=( ValueBase&& other )
    {
        if( this != &other )
        {
            m_isConst       = other.m_isConst;
            m_largest_field = other.m_largest_field;

            other.m_isConst = true;
        }
        
        return *this;
    }
};


/**
 * @brief Encapsulating a MATLAB mxArray.
 * 
 * Class Value never takes custody of a MATLAB memory object!
 *
 * It's intended that this class allocates and tends memory space 
 * through other functions than mxCreate*() functions, since these are very 
 * slow.
 *
 * Since a dtor is declared, we have to fulfil the "rule of three"
 * @sa <a href="http://en.wikipedia.org/wiki/Rule_of_three_%28C%2B%2B_programming%29">Wikipedia</a>
 */

class ValueMex : public ValueBase
{
public:
    /**
     * Complexity information about a MATLAB variable.
     * For testing if a variable is able to be packed or not.
     */
    typedef enum {
        TC_EMPTY = 0,       ///< Empty
        TC_SIMPLE,          ///< single non-complex value, char or simple string (SQLite simple types)
        TC_SIMPLE_VECTOR,   ///< non-complex numeric vectors (SQLite BLOB)
        TC_SIMPLE_ARRAY,    ///< multidimensional non-complex numeric or char arrays (SQLite typed BLOB)
        TC_COMPLEX,         ///< structs, cells, complex data (SQLite typed ByteStream BLOB)
        TC_UNSUPP = -1      ///< all other (unsuppored types)
    } type_complexity_e;
    
    
    ValueMex() : ValueBase()
    {
    }
    
    /// Copy ctor
    ValueMex( const ValueMex& other ) : ValueBase( other )
    {
    }
    
    /// Copy ctor for lvalues
    ValueMex( ValueMex& other ) : ValueBase( other )
    {
    }
    
    /// Copy ctor for rvalues (temporary objects)
    ValueMex( ValueMex&& other ) : ValueBase( std::move(other) )
    {
    }
    
    // assignment operator
    ValueMex& operator=( const ValueMex& other )
    {
        ValueBase::operator=( other );
        return *this;
    }
    
    // assignment operator for lvalues
    ValueMex& operator=( ValueMex& other )
    {
        ValueBase::operator=( other );
        return *this;
    }
    
    // assignment operator for rvalues (temporary objects)
    ValueMex& operator=( ValueMex&& other )
    {
        ValueBase::operator=( std::move(other) );
        return *this;
    }
    
    /// @param pxItem externally allocated MATLAB array
    explicit
    ValueMex( const mxArray* pcItem )
    {
        m_isConst = true;
        m_pcItem  = pcItem;
    }
    
   
    /// allocating ctor
    /// @param pxItem externally allocated MATLAB array
    ValueMex( mwIndex m, mwIndex n, int clsid )
    {
        m_pcItem  = mxCreateNumericMatrix( m, n, (mxClassID)clsid, mxREAL );
        m_isConst = false;
    }
    
/*
    void adopt()
    {
        m_isConst = false;
    }
    
    void release()
    {
        m_isConst = true;
    }
*/ 
    /// @brief deallocator
    void Destroy()
    {
        if( !m_isConst && m_pcItem )
        {
            mxDestroyArray( const_cast<mxArray*>(m_pcItem) );
            m_pcItem = NULL;
        }
    }
    
    /// @returns hosted MATLAB array m_pcItem
    inline
    const mxArray* Item() const
    {
        return m_pcItem;
    }


    /// @returns the row count (1st dimension)
    inline
    size_t GetM() const
    {
        return mxGetM(m_pcItem);
    }
    
    /// @returns the col count (2nd dimension)
    inline
    size_t GetN() const
    {
        return mxGetN(m_pcItem);
    }
    
    /// @returns true when m_pcItem is NULL or empty ([])
    inline
    bool IsEmpty() const
    {
        return mxIsEmpty( m_pcItem );
    }

    /// @returns true when m_pcItem is a cell array
    inline
    bool IsCell() const
    {
        return m_pcItem ? mxIsCell( m_pcItem ) : false;
    }

    /// @returns true when m_pcItem is neither NULL nor complex
    inline
    bool IsComplex() const
    {
        return m_pcItem ? mxIsComplex( m_pcItem ) : false;
    }

    /// @returns true when m_pcItem consists of exact 1 element
    inline
    bool IsScalar() const
    {
        return NumElements() == 1;
    }

    /// @returns true when m_pcItem is of type 1xN or Mx1
    inline
    bool IsVector() const
    {
        return NumDims() == 2 && min( GetM(), GetN() ) == 1;
    }
    
    /// @returns true when m_pcItem is of type mxDOUBLE_CLASS
    inline
    bool IsDoubleClass() const
    {
        return mxDOUBLE_CLASS == ClassID();
    }

    /// @returns the number of elements in m_pcItem
    inline
    size_t NumElements() const
    {
        return m_pcItem ? mxGetNumberOfElements( m_pcItem ) : 0;
    }
    
    /// @returns the size in bytes of one element
    inline
    size_t ByElement() const
    {
        return m_pcItem ? mxGetElementSize( m_pcItem ) : 0;
    }
    
    /// @returns the number of dimensions for m_pcItem
    inline
    int NumDims() const
    {
        return m_pcItem ? mxGetNumberOfDimensions( m_pcItem ) : 0;
    }
      
    /// @returns the data size of a MATLAB variable in bytes
    inline
    size_t ByData() const
    {
       return NumElements() * ByElement();
    }
    
    /// @returns the MATLAB class ID for m_pcItem
    inline
    mxClassID ClassID() const
    {
        return m_pcItem ? mxGetClassID( m_pcItem ) : mxUNKNOWN_CLASS;
    }

    /// @param bCanSerialize signals, if serialization of variables is enabled
    /// @returns the complexity of the m_pcItem (for storage issues)
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
    
    /// @returns pointer to raw data
    inline
    void* Data() const
    {
        return mxGetData( m_pcItem );
    }
    
    /**
     * @brief Convert a string to char, due flagUTF converted to utf8
     *
     * @param flagUTF if true, string will be converted to UTF8
     * @param [out] format optional format string (see fprintf())
     * @returns created string (allocator @ref MEM_ALLOC)
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
            count   = mxGetM( org_string ) * mxGetN( org_string ) + 1;  // one extra char for NUL
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
    
    
    /**
     * @brief Convert a string to char, due to global flag converted to utf
     *
     * @returns created string (allocator @ref MEM_ALLOC)
     */
    char* GetEncString() const
    {
        return GetString( g_convertUTF8 ? true : false );
    }


    /**
     * @brief get the integer m_pcItem from value
     *
     * @param errval value to return in case of wrong variable data type
     * @returns value of m_pcItem converted to integer type.
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
    
    /// @returns scalar variable value or NaN in case it's a non scalar
    double GetScalar() const
    {
        return IsScalar() ? mxGetScalar( m_pcItem ) : DBL_NAN;
    }
};


class ValueSQL : public ValueBase
{
public:
    int m_typeID;  // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, SQLITE_NULL ( unused: SQLITE3_TEXT )
    
    ~ValueSQL()
    {
        Destroy();
    }

    ValueSQL()
    {
        m_isConst = true;
        m_typeID  = SQLITE_NULL;
    }
    
    // copy ctor
    ValueSQL( const ValueSQL& other ) : ValueBase( other )
    {
        m_typeID = other.m_typeID;
    }
    
    // copy ctor for lvalues
    ValueSQL( ValueSQL& other ) : ValueBase( other )
    {
        m_typeID = other.m_typeID;
    }
    
    // copy ctor for rvalues (temporary objects)
    ValueSQL( ValueSQL&& other ) : ValueBase( std::move(other) )
    {
        m_typeID = other.m_typeID;
    }
    
    // assignment operator
    ValueSQL& operator=( const ValueSQL& other )
    {
        ValueBase::operator=(other);
        m_typeID = other.m_typeID;
        return *this;
    }

    // assignment operator for lvalues
    ValueSQL& operator=( ValueSQL& other )
    {
        ValueBase::operator=(other);
        m_typeID = other.m_typeID;
        return *this;
    }

    // assignment operator for rvalues (temporary objects)
    ValueSQL& operator=( ValueSQL&& other )
    {
        ValueBase::operator=(std::move(other));
        m_typeID = other.m_typeID;
        return *this;
    }

    explicit
    ValueSQL( double dValue )
    {
        m_isConst = true;
        m_float   = dValue;
        m_typeID  = SQLITE_FLOAT;
    }
    
    explicit
    ValueSQL( long long iValue )
    {
        m_isConst = true;
        m_integer = iValue;
        m_typeID  = SQLITE_INTEGER;
    }
    
    explicit
    ValueSQL( const char* txtValue )
    {
        m_isConst = true;
        m_text    = txtValue;
        m_typeID  = SQLITE_TEXT;
    }
    
    explicit
    ValueSQL( char* txtValue )
    {
        m_isConst = false;
        m_text    = txtValue;
        m_typeID  = SQLITE_TEXT;
    }
    
    explicit
    ValueSQL( const mxArray* blobValue )
    {
        m_isConst = true;
        m_blob    = blobValue;
        m_typeID  = SQLITE_BLOB;
    }

    explicit
    ValueSQL( mxArray* blobValue )
    {
        m_isConst = false;
        m_blob    = blobValue;
        m_typeID  = SQLITE_BLOB;
    }

/*
    void adopt()
    {
        m_isConst = false;
    }
    
    void release()
    {
        m_isConst = true;
    }
*/
    
    void Destroy()
    {
        if( !m_isConst )
        {
            if( m_typeID == SQLITE_TEXT && m_text )
            {
                MEM_FREE( const_cast<char*>(m_text) );
                m_text = NULL;
            }
            
            if( m_typeID == SQLITE_BLOB && m_blob )
            {
                mxDestroyArray( const_cast<mxArray*>(m_blob) );
                m_blob = NULL;
            }
        }
        
        m_typeID = SQLITE_NULL;
    }
};


class ValueSQLCol
{
public:
    string m_col_name;
    string m_name;
    bool   m_isAnyType;
    
    typedef pair<string,string>    StringPair;
    typedef vector<StringPair>     StringPairList;

    vector<ValueSQL>  m_any;
    vector<double>    m_float;
    
    ValueSQLCol( StringPair name )
    : m_col_name(name.first), m_name(name.second), m_isAnyType(false)
    {
    }
    
    ~ValueSQLCol()
    {
        for( int i = 0; i < (int)m_any.size(); i++ )
        {
            m_any[i].Destroy();
        }
    }

    void Destroy( int row )
    {
        if( row < (int)m_any.size() )
        {
            m_any[row].Destroy();
        }
    }
    
    size_t size()
    {
        return m_isAnyType ? m_any.size() : m_float.size();
    }
    
    // Always returns a copy of the original (no reference!)
    const ValueSQL operator[]( int index )
    {
        return m_isAnyType ? const_cast<const ValueSQL&>(m_any[index]) : ValueSQL( m_float[index] );
    }
    
    void swapToAnyType()
    {
        if( !m_isAnyType )
        {
            assert( !m_any.size() );
            
            for( int i = 0; i < (int)m_float.size(); i++ )
            {
                m_any.push_back( ValueSQL(m_float[i]) );
            }

            m_float.clear();
            m_isAnyType = true;
        }
    }
    
    void append( double value )
    {
        if( m_isAnyType )
        {
            m_any.push_back( ValueSQL(value) );
        }
        else
        {
            m_float.push_back( value );
        }
    }
    
    void append( long long value )
    {
        /* Test if integer value can be represented by a double */
        double    dVal  = (double)(value);
        long long llVal = (long long) dVal;

        if( llVal == value )
        {
            // double type is equivalent, and thus preferred
            append( dVal );
        }
        else
        {
            swapToAnyType();
            m_any.push_back( ValueSQL(value) );
        }
    }
    
    void append( const char* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }

    void append( char* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }

    void append( const mxArray* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }
    
    void append( mxArray* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }
    
    void append( const ValueSQL& item )
    {
        switch( item.m_typeID )
        {
          case SQLITE_FLOAT:
            append( item.m_float );
            return;
            
          case SQLITE_INTEGER:
            append( item.m_integer );
            return;
            
          case SQLITE_NULL:
            if( g_NULLasNaN )
            {
                append( DBL_NAN );
            }
            else
            {
                swapToAnyType();
                m_any.push_back( ValueSQL() );
            }
            return;
            
          case SQLITE_TEXT:
          case SQLITE_BLOB:
            swapToAnyType();
            m_any.push_back( item );
            break;
            
          default:
            assert( false );
            break;
        }
    }
    
    void append( ValueSQL& item )
    {
        switch( item.m_typeID )
        {
          case SQLITE_FLOAT:
          case SQLITE_INTEGER:
          case SQLITE_NULL:
              append( (const ValueSQL&)item );
              break;
          case SQLITE_TEXT:
          case SQLITE_BLOB:
            swapToAnyType();
            m_any.push_back( ValueSQL(item) );
            break;
            
          default:
            assert( false );
            break;
        }
    }
};

struct tagNativeArray
{
    size_t          m_elBytes;      ///< size of one single element in bytes
    size_t          m_dims[1];      ///< count of dimensions
    
    tagNativeArray()
    : m_elBytes(0)
    {
        m_dims[0] = 0;
    }

    
    static
    tagNativeArray* CreateArray( size_t nDims, size_t dims[], int typeID )
    {
        tagNativeArray* pThis   = NULL;
        size_t          elBytes = utils_elbytes( (mxClassID)typeID );
        size_t          memBytes;
        
        size_t nElements = 0;
        
        if( nDims )
        {
            nElements = dims[0];
            for( int i = 1; i < (int)nDims; i++ )
            {
                nElements *= dims[i];
            }
        }
        
        memBytes = sizeof(tagNativeArray) +
                   sizeof(dims[0]) * nDims +
                   nElements * elBytes;
        
        pThis = (tagNativeArray*)MEM_ALLOC( memBytes, 1 );
        
        if( pThis )
        {
            pThis->m_elBytes    = elBytes;
            pThis->m_dims[0]    = nDims;
            
            for( int i = 0; i < (int)nDims; i++ )
            {
                pThis->m_dims[i+1] = dims[i];
            }
        }
        
        return pThis;
    }
    
    static
    tagNativeArray* CreateMatrix( size_t m, size_t n, int typeID )
    {
        size_t dims[] = {m, n};
        return CreateArray( 2, dims, typeID );
    }
};