/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      value.hpp
 *  @brief     Value container for MATLAB/SQL data
 *  @details   Classes for value interchange between MATLAB and SQL.
 *             - ValueBase as common root
 *             - ValueMex holding MATLAB arrays
 *             - ValueSQL holding SQL values (single table element)
 *             - ValueSQLCol holding a complete table column
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


#define SQLITE_BLOBX 20   ///< Identifier to flag another allocator as used for SQLITE_BLOB

/// Asserting size of SQLite 64-bit integer type at least size of long long type
HC_ASSERT( sizeof( sqlite3_int64 ) <= sizeof( long long ) );

/**
 * \brief Base class for ValueMex and ValueSQL
 *
 * This class doesn't free objects' memory nor manage its lifetime!
 * Member \p m_largest_field is only used to copy the content of the union.
 * 
 */
class ValueBase
{
public:
    bool                m_isConst;          ///< if flagged as non-const, class may swap memory ownership (custody)
    union
    {
      double            m_float;            ///< floating point representation
      sqlite3_int64     m_integer;          ///< integer representation
      char*             m_text;             ///< text representation or allocated memory ()
      mxArray*          m_blob;             ///< binary large object representation
      mxArray*          m_pcItem;           ///< MATLAB variable representation
      tagNativeArray*   m_array;            ///< self allocated variable representation
      
      long long         m_largest_field;    ///< largest member used to copy entire union (dummy field)
    };
    
    /// Dtor
    ~ValueBase()
    {
        // Class ValueBase manages no memory, so when object runs out of scope memory must have been freed
        // already or be const either.
        assert( m_isConst || !m_largest_field );
    }
    
// Only derived classes may create class instances
protected:
    
    /// Standard ctor
    ValueBase()
    : m_isConst(true),
      m_largest_field(0)
    {
    }
    
    /// Copy ctor for constant objects
    ValueBase( const ValueBase& other )
    {
        m_isConst   = true;
        *this       = other;
    }
    
    /// Move ctor for lvalues
    ValueBase( ValueBase& other )
    {
        m_isConst       = true;
        *this           = other;
        other.m_isConst = true;   // taking ownership
    }
    
    /// Move ctor for rvalues (temporary objects)
    ValueBase( ValueBase&& other )
    {
        m_isConst       = true;
        *this           = other;
        other.m_isConst = true;   // taking ownership
    }
    
    /// Assignment operator 
    ValueBase& operator=( const ValueBase& other )
    {
        // checking self assignment
        if( this != &other )
        {
            assert( m_isConst || !m_largest_field );
            m_isConst       = true;
            m_largest_field = other.m_largest_field;
        }
        
        return *this;
    }
    
    /// Move assignment operator for lvalues
    ValueBase& operator=( ValueBase& other )
    {
        // checking self assignment
        if( this != &other )
        {
            assert( m_isConst || !m_largest_field );
            m_isConst       = other.m_isConst;
            m_largest_field = other.m_largest_field;

            other.m_isConst = true;   // taking ownership
        }
        
        return *this;
    }
    
    /// Move assignment operator for rvalues (temporary objects)
    ValueBase& operator=( ValueBase&& other )
    {
        // checking self assignment
        if( this != &other )
        {
            assert( m_isConst || !m_largest_field );
            m_isConst       = other.m_isConst;
            m_largest_field = other.m_largest_field;

            other.m_isConst = true;   // taking ownership
        }
        
        return *this;
    }
};


