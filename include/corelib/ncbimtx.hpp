#ifndef NCBIMTX__HPP
#define NCBIMTX__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- fast mutexes;  platform-specific headers and defines
 *
 *   MUTEX:
 *      CInternalMutex   -- platform-dependent mutex functionality
 *      CFastMutex       -- simple mutex with fast lock/unlock functions
 *      CFastMutexGuard  -- acquire fast mutex, then guarantee for its release
 *
 */

#include <corelib/ncbithr_conf.hpp>
#include <corelib/ncbicntr.hpp>
#include <list>
#include <memory>

#if defined(_DEBUG)
#   define  INTERNAL_MUTEX_DEBUG
#else
#   undef   INTERNAL_MUTEX_DEBUG
#   define  INTERNAL_MUTEX_DEBUG
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// DECLARATIONS of internal (platform-dependent) representations
//
//    TMutex         -- internal mutex type
//

#if defined(NCBI_NO_THREADS)

typedef int TSystemMutex; // fake
# define SYSTEM_MUTEX_INITIALIZER 0

#elif defined(NCBI_POSIX_THREADS)

typedef pthread_mutex_t TSystemMutex;
# define SYSTEM_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#elif defined(NCBI_WIN32_THREADS)

typedef HANDLE TSystemMutex;
# undef SYSTEM_MUTEX_INITIALIZER

#else
#  error Unknown threads type
#endif

class CThreadSystemID
{
public:
    typedef TThreadSystemID TID;

    TID m_ID;

    static CThreadSystemID GetCurrent(void)
        {
            CThreadSystemID id;
#if defined(NCBI_NO_THREADS)
            id.m_ID = TID(0);
#elif defined(NCBI_POSIX_THREADS)
            id.m_ID = pthread_self();
#elif defined(NCBI_WIN32_THREADS)
            id.m_ID = GetCurrentThreadId();
#endif
            return id;
        }
    bool operator==(const CThreadSystemID& id) const
        {
            return m_ID == id.m_ID;
        }
    bool operator!=(const CThreadSystemID& id) const
        {
            return m_ID != id.m_ID;
        }
};

#define THREAD_SYSTEM_ID_INITIALIZER { }

/////////////////////////////////////////////////////////////////////////////
// CMutexException - exceptions generated by mutexes


class CMutexException : public CCoreException
{
public:
    enum EErrCode {
        eLock,
        eUnlock,
        eTryLock,
        eOwner,
        eUninitialized
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CMutexException,CCoreException);
};

/////////////////////////////////////////////////////////////////////////////
//
//  SYSTEM MUTEX
//
//    SSystemFastMutex
//    SSystemMutex
//


/////////////////////////////////////////////////////////////////////////////
//
//  SSystemFastMutex
//
//    Internal platform-dependent mutex implementation
//    To be used by CMutex and CFastMutex only.
//

struct SSystemFastMutex
{
    TSystemMutex m_Handle;

    // initialization flag
    enum EMagic {
        eMutexUninitialized = 0,
        eMutexInitialized = 0x2487adab
    };
    volatile EMagic m_Magic;

protected:
    // check if mutex is initialized
    bool IsInitialized(void) const;
    // check if mutex is initialized
    bool IsUninitialized(void) const;
    // Initialize mutex. 
    // Must be called only once.
    void InitializeStatic(void);
    // Initialize mutex if it located in heap or stack (do not count on zeroed memory)
    // Must be called only once.
    void InitializeDynamic(void);
    // Destroy mutex
    void Destroy(void);

    // Acquire mutex for the current thread (no nesting checks)
    void Lock(void);
    // Release mutex (no owner or nesting checks)
    void Unlock(void);
    // Try to lock, return "true" on success
    bool TryLock(void);

private:
    // Initialize mutex structure.
    // Must be called only once.
    void InitializeHandle(void);
    // Destroy mutex structure.
    // Must be called only once.
    void DestroyHandle(void);

public:
    // Method for throwing exceptions, to make inlined method lighter
    void CheckInitialized(void) const;
    static void ThrowUninitialized(void);
    static void ThrowLockFailed(void);
    static void ThrowUnlockFailed(void);
    static void ThrowTryLockFailed(void);

	friend struct SSystemMutex;
    friend class CAutoInitializeStaticFastMutex;

