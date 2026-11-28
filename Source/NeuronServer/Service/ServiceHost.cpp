//////////////////////////////////////////////////////////////////////////////
//
// ServiceHost (ServiceHost.h)
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "ServiceHost.h"
#include "AccountRights.h"

//////////////////////////////////////////////////////////////////////////////
//
// The command line
//
//////////////////////////////////////////////////////////////////////////////

ServiceArgs ParseServiceArgs(int argc, char * argv[])
{
  ServiceArgs args = { ServiceCommand::Other, nullptr, nullptr };

  if (argc <= 1)
  {
    args.m_command = ServiceCommand::Console;
    return args;
  }

  const char * szSwitch = argv[1];
  if (*szSwitch != '-' && *szSwitch != '/')
    return args;
  szSwitch++;

  if (!lstrcmpiA(szSwitch, "install"))
  {
    // An account needs its password; an account alone is a mistake, not LocalSystem
    if (argc == 2)
      args.m_command = ServiceCommand::Install;
    else if (argc == 4)
    {
      args.m_command    = ServiceCommand::Install;
      args.m_szUser     = argv[2];
      args.m_szPassword = argv[3];
    }
  }
  else if (!lstrcmpiA(szSwitch, "remove") || !lstrcmpiA(szSwitch, "uninstall"))
  {
    if (argc == 2)
      args.m_command = ServiceCommand::Remove;
  }
  else if (!lstrcmpiA(szSwitch, "service"))
  {
    if (argc == 2)
      args.m_command = ServiceCommand::Service;
  }

  return args;
}

namespace ServiceHost
{

//////////////////////////////////////////////////////////////////////////////
//
// Errors
//
//////////////////////////////////////////////////////////////////////////////

std::string SystemErrorText(DWORD dwError)
{
  char * szMessage = nullptr;
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&szMessage, 0, nullptr);

  std::string str;
  if (szMessage)
    str = szMessage;
  else
  {
    char sz[32];
    sprintf_s(sz, "Error %lu.\r\n", dwError);
    str = sz;
  }
  LocalFree(szMessage);
  return str;
}

void PrintSystemError(const char * szText, DWORD dwError)
{
  printf("%s%s", szText, SystemErrorText(dwError).c_str());
}

//////////////////////////////////////////////////////////////////////////////
//
// Installing and removing
//
//////////////////////////////////////////////////////////////////////////////

bool Install(const char * szService, const char * szDisplayName, const char * szUser, const char * szPassword)
{
  SC_HANDLE schMgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
  if (!schMgr)
  {
    PrintSystemError("Unable to open the service control manager. Service not installed.\n", GetLastError());
    return false;
  }

  // The service control manager starts the program with -service
  char szModule[MAX_PATH];
  char szPath[MAX_PATH + 16];
  GetModuleFileNameA(nullptr, szModule, sizeof(szModule));
  sprintf_s(szPath, "\"%s\" -service", szModule);

  SC_HANDLE schSvc = CreateServiceA(schMgr, szService, szDisplayName, SERVICE_ALL_ACCESS,
                                    SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                    szPath, nullptr, nullptr, nullptr, szUser, szPassword);
  if (!schSvc)
  {
    PrintSystemError("Unable to create the service. Service not installed.\n", GetLastError());
    CloseServiceHandle(schMgr);
    return false;
  }

  CloseServiceHandle(schSvc);
  CloseServiceHandle(schMgr);
  printf("%s service installed.\n", szService);

  // The account the service runs as needs the right to log on as a service
  if (szUser)
  {
    AccountInfo info;
    bool        fHas = false;
    HRESULT     hr   = LookupAccount(szUser, info);

    if (SUCCEEDED(hr))
      hr = HasAccountRight(info, L"SeServiceLogonRight", fHas);
    if (SUCCEEDED(hr) && !fHas)
    {
      hr = AddAccountRight(info, L"SeServiceLogonRight");
      if (SUCCEEDED(hr))
        printf("The account %s has been granted the Log On As A Service right.\n", info.m_strName.c_str());
    }
    if (FAILED(hr))
      PrintSystemError("Unable to give the account the Log On As A Service right; grant it in Local Security Policy.\n",
                       HRESULT_CODE(hr));
  }

  return true;
}

bool Remove(const char * szService)
{
  SC_HANDLE schMgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
  if (!schMgr)
  {
    PrintSystemError("Unable to open the service control manager. Service not removed.\n", GetLastError());
    return false;
  }

  SC_HANDLE schSvc = OpenServiceA(schMgr, szService, SERVICE_ALL_ACCESS);
  if (!schSvc)
  {
    printf("Unable to open the %s service. Service not removed.\n", szService);
    CloseServiceHandle(schMgr);
    return false;
  }

  bool           fRemoved = false;
  SERVICE_STATUS ss       = {};
  QueryServiceStatus(schSvc, &ss);

  if (ss.dwCurrentState != SERVICE_STOPPED)
    printf("Unable to remove the %s service while it is running.\n", szService);
  else if (DeleteService(schSvc))
  {
    printf("%s service removed.\n", szService);
    fRemoved = true;
  }
  else
  {
    char szText[256];
    sprintf_s(szText, "Unable to delete the %s service.\n", szService);
    PrintSystemError(szText, GetLastError());
  }

  CloseServiceHandle(schSvc);
  CloseServiceHandle(schMgr);
  return fRemoved;
}

//////////////////////////////////////////////////////////////////////////////
//
// Running as a service
//
//////////////////////////////////////////////////////////////////////////////

static char                  s_szService[256];
static IServiceApp *         s_papp     = nullptr;
static SERVICE_STATUS_HANDLE s_ssh      = nullptr;
static bool                  s_fService = false;

