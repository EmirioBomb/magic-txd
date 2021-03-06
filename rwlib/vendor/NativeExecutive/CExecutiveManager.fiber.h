/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.h
*  PURPOSE:     Executive manager fiber logic
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_FIBER_
#define _EXECUTIVE_MANAGER_FIBER_

#define THREAD_PLUGIN_FIBER_STACK       0x00000001

BEGIN_NATIVE_EXECUTIVE

struct FiberStatus;

// size_t logically is the machine word size.
typedef size_t regType_t;
typedef char xmmReg_t[16];

struct Fiber
{
#if defined(_M_IX86)
    // Preserve __cdecl
    // If changing any of this, please update the structs inside of native_routines_x86.asm
    regType_t ebx;   // 0
    regType_t edi;   // 4
    regType_t esi;   // 8
    regType_t esp;   // 12
    regType_t eip;   // 16
    regType_t ebp;   // 20
#elif defined(_M_AMD64)
    // https://msdn.microsoft.com/en-us/library/9z1stfyw.aspx
    // If changing any of this, please update the structs inside of native_routines_x64.asm
    regType_t eip;
    regType_t esp;
    regType_t r12;
    regType_t r13;
    regType_t r14;
    regType_t r15;
    regType_t rdi;
    regType_t rsi;
    regType_t rbx;
    regType_t rbp;
    
    xmmReg_t xmm6;
    xmmReg_t xmm7;
    xmmReg_t xmm8;
    xmmReg_t xmm9;
    xmmReg_t xmm10;
    xmmReg_t xmm11;
    xmmReg_t xmm12;
    xmmReg_t xmm13;
    xmmReg_t xmm14;
    xmmReg_t xmm15;
#else
#error Unsupported architecture for Fibers!
#endif
    
    void *stack_base, *stack_limit;
    void *except_info;

    size_t stackSize;

    // Stack manipulation routines.
    template <typename dataType>
    inline void pushdata( const dataType& data )
    {
        *--((dataType*&)esp) = data;
    }
};

enum eFiberStatus : std::uint32_t
{
    FIBER_RUNNING,
    FIBER_SUSPENDED,
    FIBER_TERMINATED
};

struct FiberStatus
{
    Fiber *callee;      // yielding information

    typedef void (__cdecl*termfunc_t)( FiberStatus *userdata );

    termfunc_t termcb;  // function called when fiber terminates

    eFiberStatus status;
};

typedef void    (__stdcall*FiberProcedure) ( FiberStatus *status );

namespace ExecutiveFiber
{
    // Native assembler methods.
    Fiber*          newfiber        ( FiberStatus *userdata, size_t stackSize, FiberProcedure proc, FiberStatus::termfunc_t termcb );
    Fiber*          makefiber       ( void );
    void            closefiber      ( Fiber *env );

    typedef void* (__cdecl*memalloc_t)( size_t memSize );
    typedef void (__cdecl*memfree_t)( void *ptr );

    void            setmemfuncs     ( memalloc_t malloc, memfree_t mfree );

    void __cdecl    eswitch         ( Fiber *from, Fiber *to );
    void __cdecl    qswitch         ( Fiber *from, Fiber *to );
};

class CFiber : public FiberStatus
{
public:
    friend class CExecutiveManager;
    friend class CExecutiveGroup;

    Fiber *runtime;                 // storing Fiber runtime context
    void *userdata;

    typedef void (__stdcall*fiberexec_t)( CFiber *fiber, void *userdata );

    fiberexec_t callback;           // routine set by the fiber request.

    RwListEntry <CFiber> node;          // node in fiber manager
    RwListEntry <CFiber> groupNode;     // node in fiber group

    CExecutiveGroup *group;         // fiber group this fiber is in.

    CFiber( CExecutiveManager *manager, CExecutiveGroup *group, Fiber *runtime )
    {
        // Using "this" is actually useful.
        this->runtime = runtime;
        this->callee = ExecutiveFiber::makefiber();

        this->manager = manager;
        this->group = group;

        status = FIBER_SUSPENDED;
    }

    inline bool is_running( void ) const        { return ( status == FIBER_RUNNING ); }
    inline bool is_terminated( void ) const     { return ( status == FIBER_TERMINATED ); }
    
    // Manager functions
    void push_on_stack( void );
    void pop_from_stack( void );
    bool is_current_on_stack( void ) const;

    // Native functions that skip manager activity.
    inline void resume( void )
    {
        if ( status == FIBER_SUSPENDED )
        {
            // Save the time that we resumed from this.
            resumeTimer = ExecutiveManager::GetPerformanceTimer();

            status = FIBER_RUNNING;

            // Push the fiber on the current thread's executive stack.
            push_on_stack();

            ExecutiveFiber::eswitch( callee, runtime );
        }
    }

    inline void yield( void )
    {
        assert( status == FIBER_RUNNING );
        assert( is_current_on_stack() );

        // WARNING: only call this function from the fiber stack!
        status = FIBER_SUSPENDED;

        // Pop the fiber from the current active executive stack.
        pop_from_stack();

        ExecutiveFiber::qswitch( runtime, callee );
    }

    // Managed methods that use logic.
    void yield_proc( void );

    CExecutiveManager *manager;

    double resumeTimer;
};

// Thread fiber stack iterator.
struct threadFiberStackIterator
{
    threadFiberStackIterator( CExecThread *thread );
    ~threadFiberStackIterator( void );

    bool IsEnd( void ) const;
    void Increment( void );

    CFiber* Resolve( void ) const;

private:
    CExecThread *thread;

    size_t iter;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_FIBER_