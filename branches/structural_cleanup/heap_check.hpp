/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      heap_check.hpp
 *  @brief     Memory manager for leak detection.
 *  @details   Inspired by "Writing Bug-Free C Code" by Jerry Jongerius
 *  @see       http://www.duckware.com/index.html
 *  @author    Martin Kortmann <mail@kortmann.de>
 *  @author    Andreas Martin  <andi.martin@gmx.net>
 *  @version   2.0
 *  @date      2008-2014
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */


#ifndef HEAP_CHECK_HPP
#define HEAP_CHECK_HPP

#include <vector>

#define HC_ASSERT_ERROR       _HC_DoAssert(__FUNCTION__,__LINE__)
#define HC_ASSERT(exp)        if (!(exp)) {HC_ASSERT_ERROR;} else

/// HC_COMP_ASSERT() to verify design-time assumptions at compile-time
#define HC_COMP_ASSERT(exp)   extern char _HC_CompAssert[(exp)?1:-1]

#define HC_ABS(x)             (((x)>0)?(x):-(x))                          ///< absolute value
#define HC_ISPOWER2(x)        (!((x)&((x)-1)))                            ///< check if is a power of 2
#define HC_ALIGNMENT          (sizeof(int))                               ///< memory alignment
#define HC_DOALIGN(num)       (((num)+HC_ALIGNMENT-1)&~(HC_ALIGNMENT-1))  ///< align to integer


/// USE_HC_ASSERT must exist in every module which uses assertions
#define USE_HC_ASSERT                                                   \
static      char  szFILE[]=__FILE__;                                    \
extern "C"  void  HC_ReportAssert( const char*, const char*, long );    \
static      int   _HC_DoAssert( const char* func, int nLine )           \
{                                                                       \
    HC_ReportAssert(szFILE, func, nLine);                               \
    HC_ASSERT(nLine);  /* inhibit removal of unreferenced function */   \
    return(0);                                                          \
}  


USE_HC_ASSERT;


class HeapCheck
{
    struct tagFooter;
    struct tagHeader
    {
        tagFooter*    lpFooter;         ///< pointer to footer
        const char*   lpFilename;       ///< filename or NULL
        const char*   lpFunctionName;   ///< function name
        long          lLineNumber;      ///< line number or 0
        void*         lpMem;            ///< memory block
        const char*   lpNotes;          ///< pointer to further notes or NULL
    };
    
    struct tagFooter
    {
        tagHeader*    lpHeader;         ///< pointer to header of this memory block
    };
    
    
    typedef std::vector<const tagHeader*> vec_tagHeader;
    
    vec_tagHeader m_mem_blocks;
    
public:
    ~HeapCheck()
    {
        Walk();
        
        for( int i = 0; i < m_mem_blocks.size(); i++ )
        {
            if( m_mem_blocks[i] != NULL )
            {
                MEM_FREE( m_mem_blocks[i] );
                m_mem_blocks[i] = NULL;
            }
        }
    }
    
    static
    size_t GetHeaderSize()
    {
        return sizeof( tagHeader );
    }
    
    
    static
    int IsPtrOk( const void* ptr )
    {
        return ( (ptr) && (!( (long)ptr & (HC_ALIGNMENT-1) )) );
    }
    
   
    static
    int VerifyPtr( const void* ptr )
    {
      int bOk=0;

      if( ptr ) 
      {
          HC_ASSERT( IsPtrOk(ptr) ) 
          {
              tagHeader* header = (tagHeader*)ptr - 1;
              HC_ASSERT( header->lpMem == ptr ) 
              {
                  HC_ASSERT( header->lpFooter->lpHeader == header ) 
                  {
                      bOk = 1;
                  }
              }
          }
      }

      return bOk;
    } 
    
    
    void AddPtr( const tagHeader* ptr )
    {
        m_mem_blocks.push_back( ptr );
    }
    
    
    void RemovePtr( const tagHeader* ptr )
    {
        for( int i = 0; i < m_mem_blocks.size(); i++ )
        {
            if( m_mem_blocks[i] == ptr )
            {
                m_mem_blocks.erase( m_mem_blocks.begin() + i );
                break;
            }
        }
    }
    

