//////////////////////////////////////////////////////////////////////////////
//
// Asserts
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _assert_h_
#define _assert_h_

#include "crtdbg.h"

//////////////////////////////////////////////////////////////////////////////
//
// Make sure we didn't include some other assert mechanism
//
//////////////////////////////////////////////////////////////////////////////

// (The original refused to build if another assert was already defined.
// Current CRT and ATL headers include <assert.h> themselves, so instead the
// zlib definition is (re)established at the end of this file on every
// inclusion, outside the include guard.)

//////////////////////////////////////////////////////////////////////////////
//
// Asserts
//
//////////////////////////////////////////////////////////////////////////////
void ZAssertImpl(bool bSucceeded, const char* psz, const char* pszFile, int line, const char* pszModule);
#define ZRetailAssert(bCond)    ZAssertImpl((bCond) ? true : false, #bCond, __FILE__, __LINE__, __MODULE__)

void retailf(const char* , ...);

extern bool g_bOutput;

#ifdef _DEBUG
    //
    // These are implemented in Win32app.cpp
    //

    inline void ZDebugBreak() { DebugBreak(); }


    void debugf(const char* , ...);
    void ZWarningImpl(bool bSucceeded, const char* psz, const char* pszFile, int line, const char* pszModule);
    bool ZFailedImpl(HRESULT hr, const char* pszFile, int line, const char* pszModule);
    bool ZSucceededImpl(HRESULT hr, const char* pszFile, int line, const char* pszModule);
    void ZDebugOutputImpl(const char* psz);

    #define ZAssert(bCond)    ZAssertImpl((bCond) ? true : false, #bCond, __FILE__, __LINE__, __MODULE__)
    #define ZVerify(bCond)    ZAssert(bCond)
    #define ZWarning(bCond)   ZWarningImpl((bCond) ? true : false, #bCond, __FILE__, __LINE__, __MODULE__)
    #define ZError(psz)       ZAssertImpl(false, psz, __FILE__, __LINE__, __MODULE__)
    #define ZBadCase()        ZError("Bad case in switch statement"); break;
    #define ZFailed(hr)       ZFailedImpl(hr, __FILE__, __LINE__, __MODULE__)
    #define ZSucceeded(hr)    ZSucceededImpl(hr, __FILE__, __LINE__, __MODULE__)
    #define ZDebugOutput(str) ZDebugOutputImpl(str)
#else
    inline void debugf(...) {}

    #define ZDebugBreak()

    #define ZAssert(bCond)
    #define ZWarning(str)
    #define ZVerify(bCond)    (bCond)
    #define ZError(str)
    #define ZBadCase()        break;
    #define ZFailed(hr)       FAILED(hr)
    #define ZSucceeded(hr)    SUCCEEDED(hr)
    #define ZDebugOutput(str)

#endif

#define ZUnimplemented()  ZError("Unimplemented member called")

#define ShouldBe(exp) ZWarning(exp)
#define VerifyHR(exp) ZSucceeded(exp)

#define BreakOnError(expr) if (ZFailed(expr)) break;

//////////////////////////////////////////////////////////////////////////////
//
// Tracing
//
//////////////////////////////////////////////////////////////////////////////

#if (defined(_TRACE) && defined(_DEBUG))
    void ZEnterImpl(const char* pcc);
    void ZExitImpl(const char* pcc);
    void ZTraceImpl(const char* pcc);
    void ZStartTraceImpl(const char* pcc);

    #define ZEnter(str)      ZEnterImpl(str)
    #define ZExit(str)       ZExitImpl(str)
    #define ZTrace(str)      ZTraceImpl(str)
    #define ZStartTrace(str) ZStartTraceImpl(str)
#else
    #define ZEnter(str)       
    #define ZExit(str) 
    #define ZTrace(str)       
    #define ZStartTrace(str)    
#endif

#endif

// Outside the include guard on purpose, see the note at the top.
#undef assert
#define assert(exp)   ZAssert(exp)
