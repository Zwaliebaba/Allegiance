//////////////////////////////////////////////////////////////////////////////
//
// ServiceHost: how a server process runs, as a console program or as a
// Windows service, and installs and removes itself as one. The server is an
// IServiceApp; this is the Windows side of starting, pausing and stopping
// it. (It was the server's CServiceModule, minus the game.)
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <string>

//
// The server, as ServiceHost drives it
//

class IServiceApp
{
public:
  virtual HRESULT Start() = 0;            // returns once the server runs; it works on its own threads
  virtual void    Stop() = 0;             // returns once it has stopped
  virtual void    Pause() = 0;
  virtual void    Continue() = 0;
  virtual HANDLE  StopEvent() = 0;        // set when the server should stop (or has decided to)
};

//
// The command line's service switch: AllSrv32 [-install [user password] | -remove | -service | ...]
// A switch starts with '-' or '/', in any case; "-uninstall" is "-remove".
//

enum class ServiceCommand
{
  Console,      // no arguments: run in this console
  Install,      // -install [user password]
  Remove,       // -remove, -uninstall
  Service,      // -service, alone: the service control manager started us
  Other         // anything else: for the program to parse (or print its usage)
};

struct ServiceArgs
{
  ServiceCommand m_command;
  const char *   m_szUser;        // -install's account, or NULL for LocalSystem
  const char *   m_szPassword;
};

ServiceArgs ParseServiceArgs(int argc, char * argv[]);

namespace ServiceHost
{
  //
  // Install the program as an auto-start service that the service control
  // manager runs with -service, optionally as szUser (DOMAIN\name or .\name),
  // who is then granted the right to log on as a service. Prints what it did.
  //
  bool Install(const char * szService, const char * szDisplayName, const char * szUser, const char * szPassword);
  bool Remove(const char * szService);

  //
  // Run as the service szService until it is stopped. Fails at once (false,
  // with the reason printed) unless the service control manager started us.
  //
  bool RunAsService(const char * szService, IServiceApp * papp);
  bool IsService();     // whether RunAsService is what runs

  //
  // Run in this console: 'Q' quits, 'P' pauses, 'C' continues, and closing
  // the console quits. Returns Start's result.
  //
  HRESULT RunInConsole(IServiceApp * papp);

  //
  // Ask the running server to stop, from inside it: as a service, the way
  // the service control manager's stop does; in a console, its StopEvent
  //
  void RequestStop(IServiceApp * papp);

  //
  // What dwError means, and printing it after szText
  //
  std::string SystemErrorText(DWORD dwError);
  void        PrintSystemError(const char * szText, DWORD dwError);
}
