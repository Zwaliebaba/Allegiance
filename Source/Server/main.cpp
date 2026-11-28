//////////////////////////////////////////////////////////////////////////////
//
// The server executable, AllSrv32.exe: its entry point. The server itself is
// the GameLogic library.
//
//////////////////////////////////////////////////////////////////////////////

#include "GameLogic.h"

/*-------------------------------------------------------------------------
 * ShowUsage()
 *-------------------------------------------------------------------------
 */
static void ShowUsage()
{
  ZVersionInfo vi;
  printf("%s\n%s\n\n",
    vi.GetFileDescription().c_str(), vi.GetLegalCopyright().c_str());

  //      ----+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8
  printf("usage is ONE of:\n\n");
  printf("AllSrv32                           runs the server in this console\n");
  printf("AllSrv32 -install [user pw]        installs the server as a Windows service\n");
  printf("AllSrv32 -remove                   removes the Windows service\n");
  printf("\nSettings are read from %s.\n", c_szServerSettingsFile);
  printf("To start the service once installed: net start %s\n", c_szSvcName);
  //      ----+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8
}


/*-------------------------------------------------------------------------
 * ParseTrainingArgs()
 *-------------------------------------------------------------------------
 * Purpose:
 *    -training <civ> [-stopevent <handle>]: the client's training mission 7
 *    (see trainingserver.h). Returns false if the arguments are malformed.
 */
static bool ParseTrainingArgs(int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
  {
    if (!lstrcmpi("-training", argv[i]) && i + 1 < argc)
      g.civTraining = (short)atoi(argv[++i]);
    else if (!lstrcmpi("-stopevent", argv[i]) && i + 1 < argc)
      g.hTrainingStop = (HANDLE)(INT_PTR)_strtoui64(argv[++i], NULL, 10);
    else
      return false;
  }
  return NA != g.civTraining;
}


////////////////////////////////////////////////////////////////////////
// main
//
// This is the entry point to the process, whether run in a console, as a
// service, or by the client for its training mission:
//
//   AllSrv32                      run in this console ('Q' quits)
//   AllSrv32 -install [user pw]   install as a Windows service
//   AllSrv32 -remove              remove the Windows service
//   AllSrv32 -service             how the service control manager starts it
//   AllSrv32 -training <civ> -stopevent <handle>   (trainingserver.h)
//
// There is no COM any more: no registration, no COM classes, no AGC.
//
int __cdecl main(int argc, char *argv[])
{
    // The server's settings live in server.json, for all users (serversettings.h)
    ZSettings::Configure(c_szServerSettingsFile, ZSettings::AllUsers);

    // Load the global settings
    LoadSettings();

    // seed the random number generator with the current time
    // (GetTickCount may be semi-predictable on server startup, so we add the 
    // clock time to shake things up a bit)
    srand(GetTickCount() + time(NULL));

    g.hInst = GetModuleHandle(NULL);

    //
    // Service installation needs neither COM nor AGC
    //
    const char * szSwitch = (argc > 1 && (*argv[1]=='-' || *argv[1]=='/')) ? argv[1] + 1 : NULL;

    if (szSwitch && !lstrcmpi("install", szSwitch))
      return g_service.InstallService(argc, argv) ? 0 : 1;

    if (szSwitch && (!lstrcmpi("remove", szSwitch) || !lstrcmpi("uninstall", szSwitch)))
      return g_service.RemoveService() ? 0 : 1;


    bool fRun = false;
    if (argc == 1)
      fRun = true;
    else if (szSwitch && !lstrcmpi("service", szSwitch) && argc == 2)
      fRun = g.fService = true;
    else if (szSwitch && !lstrcmpi("training", szSwitch))
      fRun = ParseTrainingArgs(argc, argv);

    if (!fRun)
    {
      ShowUsage();
      return 1;
    }

    // Start the event log (serverevents.h)
    g_service.InitEvents();

    if (g.fService)
    {
      char szSvcName[64];
      strcpy(szSvcName, c_szSvcName);
      SERVICE_TABLE_ENTRY rgSTE[] =
      {
          { szSvcName, (LPSERVICE_MAIN_FUNCTION)_ServiceMain },
          { NULL, NULL }
      };

      if (!StartServiceCtrlDispatcher(rgSTE))
      {
        PrintSystemErrorMessage("-service is for the service control manager; use -install to install the service.\n",
          GetLastError());
      }
    }
    else
    {
      g_service.RunAsExecutable();
    }

    // Stop the event log
    g_service.TermEvents();

    // Indicate success
    return 0;
}