/**
 * \brief Encapsulating a MATLAB mxArray.
 * 
 * Class ValueMex never takes custody of a MATLAB memory object! \n
 * Even though there is a function Destroy() which frees the MATLAB object,
 * this class has no destructor which automatically does.
 *
 * It's intended that this class allocates and tends memory space 
 * through other functions than mxCreate*() functions, since these are quite 
 * slow. (see \ref tagNativeArray)
 *
 * If a dtor is declared, we have to fulfil the "rule of three"
 * \sa <a href="http://en.wikipedia.org/wiki/Rule_of_three_%28C%2B%2B_programming%29">Wikipedia</a>
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

    enum {
        LOGICAL_CLASS  = mxLOGICAL_CLASS,
        INT8_CLASS     = mxINT8_CLASS,
        UINT8_CLASS    = mxUINT8_CLASS,
        INT16_CLASS    = mxINT16_CLASS,
        INT32_CLASS    = mxINT32_CLASS,
        UINT16_CLASS   = mxUINT16_CLASS,
        UINT32_CLASS   = mxUINT32_CLASS,
        INT64_CLASS    = mxINT64_CLASS,
        DOUBLE_CLASS   = mxDOUBLE_CLASS,
        SINGLE_CLASS   = mxSINGLE_CLASS,
        CHAR_CLASS     = mxCHAR_CLASS,
    };
    
    
    /// Standard ctor
    ValueMex() : ValueBase()
    {
    }
    
    /// Copy ctor for const objects
    ValueMex( const ValueMex& other ) : ValueBase( other )
    {
    }
    
    /// Move ctor for lvalues
    ValueMex( ValueMex& other ) : ValueBase( other )
    {
    }
    
    /// Move ctor for rvalues (temporary objects)
    ValueMex( ValueMex&& other ) : ValueBase( other )
    {
    }
    
    /// Assignment operator for const objects
    ValueMex& operator=( const ValueMex& other )
    {
        ValueBase::operator=( other );
        return *this;
    }
    
    /// Move assignment operator for lvalues
    ValueMex& operator=( ValueMex& other )
    {
        ValueBase::operator=( other );
        return *this;
    }
    
    /// Move assignment operator for rvalues (temporary objects)
    ValueMex& operator=( ValueMex&& other )
    {
        ValueBase::operator=( other );
        return *this;
    }
    
    /**
     * \brief Copy ctor for mxArrays
     * 
     *  \param[in] pcItem MATLAB array
     */
    explicit
    ValueMex( const mxArray* pcItem )
    {
        m_isConst = true;
        m_pcItem  = const_cast<mxArray*>(pcItem);
    }
    
   
    /**
     * \brief Ctor allocating new MATLAB matrix object
     *
     * \param[in] m Number of rows
     * \param[in] n Number of columns
     * \param[in] clsid Class ID of value type (refer MATLAB manual)
     */
    ValueMex( mwIndex m, mwIndex n, int clsid = mxDOUBLE_CLASS )
    {
        m_pcItem  = mxCreateNumericMatrix( m, n, (mxClassID)clsid, mxREAL );
        m_isConst = false;
    }


    /**
     * @brief Take ownership (custody) of a MEX array
     * 
     * @param doAdopt true to adopt, false to make it const
     */
    ValueMex& Adopt( bool doAdopt = true )
    {
        m_isConst = !doAdopt;
        return *this;
    }


    /**
     * @brief Create a cell array
     * 
     * @param m Number of rows
     * @param n Number of cols
     * 
     * @return Cell array
     */
    static
    ValueMex CreateCellMatrix( int m, int n )
    {
        return ValueMex( mxCreateCellMatrix( m, n ) ).Adopt();
    }


    /**
     * @brief Create a double scalar
     * 
     * @param value Scalar value
     * @return Scalar value matrix
     */
    static
    ValueMex CreateDoubleScalar( double value )
    {
        return ValueMex( mxCreateDoubleScalar( value ) ).Adopt();
    }


    /**
     * @brief Create a string
     * 
     * @param str String value
     * @return String as char array
     */
    static
    ValueMex CreateString( const char* str )
    {
        return ValueMex( mxCreateString( str ) ).Adopt();
    }

    
    /**
     * \brief Dtor
     *
     * If this class has its custody, memory is freed.
     */
    void Destroy()
    {
        if( !m_isConst && m_pcItem )
        {
            // Workaround to avoid MATLAB crash with persistent arrays ("Case 02098404", Lucas Lebert, MathWorks Technical Support Department 
            mxArray* tmp = m_pcItem;
            m_pcItem = NULL;
            mxDestroyArray( m_pcItem );
        }
    }

    
    /**
     * \brief Returns hosted MATLAB array
     */
    inline
    const mxArray* Item() const
    {
        return const_cast<const mxArray*>( m_pcItem );
    }


    /**
     * \brief Returns hosted MATLAB array
     */
    inline
    mxArray* Item()
    {
        return m_pcItem;
    }


    /**
     * \brief Returns a duplictae of the hosted MATLAB array
     */
    inline
    ValueMex Duplicate() const
    {
        return ValueMex( m_pcItem ? mxDuplicateArray( m_pcItem ) : NULL ).Adopt();
    }


    /**
     * \brief Detach hosted MATLAB array
     */
    inline
    mxArray* Detach()
    {
        assert( !m_isConst );
        mxArray* pcItem = m_pcItem;
        m_largest_field = 0;
        return pcItem;
    }


    /**
     * \brief Returns row count (1st dimension)
     */
    inline
    size_t GetM() const
    {
        return mxGetM(m_pcItem);
    }
    
    /**
     * \brief Returns col count (2nd dimension)
     */
    inline
    size_t GetN() const
    {
        return mxGetN(m_pcItem);
    }
    
    /**
     * \brief Returns true if item is NULL or empty ([])
     */
    inline
    bool IsEmpty() const
    {
        return !m_pcItem || mxIsEmpty( m_pcItem );
    }

    /**
     * \brief Returns true if item is a cell array
     */
    inline
    bool IsCell() const
    {
        return m_pcItem ? mxIsCell( m_pcItem ) : false;
    }

    /**
     * \brief Returns true if item is not NULL and complex
     */
    inline
    bool IsComplex() const
    {
        return m_pcItem ? mxIsComplex( m_pcItem ) : false;
    }

    /**
     * \brief Returns true if item consists of exact 1 element
     */
    inline
    bool IsScalar() const
    {
        return NumElements() == 1;
    }

    /**
     * \brief Returns true if item is a struct array
     */
    inline
    bool IsStruct() const
    {
        return mxIsStruct( m_pcItem );
    }

    /**
     * \brief Returns true if m_pcItem is of size 1xN or Mx1
     */
    inline
    bool IsVector() const
    {
        return NumDims() == 2 && min( GetM(), GetN() ) == 1;
    }
    
    /**
     * \brief Returns true if m_pcItem is of type mxDOUBLE_CLASS
     */
    inline
    bool IsDoubleClass() const
    {
        return mxDOUBLE_CLASS == ClassID();
    }

    /**
     * \brief Returns true if m_pcItem is of type mxFUNCTION_CLASS
     */
    inline
    bool IsFunctionHandle() const
    {
        return mxFUNCTION_CLASS == ClassID();
    }

    /**
     * \brief Returns number of elements
     */
    inline
    size_t NumElements() const
    {
        return m_pcItem ? mxGetNumberOfElements( m_pcItem ) : 0;
    }
    
    /**
     * \brief Returns size in bytes of one element
     */
    inline
    size_t ByElement() const
    {
        return m_pcItem ? mxGetElementSize( m_pcItem ) : 0;
    }
    
    /**
     * \brief Returns number of dimensions
     */
    inline
    int NumDims() const
    {
        return m_pcItem ? mxGetNumberOfDimensions( m_pcItem ) : 0;
    }
      
    /**
     * \brief Returns data size in bytes
     */
    inline
    size_t ByData() const
    {
       return NumElements() * ByElement();
    }
    
    /**
     * \brief Returns item class ID or mxUNKNOWN_CLASS if item is NULL
     */
    inline
    mxClassID ClassID() const
    {
        return m_pcItem ? mxGetClassID( m_pcItem ) : mxUNKNOWN_CLASS;
    }

    /**
     * \brief Get complexity information. Which storage level is necessary (scalar, vector, matrix, text, blob)
     *
     * \param[in] bCanSerialize true if serialization of item is enabled
     * \returns The item complexity (for storage issues)
     */
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
    
    /**
     * \brief Returns pointer to raw data
     */
    inline
    void* Data() const
    {
        return !IsEmpty() ? mxGetData( m_pcItem ) : NULL;
    }
    
    /**
     * \brief Convert a string to char, due flagUTF converted to utf8
     *
     * \param[in] flagUTF if true, string will be converted to UTF8
     * \param[out] format optional format string (see fprintf())
     * \returns created string (allocator \ref MEM_ALLOC)
     */
    char *GetString( bool flagUTF = false, const char* format = NULL ) const
    {
        size_t      count;
        char*       result          = NULL;
        mxArray*    new_string      = NULL;
        mxArray*    org_string      = m_pcItem;
        
        // reformat original string with MATLAB function "sprintf" into new string
        if( format )
        {
            mxArray* args[2] = { mxCreateString( format ), org_string };

            mexCallMATLAB( 1, &new_string, 2, args, "sprintf" );
            mxDestroyArray( args[0] );  // destroy format string
            
            org_string = new_string;
        }
        
        // get character stream from original string (MATLAB array)
        if( org_string )
        {
            char* temp = mxArrayToString( org_string );  // Handles multibyte strings, too
            
            // Copy string with own memory management
            if( temp )
            {
                count  = strlen( temp ) + 1;
                result = (char*) MEM_ALLOC( count, sizeof(char) );
                
                if( result )
                {
                    memcpy( result, temp, count );
                }
                
                mxFree( temp );
            }
        }

        // try to retrieve the character stream
        if( !result )
        {
            // free memory and return with error
            ::utils_free_ptr( result );
            ::utils_destroy_array( new_string );
            mexErrMsgTxt( getLocaleMsg( MSG_CANTCOPYSTRING ) );
        }
        
        // reformatted string is no longer needed
        ::utils_destroy_array( new_string );

        // convert to UFT
        if( flagUTF )
        {
            char *buffer = NULL;
            int buflen;

            /* get only the buffer size needed */
            buflen = utils_latin2utf( (unsigned char*)result, (unsigned char*)buffer );
            buffer = (char*) MEM_ALLOC( buflen, sizeof(char) );

            if( !buffer )
            {
                ::utils_free_ptr( result ); // Needless due to mexErrMsgTxt(), but clean
                mexErrMsgTxt( getLocaleMsg( MSG_CANTCOPYSTRING ) );
            }

            /* encode string to utf now */
            ::utils_latin2utf( (unsigned char*)result, (unsigned char*)buffer );

            ::utils_free_ptr( result );

            result = buffer;
        }

        return result;
    }
    
    
    /**
     * \brief Returns allocated memory with items test, due to global flag converted to UTF
     *
     * \returns created string (allocator \ref MEM_ALLOC)
     */
    char* GetEncString() const
    {
        return GetString( g_convertUTF8 ? true : false );
    }


    /**
     * \brief Get integer value from item
     *
     * \param errval Value to return in case of non-convertible variable data type
     * \returns Item value converted to integer type.
     */
    int GetInt( int errval = 0 ) const
    {
        if( !IsEmpty() )
        {
            switch( ClassID() )
            {
                case mxINT8_CLASS  :  return (int) *( (int8_t*)   Data() );
                case mxUINT8_CLASS :  return (int) *( (uint8_t*)  Data() );
                case mxINT16_CLASS :  return (int) *( (int16_t*)  Data() );
                case mxUINT16_CLASS:  return (int) *( (uint16_t*) Data() );
                case mxINT32_CLASS :  return (int) *( (int32_t*)  Data() );
                case mxUINT32_CLASS:  return (int) *( (uint32_t*) Data() );
                case mxSINGLE_CLASS:  return (int) *( (float*)    Data() );
                case mxDOUBLE_CLASS:  return (int) *( (double*)   Data() );
                case mxLOGICAL_CLASS: return (int) mxIsLogicalScalarTrue( m_pcItem );

                default: 
                    assert( false );
                    return errval;
            }
        }

        return errval;
    }
    
    /**
     * \brief Get 64 bit integer value from item
     *
     * \param errval Value to return in case of non-convertible variable data type
     * \returns Item value converted to integer type.
     */
    sqlite3_int64 GetInt64( int errval = 0 ) const
    {
        if( !IsEmpty() )
        {
            switch( ClassID() )
            {
                case mxINT64_CLASS : return *( (sqlite3_int64*)  Data() );

                default: 
                    assert( false );
                    return (sqlite3_int64) errval;
            }
        }

        return errval;
    }
    
    /// \returns Scalar item value (double), or NaN if it's not a scalar
    double GetScalar() const
    {
        return IsScalar() ? mxGetScalar( m_pcItem ) : DBL_NAN;
    }

    /**
     * \brief Get field from a struct array
     *
     * \param n Index of the array
     * \param name Name of the requested field
     * \returns Handle to mxArray
     */
    const mxArray* GetField( int n, const char* name ) const
    {
        mxArray* result = NULL;
        
        if( m_pcItem )
        {
            result = mxGetField( m_pcItem, n, name );

            if( !result )
            {
                // MATLAB bug? For implicit non-initialized structure members mxGetField()
                // returns NULL.
                // ( Thx Knut Voigtlaender for pointing that out )
                // Workaround: Check if struct has requested field
                if( mxGetFieldNumber( m_pcItem, name ) >= 0 )
                {
                    // Create and return an empty array in this case
                    // (MATLAB destroys it automatically when exitting MEX function)
                    result = mxCreateNumericMatrix( 0, 1, mxDOUBLE_CLASS, mxREAL );
                }
            }
        }
        return result;
    }


    /**
     * @brief Sets a cell of a MATLAB cell array
     * 
     * @param i Index
     * @param cell Cell content
     */
    void SetCell( int i, const mxArray* cell )
    {
        if( m_pcItem )
        {
            mxSetCell( m_pcItem, i, const_cast<mxArray*>(cell) );
        }
    }


    /**
     * @brief Make the MATLAB array persistent
     */
    void MakePersistent()
    {
        if( m_pcItem )
        {
            mexMakeArrayPersistent( m_pcItem );
            m_isConst = false;
        }
    }


    /**
     * @brief Throws an exception if any occured
     */
    void Throw()
    {
        if( !IsEmpty() && mxIsClass( m_pcItem, "MException" ) )
        {
            mxArray* exception = Detach();
            mexCallMATLAB( 0, NULL, 1, &exception, "throw" );
        }

    }


    /**
     * @brief Calling a MATLAB function (handle) with arguemnts
     * 
     * @param lhs Return value (only one)
     * @param exception Exception container
     */
    void Call( ValueMex* lhs, ValueMex* exception )
    {
        assert( IsCell() && !IsEmpty() );

        mxArray* _lhs       = NULL;  // Function return value
        mxArray** prhs      = (mxArray**)Data();  // Function handle and arguments
        mxArray* _exception = mexCallMATLABWithTrap( 1, &_lhs, (int)NumElements(), prhs, "feval" );

        // Function returned results?
        if( _lhs )
        {
            *lhs = ValueMex( _lhs ).Adopt();
            _lhs = NULL;
        }

        // An exception occured?
        if( _exception )
        {
            *exception = ValueMex( _exception ).Adopt();
            _exception = NULL;
        }

        // Cleanup
        
        if( _lhs )
        {
            mxDestroyArray( _lhs );
        }

        if( _exception )
        {
            mxDestroyArray( _exception );
        }
    }
};


