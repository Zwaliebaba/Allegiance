
/*-------------------------------------------------------------------------
 * fedsrv\AllSrvModule.CPP
 * 
 * How AllSrv runs: as a console program, or as a Windows service (see
 * allsrvmodule.h). Also sets up AGC and its event logger.
 * 
 * Owner: 
 * 
 * Copyright 1986-1998 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/

#include "pch.h"
#include <AGC.h>

#ifdef _DEBUG
  #include "FedSrvApp.h"
  #include <AGCDebugHook.h>
  CComObjectGlobal<CAGCDebugHook> g_DebugHook;
#endif // _DEBUG


/////////////////////////////////////////////////////////////////////////////
// Global Initialization

CAGCModule _AGCModule;
CServiceModule _Module;


/////////////////////////////////////////////////////////////////////////////
// External Declarations

extern Global g;


/////////////////////////////////////////////////////////////////////////////
// Object Map: empty, since AllSrv no longer exposes COM classes

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()


/*-------------------------------------------------------------------------
 * PrintSystemErrorMessage()
 *-------------------------------------------------------------------------
 * Paramters:
 *    dwErrorCode: take a dwErrorCode and print what it means as text    
 * 
 */
void PrintSystemErrorMessage(LPCTSTR szText, DWORD dwErrorCode)
{
  LPVOID lpMsgBuf;

  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 
                FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_IGNORE_INSERTS, 
                NULL, 
                dwErrorCode, 
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                (LPTSTR) &lpMsgBuf,
                0,
                NULL 
                );

  printf("%s", (LPCTSTR)szText);
  printf("%s", (LPCTSTR)lpMsgBuf);

  _AGCModule.TriggerEvent(NULL, AllsrvEventID_SystemError, "", -1, -1, -1, 2,
      "Text", VT_LPSTR, szText,
      "Message", VT_LPSTR, lpMsgBuf);

  LocalFree( lpMsgBuf );
}


                                                 


/*-------------------------------------------------------------------------
 * CServiceModule::Init
 *-------------------------------------------------------------------------
 * Purpose:
 *    Init some COM/ATL/AGC stuff
 * 
 */
HRESULT CServiceModule::Init(HINSTANCE hInst)
{
  // Initialize the ATL module
  HRESULT hr;
  ZSucceeded(hr = CComModule::Init(ObjectMap, hInst));
  RETURN_FAILED(hr);

  // Indicate success
  return S_OK;
}


/*-------------------------------------------------------------------------
 * CServiceModule::Term
 *-------------------------------------------------------------------------
 * Purpose:
 *    Terminate some COM/ATL/AGC stuff
 * 
 */
void CServiceModule::Term()
{
  CComModule::Term();
}


HRESULT CServiceModule::InitAGC()
{
  // Initialize AGC
  HRESULT hr;
  ZSucceeded(hr = _AGCModule.Init());
  RETURN_FAILED(hr);

  _AGCModule.SetDebugBreakOnErrors(g.fWantInt3);

  #ifdef _DEBUG
    _AGCModule.GetAGCGlobal()->SetDebugHook(&g_DebugHook);
  #endif // _DEBUG

  // Create the set of available AGCEventID ranges
  CComPtr<IAGCEventIDRanges> spRanges;
  ZSucceeded(hr = spRanges.CoCreateInstance(L"AGC.EventIDRanges"));
  RETURN_FAILED(hr);

  // Add our ranges to it
  ZSucceeded(spRanges->AddByValues(EventID_AGC_LowerBound, EventID_AGC_UpperBound));
  ZSucceeded(spRanges->AddByValues(AllsrvEventID_Allsrv_LowerBound, AllsrvEventID_Allsrv_UpperBound));
  ZSucceeded(spRanges->AddByValues(EventID_Admin_LowerBound, EventID_Admin_UpperBound));

  // Set the ranges of available events
  GetAGCGlobal()->SetAvailableEventIDRanges(spRanges);

  // Create the event logger object
  ZSucceeded(hr = m_spEventLogger.CreateInstance("AGC.EventLogger"));
  RETURN_FAILED(hr);

  // Initialize the event logger object
  CComBSTR bstrEventSource(__MODULE__);
  CComBSTR bstrSettingsFile(ZSettings::Get().GetPath().c_str()); // server.json
  IAGCEventLoggerPrivatePtr spPrivate(m_spEventLogger);
  ZSucceeded(hr = spPrivate->Initialize(bstrEventSource, bstrSettingsFile));
  RETURN_FAILED(hr);

  // Indicate success
  return S_OK;
}

