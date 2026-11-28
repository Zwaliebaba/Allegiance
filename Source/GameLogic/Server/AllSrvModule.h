/*-------------------------------------------------------------------------
 * fedsrv\AllSrvModule.h
 *
 * The game server as NeuronServer's ServiceHost runs it, as a console
 * program or as a Windows service (ServiceHost.h), and the training server
 * the client starts.
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

#include "ServiceHost.h"

class CServiceModule : public IServiceApp
{
public:
  void         InitEvents();        // the server event log (ServerEvents.h)
  void         TermEvents();

  void         RunAsExecutable();   // in this console, or as the client's training server
  void         StopAllsrv();        // from inside the server

// IServiceApp
  HRESULT      Start() override;
  void         Stop() override;
  void         Pause() override;
  void         Continue() override;
  HANDLE       StopEvent() override;
};

extern CServiceModule g_service;

void PrintSystemErrorMessage(LPCTSTR szBuf, DWORD dwErrorCode);

#endif