/**
 * \brief Class encapsulating a SQL field value
 *
 * SQLite supports following types:
 * - SQLITE_INTEGER
 * - SQLITE_FLOAT
 * - SQLITE_TEXT
 * - SQLITE_BLOB
 * - SQLITE_NULL
 * - SQLITE3_TEXT (not supported)
 *
 * ValueSQL holds one field value of listed types.
 */
class ValueSQL : public ValueBase
{
public:
    int    m_typeID;    ///< Type of SQL value as integer ID
    size_t m_blobsize;  ///< Size of BLOB in bytes (only type SQLITE_BLOBX)
  
    /// Dtor
    ~ValueSQL()
    {
        Destroy();
    }

    /// Standard ctor
    ValueSQL()
    {
        m_blobsize = 0;
        m_typeID   = SQLITE_NULL;
    }
    
    /// Copy ctor for constant objects
    ValueSQL( const ValueSQL& other ) : ValueBase( other )
    {
        m_typeID   = other.m_typeID;
        m_blobsize = other.m_blobsize;
    }
    
    /// Move ctor for lvalues
    ValueSQL( ValueSQL& other ) : ValueBase( other )
    {
        m_typeID   = other.m_typeID;
        m_blobsize = other.m_blobsize;
    }
    
    /// Move ctor for rvalues (temporary objects)
    ValueSQL( ValueSQL&& other ) : ValueBase( other )
    {
        m_typeID   = other.m_typeID;
        m_blobsize = other.m_blobsize;
    }
    