    friend class CFastMutex;
    friend class CFastMutexGuard;

    friend class CSafeStaticPtr_Base;
};


/////////////////////////////////////////////////////////////////////////////
//
//  SSystemMutex
//
//    Internal platform-dependent mutex implementation
//    To be used by CMutex and CFastMutex only.
//

struct SSystemMutex
{
    SSystemFastMutex m_Mutex;
    CThreadSystemID  m_Owner;  // platform-dependent thread ID
    int              m_Count;  // # of recursive (in the same thread) locks

protected:
    // check if mutex is initialized
    bool IsInitialized(void) const;
    // check if mutex is initialized
    bool IsUninitialized(void) const;
    // Initialize mutex. 
    // Must be called only once.
    void InitializeStatic(void);
    // Initialize mutex if it located in heap or stack (do not count on zeroed memory)
    // Must be called only once.
    void InitializeDynamic(void);
    // Destroy mutex
    void Destroy(void);

    // Acquire mutex for the current thread
    void Lock(void);
    // Release mutex
    void Unlock(void);
    // Try to lock, return "true" on success
    bool TryLock(void);

public:
    // throw exception eOwner
    static void ThrowNotOwned(void);

    friend class CAutoInitializeStaticMutex;

    friend class CMutex;
    friend class CMutexGuard;
};

#if defined(NCBI_NO_THREADS)

#   define DEFINE_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id
#   define DECLARE_CLASS_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id
#   define DEFINE_CLASS_STATIC_FAST_MUTEX(id) \
NCBI_NS_NCBI::SSystemFastMutex id

#   define DEFINE_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id
#   define DECLARE_CLASS_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id
#   define DEFINE_CLASS_STATIC_MUTEX(id) \
NCBI_NS_NCBI::SSystemMutex id

#elif defined(SYSTEM_MUTEX_INITIALIZER)

#   define STATIC_FAST_MUTEX_INITIALIZER \
    { SYSTEM_MUTEX_INITIALIZER, NCBI_NS_NCBI::SSystemFastMutex::eMutexInitialized }

#   define DEFINE_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id = STATIC_FAST_MUTEX_INITIALIZER
#   define DECLARE_CLASS_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id
#   define DEFINE_CLASS_STATIC_FAST_MUTEX(id) \
NCBI_NS_NCBI::SSystemFastMutex id = STATIC_FAST_MUTEX_INITIALIZER

#   define STATIC_MUTEX_INITIALIZER \
    { STATIC_FAST_MUTEX_INITIALIZER, THREAD_SYSTEM_ID_INITIALIZER, 0 }

#   define DEFINE_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id = STATIC_MUTEX_INITIALIZER
#   define DECLARE_CLASS_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id
#   define DEFINE_CLASS_STATIC_MUTEX(id) \
NCBI_NS_NCBI::SSystemMutex id = STATIC_MUTEX_INITIALIZER

#else

#   define NEED_AUTO_INITIALIZE_MUTEX

#   define DEFINE_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id
#   define DECLARE_CLASS_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id
#   define DEFINE_CLASS_STATIC_FAST_MUTEX(id) \
NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id

#   define DEFINE_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticMutex id
#   define DECLARE_CLASS_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticMutex id
#   define DEFINE_CLASS_STATIC_MUTEX(id) \
NCBI_NS_NCBI::CAutoInitializeStaticMutex id

#endif

/////////////////////////////////////////////////////////////////////////////
//
//  CAutoInitializeStaticBase
//
//    Thread safe initializer base class 
//    It is needed on platforms where system mutex struct
//    cannot be initialized at compile time (Win32)
//

class CAutoInitializeStaticBase
{
protected:
    // this method return true only once and wait in all other threads
    bool NeedInitialization(void);
    // this method should be called only once after
    // NeedInitialization() had returned true
    void DoneInitialization(void);

    // use pattern:
    // if ( NeedInitialization() ) {
    //     do some initialization;
    //     DoneInitialization();
    // }
    // here our object is safely initialized

private:
    enum {
        // large enough - should be more than any possible amount 
        // of simultaniously running threads
        kMaxUninitialized = 1000000000,
        // counter of polling cycle before scheduling
        kSpinCounter = 100,
        // max counter of scheduled waits
        kMaxWaitCounter = 1000
    };
    volatile TNCBIAtomicValue m_InitializeCounter;
};