static void SetStatus(DWORD state, DWORD dwExitCode)
{
  if (!s_ssh)
    return;

  static DWORD s_dwCheckPoint = 1;

  SERVICE_STATUS ss = {};
  ss.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
  ss.dwCurrentState     = state;
  ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
  ss.dwWin32ExitCode    = dwExitCode;

  switch (state)
  {
    case SERVICE_RUNNING:
    case SERVICE_STOPPED:
    case SERVICE_PAUSED:
      ss.dwCheckPoint = s_dwCheckPoint++;
      ss.dwWaitHint   = 1000;
      break;

    case SERVICE_START_PENDING:     // the server takes about 30 seconds to start or stop
    case SERVICE_STOP_PENDING:
      ss.dwCheckPoint = s_dwCheckPoint++;
      ss.dwWaitHint   = 30000;
      break;
  }

  SetServiceStatus(s_ssh, &ss);
}

static DWORD WINAPI StopThread(void *)
{
  s_papp->Stop();
  SetStatus(SERVICE_STOPPED, NO_ERROR);
  return 0;
}

static void WINAPI ServiceControl(DWORD dwCode)
{
  switch (dwCode)
  {
    case SERVICE_CONTROL_STOP:
      {
        // Stopping takes a while, and the service control manager wants an
        // answer at once, so another thread stops the server
        SetStatus(SERVICE_STOP_PENDING, NO_ERROR);
        HANDLE hThread = CreateThread(nullptr, 0, StopThread, nullptr, 0, nullptr);
        if (hThread)
          CloseHandle(hThread);
      }
      break;

    case SERVICE_CONTROL_PAUSE:
      s_papp->Pause();
      SetStatus(SERVICE_PAUSED, NO_ERROR);
      break;

    case SERVICE_CONTROL_CONTINUE:
      s_papp->Continue();
      SetStatus(SERVICE_RUNNING, NO_ERROR);
      break;
  }
}

static void WINAPI ServiceMain(DWORD, LPSTR *)
{
  s_ssh = RegisterServiceCtrlHandlerA(s_szService, ServiceControl);
  if (!s_ssh)
    return;

  SetStatus(SERVICE_START_PENDING, NO_ERROR);

  // The server runs on its own threads once started; the dispatcher returns
  // when it reports SERVICE_STOPPED
  HRESULT hr = s_papp->Start();
  if (SUCCEEDED(hr))
    SetStatus(SERVICE_RUNNING, NO_ERROR);
  else
    SetStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
}

bool RunAsService(const char * szService, IServiceApp * papp)
{
  lstrcpynA(s_szService, szService, sizeof(s_szService));
  s_papp     = papp;
  s_fService = true;

  SERVICE_TABLE_ENTRYA rgSTE[] =
  {
    { s_szService, ServiceMain },
    { nullptr, nullptr }
  };

  if (!StartServiceCtrlDispatcherA(rgSTE))
  {
    s_fService = false;
    PrintSystemError("-service is for the service control manager; use -install to install the service.\n",
                     GetLastError());
    return false;
  }
  return true;
}

bool IsService()
{
  return s_fService;
}

//////////////////////////////////////////////////////////////////////////////
//
// Running in a console
//
//////////////////////////////////////////////////////////////////////////////

static HANDLE s_hConsoleStop = nullptr;

static BOOL WINAPI ConsoleCtrl(DWORD dwCtrlType)
{
  if (dwCtrlType == CTRL_CLOSE_EVENT && s_hConsoleStop)
  {
    SetEvent(s_hConsoleStop);
    return TRUE;
  }
  return FALSE;
}

HRESULT RunInConsole(IServiceApp * papp)
{
  printf("Initializing...\n");

  HRESULT hr = papp->Start();
  if (FAILED(hr))
  {
    printf("\rInitialization failed. (%x)\n", hr);
    return hr;
  }

  printf("\rType 'Q' to Quit.\n");
  printf("\rType 'P' to Pause.\n");
  printf("\rType 'C' to Continue.\n");

  HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(hConsole, 0);

  s_hConsoleStop = papp->StopEvent();
  SetConsoleCtrlHandler(ConsoleCtrl, TRUE);

  HANDLE rgh[] = { s_hConsoleStop, hConsole };
  DWORD  dwWoken;

  // Until the server stops; 'Q' tells it to
  do
  {
    dwWoken = MsgWaitForMultipleObjects(2, rgh, FALSE, INFINITE, 0);

    if (dwWoken == WAIT_OBJECT_0 + 1)
    {
      INPUT_RECORD key;
      DWORD        cRead = 0;
      ReadConsoleInputA(hConsole, &key, 1, &cRead);

      if (cRead && key.EventType == KEY_EVENT)
      {
        switch (key.Event.KeyEvent.wVirtualKeyCode)
        {
          case 'Q': debugf("Q pressed\n"); SetEvent(s_hConsoleStop); break;
          case 'P': debugf("P pressed\n"); papp->Pause();            break;
          case 'C': debugf("C pressed\n"); papp->Continue();         break;
        }
      }
    }
  } while (dwWoken != WAIT_OBJECT_0);

  papp->Stop();
  SetConsoleCtrlHandler(ConsoleCtrl, FALSE);
  s_hConsoleStop = nullptr;
  return hr;
}

void RequestStop(IServiceApp * papp)
{
  if (s_fService)
    ServiceControl(SERVICE_CONTROL_STOP);
  else if (papp->StopEvent())
    SetEvent(papp->StopEvent());
}

}   // namespace ServiceHost