    /// Assignment operator for constant objects
    ValueSQL& operator=( const ValueSQL& other )
    {
        ValueBase::operator=(other);
        m_typeID      = other.m_typeID;
        m_blobsize    = other.m_blobsize;
        return *this;
    }

    /// Move assignment operator for lvalues
    ValueSQL& operator=( ValueSQL& other )
    {
        ValueBase::operator=(other);
        m_typeID      = other.m_typeID;
        m_blobsize    = other.m_blobsize;
        return *this;
    }

    /// Move assignment operator for rvalues (temporary objects)
    ValueSQL& operator=( ValueSQL&& other )
    {
        ValueBase::operator=(other);
        m_typeID      = other.m_typeID;
        m_blobsize    = other.m_blobsize;
        return *this;
    }

    /// Ctor for double type initializer
    explicit
    ValueSQL( double dValue )
    {
        m_float   = dValue;
        m_typeID  = SQLITE_FLOAT;
    }
    
    /// Ctor for llong type initializer
    explicit
    ValueSQL( sqlite3_int64 iValue )
    {
        m_integer = iValue;
        m_typeID  = SQLITE_INTEGER;
    }
    
    /// Ctor for const char* type initializer
    explicit
    ValueSQL( const char* txtValue )
    {
        m_text    = const_cast<char*>(txtValue);
        m_typeID  = SQLITE_TEXT;
    }
    
