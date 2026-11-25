/*-------------------------------------------------------------------------
 * fedsrv\AllSrvModule.H
 *
 * How AllSrv runs: as a console program, or as a Windows service. Also the
 * ATL module the AGC objects need inside the process.
 *
 * AllSrv used to be a DCOM server as well (the IAdmin* objects, for
 * AllSrvUI and the client's training mission 7); that is gone. See
 * MODERNIZATION.md, "Server administration: the DCOM admin layer is gone".
 *
 * Owner:
 *
 * Copyright 1986-1999 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/

#ifndef _ALLSRV_MOD_
#define _ALLSRV_MOD_

#ifndef STRICT
  #define STRICT
#endif

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0400
#endif

#define _ATL_APARTMENT_THREADED

// zlib/alloc.h defines "new" as a macro in debug builds; ATL uses
// new(std::nothrow), which the macro breaks. Suspend it for the ATL headers.
#pragma push_macro("new")
#undef new
#include <atlbase.h>
#pragma pop_macro("new")

#include <AGC.h>

//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
class CServiceModule : public CComModule
{
// Construction / Destruction
public:
  HRESULT      Init(HINSTANCE hInst);
  void         Term();
  HRESULT      InitAGC();
  void         TermAGC();

// Operations
public:
  VOID         RunAsExecutable();
  BOOL         InstallService(int argc, char * argv[]);
  BOOL         RemoveService(void);
  static void  WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
  void         StopAllsrv();

// Implementation
protected:
  static void  WINAPI StartServerTerminateThread();
  static void  WINAPI ServiceControl(DWORD dwCode);
  static void  SetSvcStatus(DWORD state, DWORD exitcode);
  static DWORD WINAPI ServerTerminateThread(DWORD dwUnused);

// Data Members
protected:
  IAGCEventLoggerPtr m_spEventLogger;
};

extern CServiceModule _Module;

#pragma push_macro("new")
#undef new
#include <atlcom.h>
#pragma pop_macro("new")

void PrintSystemErrorMessage(LPCTSTR szBuf, DWORD dwErrorCode);
void WINAPI _ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);

#endif