void CServiceModule::TermAGC()
{
  // Shutdown the event logger object
  if (NULL != m_spEventLogger)
  {
    IAGCEventLoggerPrivatePtr spPrivate(m_spEventLogger);
    assert(NULL != spPrivate);

    // Terminate the event logger
    ZSucceeded(spPrivate->Terminate());

    // Release the event logger
    spPrivate = NULL;
    m_spEventLogger = NULL;
  }

  // Terminate AGC
  _AGCModule.Term();
}


////////////////////////////////////////////////////////////////////////
// InstallService - installs this service into the NT service manager
//
//

BOOL CServiceModule::InstallService(int argc, char * argv[])
{
    SC_HANDLE schMgr;
    SC_HANDLE schSvc;
    char szModule[MAX_PATH];
    char szPath[MAX_PATH + 16];
    char * szUserName;
    char * szPassword;


    schMgr = OpenSCManager(NULL,NULL,SC_MANAGER_CREATE_SERVICE);

    if (!schMgr)
    {
        printf("Unable to open SCManager.  Service not installed.\n");
        return FALSE;
    }

    // The service control manager starts the server with -service
    GetModuleFileName(NULL, szModule, sizeof(szModule));
    sprintf(szPath, "\"%s\" -service", szModule);

    if (argc > 3)
    {
        szUserName = argv[2];
        szPassword = argv[3];
    } else
    {
        szUserName = NULL;
        szPassword = NULL;
    }

    schSvc = CreateService(schMgr,
                           c_szSvcName,
                           "Allegiance Game Server",
                           SERVICE_ALL_ACCESS,
                           SERVICE_WIN32_OWN_PROCESS,
                           SERVICE_AUTO_START,
                           SERVICE_ERROR_NORMAL,
                           szPath,
                           NULL,
                           NULL,
                           NULL,
                           szUserName,
                           szPassword);

    if (!schSvc)
    {
      char szBuf[MAX_PATH];
      DWORD dwErrorCode(GetLastError());
      sprintf(szBuf, "Unable to create service [0x%08x].  Service not installed.\n", dwErrorCode);
      PrintSystemErrorMessage(szBuf, dwErrorCode);

      CloseServiceHandle(schMgr);
      return FALSE;
    }

    CloseServiceHandle(schSvc);
    CloseServiceHandle(schMgr);
    printf("%s service installed.\n", c_szSvcName);

    if (szUserName)
    {
        TCUserAccount acct;
        acct.Init(szUserName);  // example: szUserName == Redmond\federat which was passed in on the cmdline
        if (S_OK != acct.HasRight(SE_SERVICE_LOGON_NAME))
        {
          acct.SetRight(SE_SERVICE_LOGON_NAME);
          printf("The account %ls\\%ls has been granted the Logon As A Service right.", acct.GetDomainNameW(), acct.GetUserNameW());
        }
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////
// RemoveService - uninstalls this service from the NT service manager
//
//

BOOL CServiceModule::RemoveService(void)
{
    SC_HANDLE schMgr;
    SC_HANDLE schSvc;
    SERVICE_STATUS ss;

    schMgr = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    if (!schMgr)
    {
        printf("Unable to open SCManager.  Service not removed.\n");
        return FALSE;
    }

    schSvc = OpenService(schMgr, c_szSvcName, SERVICE_ALL_ACCESS);

    if (!schSvc)
    {
        printf("Unable to open %s service.  Service not removed.\n", c_szSvcName);
        CloseServiceHandle(schMgr);
        return FALSE;
    }

    QueryServiceStatus(schSvc,&ss);

    if (ss.dwCurrentState!=SERVICE_STOPPED)
    {
        printf("Unable to remove %s service while it is running.\n", c_szSvcName);
        CloseServiceHandle(schSvc);
        CloseServiceHandle(schMgr);
        return FALSE;
    }

    if (DeleteService(schSvc))
      printf("%s removed (as an NT Service).\n", c_szSvcName);
    else
    {
      char szBuf[MAX_PATH];
      sprintf(szBuf, "Unable to delete %s service.\n", c_szSvcName);

      DWORD dwErrorCode(GetLastError());
      PrintSystemErrorMessage(szBuf, dwErrorCode);
    }

    CloseServiceHandle(schSvc);
    CloseServiceHandle(schMgr);

    return TRUE;
}


// for when running as EXE
BOOL WINAPI HandlerRoutine(
  DWORD dwCtrlType   //  control signal type
)
{
    if (dwCtrlType == CTRL_CLOSE_EVENT)
    {
        SetEvent(g.hKillReceiveEvent);
        return TRUE;
    }

    return FALSE;
}

////////////////////////////////////////////////////////////////////////
// RunAsExecutable - for debugging, this service can be run from the
//                   command-line.
//
//

VOID CServiceModule::RunAsExecutable()
{
    HRESULT hr;

    ZVersionInfo vi;
    printf("%s\n%s\n\n",
      (LPCSTR)vi.GetFileDescription(), (LPCSTR)vi.GetLegalCopyright());

    if (IsWinNT())
      printf("Running as an executable.\n");

    printf("Initializing...\n");

    hr = FedSrv_Init();

    //
    // A training server (started by the client, without a console) just
    // runs until it is told to stop
    //
    if (SUCCEEDED(hr) && NA != g.civTraining)
    {
        WaitForSingleObject(g.hKillReceiveEvent, INFINITE);
        FedSrv_Terminate();
    }
    else if (SUCCEEDED(hr))
    {
        HANDLE hConsole;
                                  
        printf("\rType 'Q' to Quit.\n");
        printf("\rType 'P' to Pause.\n");
        printf("\rType 'C' to Continue.\n");

        hConsole = GetStdHandle(STD_INPUT_HANDLE);
        SetConsoleMode(hConsole,0);

        SetConsoleCtrlHandler(HandlerRoutine, true);

        assert(g.hKillReceiveEvent);

        HANDLE hEventArray[] = { g.hKillReceiveEvent, hConsole };

        DWORD dwAwaker;

        //
        // Wait until Receive Thread dies; if Q is pressed, kill Receive Thread
        //
        do
        {
            dwAwaker = MsgWaitForMultipleObjects(2, hEventArray, FALSE, INFINITE, 0);

            if (dwAwaker == WAIT_OBJECT_0 + 1)
            {
                INPUT_RECORD Key;
                DWORD dwRead;

                ReadConsoleInput(hConsole, &Key, 1, &dwRead);

                if (Key.EventType == KEY_EVENT && 
                    Key.Event.KeyEvent.wVirtualKeyCode == 'Q')
                {
                     debugf("Q pressed\n");
                     SetEvent(g.hKillReceiveEvent);
                }
                if (Key.EventType == KEY_EVENT && 
                    Key.Event.KeyEvent.wVirtualKeyCode == 'P')
                {
                     debugf("P pressed\n");
                     FedSrv_Pause();
                }
                if (Key.EventType == KEY_EVENT && 
                    Key.Event.KeyEvent.wVirtualKeyCode == 'C')
                {
                     debugf("C pressed\n");
                     FedSrv_Continue();
                }
            }

        } while (dwAwaker != WAIT_OBJECT_0);

        FedSrv_Terminate();
    } 
    else
    {
        printf("\rInitialization failed. (%x)\n", hr);
    }
}



////////////////////////////////////////////////////////////////////////
// SetServiceStatus - more useful version of this function than the NT
// one.
//
//

void CServiceModule::SetSvcStatus(
  DWORD state,
  DWORD exitcode
)
{
   if (g.ssh)
   {
       SERVICE_STATUS ss;
       static DWORD s_cp = 1;

       ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
       ss.dwCurrentState = state;
       ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
       ss.dwWin32ExitCode = exitcode;
       ss.dwServiceSpecificExitCode = 0;

       switch (state)
       {
           case SERVICE_RUNNING:
           case SERVICE_STOPPED:
           case SERVICE_PAUSED:
               ss.dwCheckPoint = s_cp++;
               ss.dwWaitHint = 1000;
               break;

         case SERVICE_START_PENDING:
               ss.dwCheckPoint = s_cp++;
               ss.dwWaitHint = 30000;       // we take about 30 seconds to start
            break;

         case SERVICE_STOP_PENDING:
               ss.dwCheckPoint = s_cp++;
               ss.dwWaitHint = 30000;       // we take about 30 seconds to shutdown
            break;

           default:
               ss.dwCheckPoint = 0;
               ss.dwWaitHint = 0;
               break;
       }

       SetServiceStatus(g.ssh,&ss);
   }
}
       
DWORD WINAPI CServiceModule::ServerTerminateThread(DWORD dwUnused)
{
    FedSrv_Terminate();
    SetSvcStatus(SERVICE_STOPPED,0);
    return 0;
}      

void WINAPI CServiceModule::StartServerTerminateThread()
{
    DWORD dwId;
    HANDLE hthrPending = CreateThread(NULL, 8192, (LPTHREAD_START_ROUTINE) ServerTerminateThread, 0, 0, &dwId);
    CloseHandle(hthrPending);
}
       
////////////////////////////////////////////////////////////////////////
// ServiceControl
//
// Service control handler which is registered with NT (using
// RegisterServiceCtrlHandler) as the callback for the service events.
// Right now the service does not support PAUSE/CONTINUE functionality,
// but this could be added here if required.
//
//
void WINAPI CServiceModule::ServiceControl(DWORD dwCode)
{
    switch (dwCode)
    {
        case SERVICE_CONTROL_STOP:
            // If it can take us a long time to stop our service as we wait for operations to complete,
            // the SCM doesn't like the ServiceControl() function to take a long time, so we spin up another
            // thread to terminate us.
            SetSvcStatus(SERVICE_STOP_PENDING, 0);
            StartServerTerminateThread();
            break;

        case SERVICE_CONTROL_PAUSE:
            FedSrv_Pause();
            SetSvcStatus(SERVICE_PAUSED, 0);
            
            break;

        case SERVICE_CONTROL_CONTINUE:
            FedSrv_Continue();
            SetSvcStatus(SERVICE_RUNNING, 0);
            break;

        case SERVICE_CONTROL_SHUTDOWN:
            // System is shutting down.  Memory and resources should *not*
            // be freed in this case due to limited time available for
            // shutdown.  Instead, only urgent non-volatile (i.e. disk)
            // structures should be flushed.
            break;

        case SERVICE_CONTROL_INTERROGATE:
            break;

    }

}




////////////////////////////////////////////////////////////////////////
// ServiceMain
//
// Pointer to this function is in the SERVICE_TABLE_ENTRY of the
// array passed to StartServiceCtrlDispatcher.

void WINAPI CServiceModule::ServiceMain(
  DWORD dwArgc,
  LPSTR *lpszArgv)
{
    HRESULT hr;

    g.ssh = RegisterServiceCtrlHandler(c_szSvcName, ServiceControl);

    if (!g.ssh)
    {
        SetSvcStatus(SERVICE_STOPPED,GetLastError());
        return;
    }

    SetSvcStatus(SERVICE_START_PENDING,NO_ERROR);

    hr = FedSrv_Init();

    if (SUCCEEDED(hr))
        SetSvcStatus(SERVICE_RUNNING,NO_ERROR);
    else
        SetSvcStatus(SERVICE_STOPPED,GetLastError());

}

void WINAPI _ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    _Module.ServiceMain(dwArgc, lpszArgv);
}


/*-------------------------------------------------------------------------
 * StopAllsrv
 *-------------------------------------------------------------------------
 * Purpose:
 *    cause allsrv to terminate 
 */

void CServiceModule::StopAllsrv()
{ 
  if (g.fService)
    _Module.ServiceControl(SERVICE_CONTROL_STOP);
  else
  {
    if(g.hKillReceiveEvent)
      SetEvent(g.hKillReceiveEvent);
//    if(m_hEventStopRunningAsEXE)
//        SetEvent(m_hEventStopRunningAsEXE);
  }
}