    /// Ctor for char* type initializer
    explicit
    ValueSQL( char* txtValue )
    {
        m_text    = txtValue;
        m_typeID  = SQLITE_TEXT;
    }
    
    /// Ctor for char* type initializer
    explicit
    ValueSQL( char* blobValue, size_t size )
    {
        m_isConst     = false;
        m_text        = blobValue;
        m_blobsize    = size;
        m_typeID      = SQLITE_BLOBX;
    }
    
    /// Ctor for MATLAB const mxArray* type initializer
    explicit
    ValueSQL( const mxArray* blobValue )
    {
        m_blob    = const_cast<mxArray*>(blobValue);
        m_typeID  = SQLITE_BLOB;
    }

    /// Ctor for MATLAB mxArray* type initializer
    explicit
    ValueSQL( mxArray* blobValue )
    {
        m_isConst = false;
        m_blob    = blobValue;
        m_typeID  = SQLITE_BLOB;
    }


    /// Release custody and return pointer type
    void* Detach()
    {
        assert( m_text && ( m_typeID == SQLITE_TEXT || m_typeID == SQLITE_BLOB || m_typeID == SQLITE_BLOBX ) );
        m_isConst = true;
        return (void*)m_text;
    }

    /**
     * \brief Freeing memory space if having ownership
     *
     * Text and BLOB reserve dynamic memory space to store its contents
     */
    void Destroy()
    {
        extern void blob_free( void** pBlob );  /* sql_builtin_functions.hpp */
        
        if( !m_isConst )
        {
            if( m_typeID == SQLITE_TEXT && m_text )
            {
                ::utils_free_ptr( m_text );
                m_text = NULL;
            }
            
            if( m_typeID == SQLITE_BLOBX && m_text )
            {
                blob_free( (void**)&m_text );
            }
            
            if( m_typeID == SQLITE_BLOB && m_blob )
            {
                mxDestroyArray( m_blob );
                m_blob = NULL;
            }
        }
        
        m_typeID = SQLITE_NULL;
    }
};