#if defined(NEED_AUTO_INITIALIZE_MUTEX)

/////////////////////////////////////////////////////////////////////////////
//
//  CAutoInitializeStaticFastMutex
//
//    Thread safe initializer of static SSystemFastMutex
//    It is needed on platforms where system mutex struct
//    cannot be initialized at compile time (Win32)
//

class CAutoInitializeStaticFastMutex : protected CAutoInitializeStaticBase
{
public:
    typedef SSystemFastMutex TObject;

    void Lock(void);
    void Unlock(void);

    // return initialized mutex object
    operator TObject&(void);

protected:
    // this method can be called many times
    // it will return only after successful initialization of m_Mutex
    void Initialize(void);

    // return initialized mutex object
    TObject& Get(void);

private:
    TObject m_Mutex;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CAutoInitializeStaticMutex
//
//    Thread safe initializer of static SSystemMutex
//    It is needed on platforms where system mutex struct
//    cannot be initialized at compile time (Win32)
//

class CAutoInitializeStaticMutex : protected CAutoInitializeStaticBase
{
public:
    typedef SSystemMutex TObject;

    void Lock(void);
    void Unlock(void);

    // return initialized mutex object
    operator TObject&(void);

protected:
    // this method can be called many times
    // it will return only after successful initialization of m_Mutex
    void Initialize(void);

    // return initialized mutex object
    TObject& Get(void);

private:
    TObject m_Mutex;
};

#endif

/////////////////////////////////////////////////////////////////////////////
//
//  FAST MUTEX
//
//    CFastMutex::
//    CFastMutexGuard::
//


/////////////////////////////////////////////////////////////////////////////
//
//  CFastMutex::
//
//    Simple mutex with fast lock/unlock functions
//
//  This mutex can be used instead of CMutex if it's guaranteed that
//  there is no nesting. This mutex does not check nesting or owner.
//  It has better performance than CMutex, but is less secure.
//

class CFastMutex
{
public:
    // Create mutex handle
    CFastMutex(void);
    // Close mutex handle (no checks if it's still acquired)
    ~CFastMutex(void);

    /*
    // Acquire mutex for the current thread (no nesting checks)
    void Lock(void);
    // Release mutex (no owner or nesting checks)
    void Unlock(void);
    //
    void TryLock(void);
    */

    // for CFastMutexGuard
    operator SSystemFastMutex&(void);

private:
#if defined(NCBI_WIN32_THREADS)
    // for CRWLock
    HANDLE GetHandle(void) { return m_Mutex.m_Handle; }
#else
    TSystemMutex* GetHandle(void) { return &m_Mutex.m_Handle; }
#endif

    friend class CRWLock;

    // Platform-dependent mutex handle, also used by CRWLock
    SSystemFastMutex m_Mutex;

    // Disallow assignment and copy constructor
    CFastMutex(const CFastMutex&);
    CFastMutex& operator= (const CFastMutex&);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CFastMutexGuard::
//
//    Acquire fast mutex, then guarantee for its release.
//

class CFastMutexGuard
{
public:
    // Register the mutex to be released by the guard destructor.
    CFastMutexGuard(SSystemFastMutex& mtx);

    // Release the mutex, if it was (and still is) successfully acquired.
    ~CFastMutexGuard(void);

    // Release the mutex right now (do not release it in the guard destructor).
    void Release(void);

    // Lock on mutex "mtx" (if it's not guarded yet) and start guarding it.
    // NOTE: it never holds more than one lock on the guarded mutex!
    void Guard(SSystemFastMutex& mtx);

private:
    SSystemFastMutex* m_Mutex;  // the mutex (NULL if released)

    // Disallow assignment and copy constructor
    CFastMutexGuard(const CFastMutexGuard&);
    CFastMutexGuard& operator= (const CFastMutexGuard&);
};


/////////////////////////////////////////////////////////////////////////////
//
//  MUTEX
//
//    CMutex::
//    CMutexGuard::
//



/////////////////////////////////////////////////////////////////////////////
//
//  CMutex::
//
//    Mutex that allows nesting (with runtime checks)
//
//  Allows for recursive locks by the same thread. Checks the mutex
//  owner before unlocking. This mutex should be used when performance
//  is less important than data protection.
//

class CMutex
{
public:
    // 'ctors
    CMutex(void);
    // Report error if the mutex is locked
    ~CMutex(void);

