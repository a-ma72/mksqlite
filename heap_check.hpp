/**
 *  <!-- mksqlite: A MATLAB interface to SQLite -->
 * 
 *  @file      heap_check.hpp
 *  @brief     Memory leak detection helper
 *  @details   Inspired by "Writing Bug-Free C Code" by Jerry Jongerius
 *  @see       http://www.duckware.com/index.html
 *  @author    Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.5
 *  @date      2008-2017
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */


#ifndef HEAP_CHECK_HPP
#define HEAP_CHECK_HPP

#include <vector>

#define HC_ASSERT_ERROR       _HC_DoAssert(__FILE__,__FUNCTION__,__LINE__) /**< intermediate macro for expanding __FILE__, __FUNCTION__ and __LINE__ */
#define HC_ASSERT(exp)        if (!(exp)) {HC_ASSERT_ERROR;} else          /**< Assert condition \p exp with reporting */
#define HC_NOTES(ptr,notes)   HeapCheck.UpdateNotes(ptr,notes)             /**< Update field "notes" in memory block header */

/// Verifies design-time assumptions (\p exp) at compile-time
#define HC_COMP_ASSERT(exp)   extern char _HC_CompAssert[(exp)?1:-1]

#define HC_ABS(x)             (((x)>0)?(x):-(x))                          ///< calculate absolute value
#define HC_ISPOWER2(x)        (!((x)&((x)-1)))                            ///< check if is a power of 2
#define HC_ALIGNMENT          (sizeof(int))                               ///< align to integer
#define HC_DOALIGN(num)       (((num)+HC_ALIGNMENT-1)&~(HC_ALIGNMENT-1))  ///< memory alignment


/// Macro \ref USE_HC_ASSERT must exist in every module which uses macro \ref HC_ASSERT.
#define USE_HC_ASSERT                                                               \
extern "C"  void  HC_ReportAssert ( const char*, const char*, long );               \
static      int   _HC_DoAssert    ( const char* file, const char* func, int nLine ) \
{                                                                                   \
    HC_ReportAssert(file, func, nLine);                                             \
    HC_ASSERT(nLine);  /* inhibit removal of unreferenced function */               \
    return(0);                                                                      \
}  

/// \cond
USE_HC_ASSERT;
/// \endcond


/// Helperclass for memory leak and access violation detection
class HeapCheck
{
    struct tagFooter;  // forward declaration
    
    /** 
     * \brief Memory block header
     *
     * Each memory block is surrounded by additional information:
     * A preceding header stores who is responsible to this memory, some
     * additional notes (comments) an a pointer to the end of the memory
     * block (footer).
     * By checking consitency of header and footer, one can test, if any
     * close memory (write) accesses violation occures.
     */
    struct tagHeader
    {
        tagFooter*    lpFooter;         ///< pointer to footer, contiguous to memory block
        const char*   lpFilename;       ///< filename or NULL
        const char*   lpFunctionName;   ///< function name
        long          lLineNumber;      ///< line number or 0
        void*         lpMem;            ///< pointer to memory block (contiguous to this header space)
        const char*   lpNotes;          ///< pointer to further notes or NULL
    };
    
    /** 
     * \brief Memory block footer (end marker)
     *
     * The footer only holds a pointer to the memory block header and is
     * used for consistence check only.
     */
    struct tagFooter
    {
        tagHeader*    lpHeader;         ///< pointer to header of this memory block
    };
    
    /// Memory blocks linked list typedef
    typedef std::vector<const tagHeader*> vec_tagHeader;
    
    /// Linked list of memory blocks used in module scope
    vec_tagHeader m_mem_blocks;
    
    /// Flag will be set, when the block m_mem_blocks is released and checked
    bool flag_blocks_checked;
    
public:
  
    /**
     * \brief Standard ctor
     *
     */
    HeapCheck()
    {
        flag_blocks_checked = false;
    }