/**
 * \brief Class encapsulating a complete SQL table column with type and name
 *
 * In case of double (integer) type, a complete row may be returned as MATLAB matrix (vector).
 * If any (row) element is non scalar, or an integer has no acceptable precise double representation,
 * only a MATALB struct or cell variable can be returned. ValueSQL holds the double type up until
 * its not tenable any more. Then all rows are stored with its individual value types.
 */
class ValueSQLCol
{
private:
    /// Standard ctor (inhibited)
    ValueSQLCol();
    
public:
    string m_col_name;  ///< Table column name (SQL)
    string m_name;      ///< Table column name (MATLAB)
    bool   m_isAnyType; ///< true, if it's pure double (integer) type
    
    /// Holds one table column name (first=SQL name, second=MATLAB name)
    typedef pair<string,string>    StringPair;
    typedef vector<StringPair>     StringPairList;  ///< list of string pairs

    vector<ValueSQL>  m_any;    ///< row elements with type information
    vector<double>    m_float;  ///< row elements as pure double type
    
    /// Ctor with column name-pair
    ValueSQLCol( StringPair name )
    : m_col_name(name.first)/*SQL*/, m_name(name.second)/*MATLAB*/, m_isAnyType(false)
    {
    }
    
    /**
     * \brief Dtor
     *
     * Elements taking memory space will be freed.
     */
    ~ValueSQLCol()
    {
        for( int i = 0; i < (int)m_any.size(); i++ )
        {
            m_any[i].Destroy();
        }
    }

