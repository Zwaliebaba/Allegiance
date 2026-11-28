#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// The server's settings file (see ZSettings.h and MODERNIZATION.md,
// "Settings: a JSON file replaces the registry"): server.json in
// %ProgramData%\Allegiance, or next to the executable. main() makes it the
// process's settings file before anything else runs; code that runs during
// static initialisation (InitSql) opens it by name instead. The AGC event
// logger keeps its configuration in the file's "EventLog" section.
//
//////////////////////////////////////////////////////////////////////////////

const char c_szServerSettingsFile[] = "server.json";

//
// The named mutex that says a server is running on this machine (it used to
// come from AllSrvModuleIDL.idl)
//

#define szAllSrvRunning TEXT("{5B5ED2A6-E7C0-11D2-8B4B-00C04F681633}_Running")
#define szAllSrvRunningGlobal (TEXT("Global\\") szAllSrvRunning)

//
// The service name (not a setting, but it used to share regkey.h)
//

  const char c_szSvcName[] = "AllSrv32";