    // for CMutexGuard
    operator SSystemMutex&(void);

    // If the mutex is unlocked, then acquire it for the calling thread.
    // If the mutex is acquired by this thread, then increase the
    // lock counter (each call to Lock() must have corresponding
    // call to Unlock() in the same thread).
    // If the mutex is acquired by another thread, then wait until it's
    // unlocked, then act like a Lock() on an unlocked mutex.
    void Lock(void);

    // Try to acquire the mutex. On success, return "true", and acquire
    // the mutex (just as the Lock() above does).
    // If the mutex is already acquired by another thread, then return "false".
    bool TryLock(void);

    // If the mutex is acquired by this thread, then decrease the lock counter.
    // If the lock counter becomes zero, then release the mutex completely.
    // Report error if the mutex is not locked or locked by another
    // thread.
    void Unlock(void);

private:
    SSystemMutex m_Mutex;    // (low-level functionality is in CInternalMutex)

    // Disallow assignment and copy constructor
    CMutex(const CMutex&);
    CMutex& operator= (const CMutex&);

    friend class CRWLock; // uses m_Mtx and m_Owner members directly
};


/////////////////////////////////////////////////////////////////////////////
//
//  CMutexGuard::
//
//    Acquire the mutex, then guarantee for its release
//

class CMutexGuard
{
public:
    // Acquire the mutex;  register it to be released by the guard destructor.
    CMutexGuard(SSystemMutex& mtx);

    // Release the mutex, if it was (and still is) successfully acquired.
    ~CMutexGuard(void);

    // Release the mutex right now (do not release it in the guard destructor).
    void Release(void);

    // Lock on mutex "mtx" (if it's not guarded yet) and start guarding it.
    // NOTE: it never holds more than one lock on the guarded mutex!
    void Guard(SSystemMutex& mtx);

private:
    SSystemMutex* m_Mutex;  // the mutex (NULL if released)

