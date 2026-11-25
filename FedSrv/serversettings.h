#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// The server's settings file (see zsettings.h and MODERNIZATION.md,
// "Settings: a JSON file replaces the registry"): server.json in
// %ProgramData%\Allegiance, or next to the executable. main() makes it the
// process's settings file before anything else runs; code that runs during
// static initialisation (InitSql) opens it by name instead. The AGC event
// logger keeps its configuration in the file's "EventLog" section.
//
//////////////////////////////////////////////////////////////////////////////

const char c_szServerSettingsFile[] = "server.json";

//
// The service name (not a setting, but it used to share regkey.h)
//

#if !defined(ALLSRV_STANDALONE)
  const char c_szSvcName[] = "AllSrv";
#else
  const char c_szSvcName[] = "AllSrv32";
#endif