    /**
     * \brief Deleting a single row element
     *
     * \param[in] row Row number (0 based)
     */
    void Destroy( int row )
    {
        if( m_isAnyType && row < (int)m_any.size() )
        {
            m_any[row].Destroy();
        }
    }
    
    /// Returns the row count
    size_t size()
    {
        return m_isAnyType ? m_any.size() : m_float.size();
    }
    
    /**
     * \brief Indexing operator
     * 
     * \param[in] index Row number (0 based)
     * \returns Always returns a copy of the original (no reference!)
     */
    const ValueSQL operator[]( int index )
    {
        return m_isAnyType ? const_cast<const ValueSQL&>(m_any[index]) : ValueSQL( m_float[index] );
    }
    
    /**
     * \brief Transform storage type
     *
     * Switches from pure double representation to individual value types.
     * Each former double element will be converted to ValueSQL type.
     */
    void swapToAnyType()
    {
        if( !m_isAnyType )
        {
            assert( !m_any.size() );
            
            // convert each element to ValueSQL type
            for( int i = 0; i < (int)m_float.size(); i++ )
            {
                m_any.push_back( ValueSQL(m_float[i]) );
            }

            // clear old value vector (double types) and flag new column type
            m_float.clear();
            m_isAnyType = true;
        }
    }
    
    /// Appends a new row element (floating point)
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
    
    /// Appends a new row element (integer)
    void append( sqlite3_int64 value )
    {
        /* Test if integer value can be represented as double type */
        double    dVal  = (double)(value);
        long long llVal = (sqlite3_int64) dVal;

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
    
    /// Appends a new row element (const text)
    void append( const char* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }

    /// Appends a new row element (non-const text)
    void append( char* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }

    /// Appends a new row element (const MATLAB array)
    void append( const mxArray* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }
    
    /// Appends a new row element (non-const MATLAB array)
    void append( mxArray* value )
    {
        swapToAnyType();
        m_any.push_back( ValueSQL(value) );
    }
    
    /// Appends a new row element (const SQL value)
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
    
    /// Appends a new row element (non-const SQL value)
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

/**
 * \brief mxArray replacement for speed improvement
 *
 * \todo not supported/used yet
 * Allocating a mxArray is time expensive. BLOB items are handled as
 * mxArrays. Extensive usage is very slow. It's untested whether SQL or the
 * mxArray is the bottleneck. Speed improvements may be possible...
 */
struct tagNativeArray
{
    size_t          m_elBytes;      ///< size of one single element in bytes
    size_t          m_dims[1];      ///< count of dimensions
    
    /// Ctor
    tagNativeArray()
    : m_elBytes(0)
    {
        m_dims[0] = 0;
    }

    /**
     * \brief Array allocator
     *
     */
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
    
    /**
     * \brief Matrix allocator
     */
    static
    tagNativeArray* CreateMatrix( size_t m, size_t n, int typeID )
    {
        size_t dims[] = {m, n};
        return CreateArray( 2, dims, typeID );
    }
    
    /**
     * \brief Array deallocator
     *
     */
    static
    void FreeArray( tagNativeArray* pNativeArray )
    {
        MEM_FREE( pNativeArray );
    }
};