    void* New( size_t bytes, const char* fcn, const char* notes, long nLine )
    {
        const char* file          = szFILE;
        tagHeader*  mem_block     = NULL;
        size_t      bytes_aligned = HC_DOALIGN( bytes );
        
        mem_block = (tagHeader*)MEM_ALLOC( bytes_aligned + sizeof(tagHeader) + sizeof(tagFooter), 1 );
        
        if( mem_block != NULL )
        {
            mem_block->lpFooter           = (tagFooter*)((char*)(mem_block + 1) + bytes_aligned);
            mem_block->lpFooter->lpHeader = mem_block;
            mem_block->lpMem              = mem_block + 1;
            mem_block->lpFilename         = file;
            mem_block->lpFunctionName     = fcn;
            mem_block->lpNotes            = notes;
            mem_block->lLineNumber        = nLine;
            
            memset( mem_block->lpMem, 0, bytes_aligned );
            
            AddPtr( mem_block );
        }
        else
        {
            HC_ASSERT_ERROR;
        }
        
        return mem_block ? (mem_block + 1) : NULL;
    }
    
    
    void* Realloc( void* ptr_old, size_t bytes,
                     const char* fcn, const char* notes, long nLine )
    {
        const char* file      = szFILE;
        void*   ptr_new       = NULL;
        size_t  bytes_aligned = HC_DOALIGN(bytes);

        // Try to reallocate
        if( ptr_old )
        {
            if( VerifyPtr(ptr_old) )
            {
                tagHeader* header     = (tagHeader*)ptr_old - 1;
                tagHeader* header_new = NULL;
                tagHeader* header_ins = NULL;

                // Try to reallocate block
                RemovePtr( header );
                memset( header->lpFooter, 0, sizeof(tagFooter) );
                header_new = (tagHeader*)MEM_REALLOC( header, sizeof(tagHeader) + bytes_aligned + sizeof(tagFooter) );

                // Add new (or failed old) back in
                header_ins                      = header_new ? header_new : header;
                header_ins->lpFooter            = (tagFooter*)( (char*)(header_ins+1) + bytes_aligned );
                header_ins->lpFooter->lpHeader  = header_ins;
                header_ins->lpMem               = header_ins + 1;
                header_ins->lpFilename          = file  ? file  : header_ins->lpFilename;
                header_ins->lpFunctionName      = fcn   ? fcn   : header_ins->lpFunctionName;
                header_ins->lpNotes             = notes ? notes : header_ins->lpNotes;
                header_ins->lLineNumber         = nLine ? nLine : header_ins->lLineNumber;

                AddPtr( header_ins );


                // Finish
                ptr_new = header_new ? (header_new + 1) : NULL;

                if( !ptr_new )  
                {
                    // Report out of memory error
                    HC_ASSERT_ERROR;
                }
            }
        } 
        else 
        {
            ptr_new = New( bytes_aligned, fcn, notes, nLine );
        }

        // Return address to object
        return ptr_new;
    }
    
    
    void Free( void* ptr )
    {
        
        if( VerifyPtr(ptr) ) 
        {
            tagHeader*  header        = (tagHeader*)ptr - 1;
            size_t      bytes_aligned = (char*)(header->lpFooter+1) - (char*)header;
            
            RemovePtr( header );
            memset( header, 0, sizeof(tagHeader) );
            MEM_FREE( header );
        }
    }
    
    
    static 
    void RenderDesc( const tagHeader* header, char* lpBuffer, size_t szBuffer )
    {
        memset( lpBuffer, 0, szBuffer );
        int left;
        
        if( header->lpMem == &header[1] ) 
        {
            _snprintf( lpBuffer, szBuffer, "%08lx ", header );
            
            if( header->lpFilename && (left = (int)szBuffer - (int)strlen(lpBuffer)) > 1 )
            {
                _snprintf( lpBuffer + strlen(lpBuffer), left, "%12s %4ld ",
                           header->lpFilename, header->lLineNumber );
            }
            if( header->lpFunctionName && (left = (int)szBuffer - (int)strlen(lpBuffer)) > 1 )
            {
                _snprintf( lpBuffer + strlen(lpBuffer), left, " (%s)",
                           header->lpFunctionName );
            }
            if( header->lpNotes && (left = (int)szBuffer - (int)strlen(lpBuffer)) > 1 )
            {
                _snprintf( lpBuffer + strlen(lpBuffer), left, "%s",
                           header->lpNotes );
            }
        } else {
            _snprintf( lpBuffer, szBuffer, "(bad)" );
        }

    }
    
    
    void Walk( const char* text = NULL )
    {
        for( int i = 0; i < m_mem_blocks.size(); i++ ) 
        {
            char buffer[1024];
            
            RenderDesc( m_mem_blocks[i], buffer, 1024 );

#if defined(MATLAB_MEX_FILE) /* MATLAB MEX file */

            /*--- print out buffer ---*/
            if( text )
            {
                mexPrintf( "walk(%s): %s\n", text, buffer );
            } else {
                mexPrintf( "walk: %s\n", buffer );
            }
#endif
        }
    }


};

extern "C"
void HC_ReportAssert( const char* file, const char* lpFunctionName, long line )
{
    char buffer[1024];
    
    _snprintf( buffer, 1024, "Assertion failed in %s, %s line %d\n", file, lpFunctionName, line );
    
#if defined(MATLAB_MEX_FILE) /* MATLAB MEX file */
    mxAssert( 0, buffer );
#else
    assert( 0, buffer );
#endif
}

// Guaranteeing correct prefix structure alignment
HC_COMP_ASSERT( HC_ISPOWER2(HC_ALIGNMENT) );
HC_COMP_ASSERT( !( sizeof(HeapCheck::GetHeaderSize()) % HC_ALIGNMENT ) );

/// Instantiate HeapCheck object
/// One allocator per Module
static HeapCheck HeapCheck;

#endif //  HEAP_CHECK_HPP

