/*-------------------------------------------------------------------------
 * fedsrv\AllSrvModule.H
 *
 * How AllSrv runs: as a console program, or as a Windows service.
 *
 * AllSrv used to be a DCOM server and an AGC host as well; both are gone.
 * See MODERNIZATION.md, "Server administration: the DCOM admin layer is
 * gone" and "AGC removed: server events go to a log file".
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
// (ATL is still used for a few helpers, e.g. CComBSTR and TCLib; there is no
// ATL module, COM registration or COM server any more.)
#pragma push_macro("new")
#undef new
#include <atlbase.h>
#include <atlcom.h>
#pragma pop_macro("new")

class CServiceModule
{
// Construction / Destruction
public:
  void         InitEvents();
  void         TermEvents();

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
};

extern CServiceModule g_service;

void PrintSystemErrorMessage(LPCTSTR szBuf, DWORD dwErrorCode);
void WINAPI _ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);

#endif