    // Disallow assignment and copy constructor
    CMutexGuard(const CMutexGuard&);
    CMutexGuard& operator= (const CMutexGuard&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  RW-LOCK
//
//    CRWLock::
//    CAutoRW::
//    CReadLockGuard::
//    CWriteLockGuard::
//


// Forward declaration of internal (platform-dependent) RW-lock representation
class CInternalRWLock;


/////////////////////////////////////////////////////////////////////////////
//
//  CRWLock::
//
//    Read/Write lock related  data and methods
//
//  Allows multiple readers or single writer with recursive locks.
//  R-after-W is considered to be a recursive Write-lock.
//  W-after-R is not allowed.
//
//  NOTE: When _DEBUG is not defined, does not always detect W-after-R
//  correctly, so that deadlock may happen. Test your application
//  in _DEBUG mode firts!
//

class CRWLock
{
public:
    // 'ctors
    CRWLock(void);
    ~CRWLock(void);

    // Acquire the R-lock. If W-lock is already acquired by
    // another thread, then wait until it is released.
    void ReadLock(void);

    // Acquire the W-lock. If R-lock or W-lock is already acquired by
    // another thread, then wait until it is released.
    void WriteLock(void);

    // Try to acquire R-lock or W-lock, respectively. Return immediately.
    // Return TRUE if the RW-lock has been successfully acquired.
    bool TryReadLock(void);
    bool TryWriteLock(void);

    // Release the RW-lock.
    void Unlock(void);

private:
    // platform-dependent RW-lock data
    auto_ptr<CInternalRWLock>  m_RW;
    // Writer ID, one of the readers ID
    CThreadSystemID            m_Owner;
    // Number of readers (if >0) or writers (if <0)
    int                        m_Count;

    // List of all readers or writers (for debugging)
    list<CThreadSystemID>      m_Readers;

    // Disallow assignment and copy constructor
    CRWLock(const CRWLock&);
    CRWLock& operator= (const CRWLock&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CAutoRW::
//
//    Guarantee RW-lock release
//
//  Acts in a way like "auto_ptr<>": guarantees RW-Lock release.
//

class CAutoRW
{
public:
    // Register the RW-lock to be released by the guard destructor.
    // Do NOT acquire the RW-lock though!
    CAutoRW(CRWLock& rw) : m_RW(&rw) {}

    // Release the RW-lock right now (don't release it in the guard destructor)
    void Release(void) { m_RW->Unlock();  m_RW = 0; }

    // Release the R-lock, if it was successfully acquired and
    // not released already by Release().
    ~CAutoRW(void)  { if (m_RW) Release(); }

    // Get the RW-lock being guarded
    CRWLock* GetRW(void) const { return m_RW; }

private:
    CRWLock* m_RW;  // the RW-lock (NULL if not acquired)

    // Disallow assignment and copy constructor
    CAutoRW(const CAutoRW&);
    CAutoRW& operator= (const CAutoRW&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CReadLockGuard::
//
//    Acquire the R-lock, then guarantee for its release
//

class CReadLockGuard : public CAutoRW
{
public:
    // Acquire the R-lock;  register it to be released by the guard destructor.
    CReadLockGuard(CRWLock& rw) : CAutoRW(rw) { GetRW()->ReadLock(); }

private:
    // Disallow assignment and copy constructor
    CReadLockGuard(const CReadLockGuard&);
    CReadLockGuard& operator= (const CReadLockGuard&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CWriteLockGuard::
//
//    Acquire the W-lock, then guarantee for its release
//

class CWriteLockGuard : public CAutoRW
{
public:
    // Acquire the W-lock;  register it to be released by the guard destructor.
    CWriteLockGuard(CRWLock& rw) : CAutoRW(rw) { GetRW()->WriteLock(); }

private:
    // Disallow assignment and copy constructor
    CWriteLockGuard(const CWriteLockGuard&);
    CWriteLockGuard& operator= (const CWriteLockGuard&);
};




/////////////////////////////////////////////////////////////////////////////
//
//  SEMAPHORE
//
//    CSemaphore::   application-wide semaphore
//

class CSemaphore
{
public:
    // 'ctors
    CSemaphore(unsigned int init_count, unsigned int max_count);
    // Report error if the semaphore is locked
    ~CSemaphore(void);

    // If the semaphore's count is zero then wait until it's not zero.
    // Decrement the counter (by one).
    void Wait(void);

    // Wait up to timeout_sec + timeout_nsec/1E9 seconds for the
    // semaphore's count to exceed zero.  If that happens, decrement
    // the counter (by one) and return TRUE; otherwise, return FALSE.
    bool TryWait(unsigned int timeout_sec = 0, unsigned int timeout_nsec = 0);

    // Increment the counter (by "count").
    // Do nothing and throw an exception if counter would exceed "max_count".
    void Post(unsigned int count = 1);

private:
    struct SSemaphore* m_Sem;  // system-specific semaphore data

    // Disallow assignment and copy constructor
    CSemaphore(const CSemaphore&);
    CSemaphore& operator= (const CSemaphore&);
};



#include <corelib/ncbimtx.inl>

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2002/09/20 19:13:58  vasilche
 * Fixed single-threaded mode on Win32
 *
 * Revision 1.13  2002/09/20 13:51:56  vasilche
 * Added #include <memory> for auto_ptr<>
 *
 * Revision 1.12  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.11  2002/08/19 19:37:17  vakatov
 * Use <corelib/ncbi_os_mswin.hpp> in the place of <windows.h>
 *
 * Revision 1.10  2002/08/19 18:15:58  ivanov
 * +include <corelib/winundef.hpp>
 *
 * Revision 1.9  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.8  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.7  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.6  2002/01/23 23:31:17  vakatov
 * Added CFastMutexGuard::Guard()
 *
 * Revision 1.5  2001/05/17 14:53:50  lavr
 * Typos corrected
 *
 * Revision 1.4  2001/04/16 18:45:28  vakatov
 * Do not include system MT-related headers if in single-thread mode
 *
 * Revision 1.3  2001/03/26 22:50:24  grichenk
 * Fixed CFastMutex::Unlock() bug
 *
 * Revision 1.2  2001/03/26 21:11:37  vakatov
 * Allow use of not yet initialized mutexes (with A.Grichenko)
 *
 * Revision 1.1  2001/03/13 22:34:24  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* NCBIMTX__HPP */