    /** 
     * \brief Destructor
     *
     * The destructor frees orphan memory space
     * while reporting.
     */
    ~HeapCheck()
    {
        Release();
    }
    
    
    /** 
     * \brief Releasing unfreed memory
     *
     * Releasing memory, that is logged in the list of
     * allocated memory blocks. Calling this function ensures
     * that heap space is cleanly leaved. If any space is freed, a 
     * message will be displayed in MATLAB command window.
     */
    void Release()
    {
        int count = 0;
        
        Walk();
        
        for( int i = 0; i < (int)m_mem_blocks.size(); i++ )
        {
            if( m_mem_blocks[i] != NULL )
            {
                MEM_FREE( m_mem_blocks[i] );
                m_mem_blocks[i] = NULL;
                count++;
            }
        }    

        m_mem_blocks.clear();

#if defined(MATLAB_MEX_FILE) /* MATLAB MEX file */
        if( !count && !flag_blocks_checked )
        {
            PRINTF( "Heap check: ok\n" );
        }
#endif
        flag_blocks_checked = true;
    }
    
    
    /// Returns the header size in bytes
    static
    size_t GetHeaderSize()
    {
        return sizeof( tagHeader );
    }
    
    
    /** 
     * \brief Checks if header pointer \p ptr is well aligned
     *
     * Headers are always aligned to \a HC_ALIGNMENT. Any other alignment
     * inidcates an error.
     */
    static
    int isPtrAligned( const void* ptr )
    {
        return ( (ptr) && (!( (long)ptr & (HC_ALIGNMENT-1) )) );
    }
    
   
    /** 
     * \brief Checks if header pointer \p ptr is valid
     *
     * Check if \p ptr is well aligned and header and footer are consistent.
     * If not, the was an access violation.
     */
    static
    int VerifyPtr( const void* ptr )
    {
      int bOk = 0;

      if( ptr ) 
      {
          HC_ASSERT( isPtrAligned(ptr) ) 
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
    
    
    /// Enqueues new memory block by header pointer \p ptr
    void AddPtr( const tagHeader* ptr )
    {
        m_mem_blocks.push_back( ptr );
        flag_blocks_checked = false;
    }
    
    
    /// Removes memory block, identified by \p ptr without freeing it
    void RemovePtr( const tagHeader* ptr )
    {
        /// \todo SLOW (sorted list facilitates binary search)
        for( int i = 0; i < (int)m_mem_blocks.size(); i++ )
        {
            if( m_mem_blocks[i] == ptr )
            {
                m_mem_blocks.erase( m_mem_blocks.begin() + i );
                break;
            }
        }
    }
    

    /** 
     * \brief Allocates a new block of memory with initialized header and footer.
     *
     * @param[in] bytes Size of memory needed
     * @param[in] file Source filename with calling function
     * @param[in] fcn Name of calling function
     * @param[in] notes Additional (optional) notes as comment for reporting function
     * @param[in] nLine Line number of calling function
     * @returns Pointer to callers memory block (not to header!) or NULL on fail
     */
    void* New( size_t bytes, const char* file, const char* fcn, const char* notes, long nLine )
    {
        tagHeader*  mem_block     = NULL;
        size_t      bytes_aligned = HC_DOALIGN( bytes );
        
        // Allocate memory with additional space for header and footer
        mem_block = (tagHeader*)MEM_CALLOC( bytes_aligned + sizeof(tagHeader) + sizeof(tagFooter), 1 );
        
        if( mem_block != NULL )
        {
            mem_block->lpFooter           = (tagFooter*)((char*)(mem_block + 1) + bytes_aligned);
            mem_block->lpFooter->lpHeader = mem_block;      // do link
            mem_block->lpMem              = mem_block + 1;  // point to users memory block
            mem_block->lpFilename         = file;
            mem_block->lpFunctionName     = fcn;
            mem_block->lpNotes            = notes;
            mem_block->lLineNumber        = nLine;
            
            memset( mem_block->lpMem, 0, bytes_aligned );   // zero init
            
            AddPtr( mem_block );  // Enqueue
        }
        else
        {
            HC_ASSERT_ERROR;
        }
        
        
        return mem_block ? (mem_block + 1) : NULL;
    }
    
    
    /** 
     * \brief Reallocates a block of memory allocated with New()
     *
     * @param[in] ptr_old Pointer returned by New() or NULL
     * @param[in] bytes Size of memory needed
     * @param[in] file Source filename with calling function
     * @param[in] fcn Name replacement of calling function or NULL if not
     * @param[in] notes Additional (optional) notes replacement as comment for reporting function or NULL if not
     * @param[in] nLine Line number replacement of calling functionor 0 if not
     * @returns Pointer to callers memory block (not to header!) or NULL on fail
     */
    void* Realloc( void* ptr_old, size_t bytes,
                   const char* file, const char* fcn, const char* notes, long nLine )
    {
        void*   ptr_new       = NULL;
        size_t  bytes_aligned = HC_DOALIGN(bytes);

        // Try to reallocate previously allocated space
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
            // Pointer was NULL, do a normal allocation
            ptr_new = New( bytes_aligned, file, fcn, notes, nLine );
        }

        // Return address to object
        return ptr_new;
    }
    
    
    /// Freeing space returned from New() or Realloc()
    void Free( void* ptr )
    {
        if( VerifyPtr(ptr) ) 
        {
            tagHeader*  header        = (tagHeader*)ptr - 1;
            size_t      bytes_aligned = (char*)(header->lpFooter+1) - (char*)header;
            
            RemovePtr( header );  // dequeue
            memset( header, 0, sizeof(tagHeader) );  // set to zero
            MEM_FREE( header );  // and free
        }
    }
    
    
    /// Update "notes" field in memory block header
    void UpdateNotes( void* ptr, const char* notes )
    {
        if( VerifyPtr(ptr) ) 
        {
            tagHeader*  header = (tagHeader*)ptr - 1;

            header->lpNotes = notes;
        }
    }
    
    
    /** 
     * \brief Formatted output of memory block information (from its header)
     *
     * @param[in] header Pointer to the header identifying the memory block
     * @param[in,out] lpBuffer Buffer to hold the output (ASCII)
     * @param[in] szBuffer Size of the buffer \p lpBuffer
     */
    static 
    void RenderDesc( const tagHeader* header, char* lpBuffer, size_t szBuffer )
    {
        memset( lpBuffer, 0, szBuffer );
        int left;
        
        if( header->lpMem == &header[1] ) 
        {
            _snprintf( lpBuffer, szBuffer, "%08lx ", (long unsigned)header );
            
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
                _snprintf( lpBuffer + strlen(lpBuffer), left, " %s",
                           header->lpNotes );
            }
        } else {
            _snprintf( lpBuffer, szBuffer, "(bad)" );
        }

    }
    
    
    /** 
     * \brief Reporting walk through the linked memory list
     *
     * @param[in] text Text which is additionally outputted to each memory block report, or NULL if not
     */
    void Walk( const char* text = NULL )
    {
        for( int i = 0; i < (int)m_mem_blocks.size(); i++ ) 
        {
            char buffer[1024];
            
            RenderDesc( m_mem_blocks[i], buffer, 1024 );

#if defined(MATLAB_MEX_FILE) /* MATLAB MEX file */

            /*--- print out buffer ---*/
            if( text )
            {
                PRINTF( "walk(%s): %s\n", text, buffer );
            } else {
                PRINTF( "walk: %s\n", buffer );
            }
#endif
        }
    }
};


/// One module must define \def MAIN_MODULE
#if defined( MAIN_MODULE )

    /** 
     * \brief Standard assert routine used by macro \ref HC_ASSERT
     *
     * @param[in] file Callers filename
     * @param[in] lpFunctionName Callers function name
     * @param[in] line Callers line numer
     */
    extern "C"
    void HC_ReportAssert( const char* file, const char* lpFunctionName, long line )
    {
        char buffer[1024];

        _snprintf( buffer, 1024, "Assertion failed in %s, %s line %ld\n", file, lpFunctionName, line );

    #if defined(MATLAB_MEX_FILE) /* MATLAB MEX file */
        mxAssert( 0, buffer );
    #else
        assert( false );
    #endif
    }

    /// Instantiate HeapCheck object in main module
    class HeapCheck HeapCheck;
#else
    /// Other modules references the HeapCheck object from main module
    extern class HeapCheck HeapCheck;
#endif
    
    
// Guaranteeing correct prefix structure alignment
/// \cond
HC_COMP_ASSERT( HC_ISPOWER2(HC_ALIGNMENT) );
HC_COMP_ASSERT( !( sizeof(HeapCheck::GetHeaderSize()) % HC_ALIGNMENT ) );
/// \endcond

#endif //  HEAP_CHECK_HPP

