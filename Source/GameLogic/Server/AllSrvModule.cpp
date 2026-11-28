/*-------------------------------------------------------------------------
 * fedsrv\AllSrvModule.CPP
 *
 * The game server as ServiceHost runs it (see allsrvmodule.h)
 *
 * Owner:
 *
 * Copyright 1986-1998 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/

#include "pch.h"

CServiceModule g_service;

extern Global g;


/*-------------------------------------------------------------------------
 * PrintSystemErrorMessage()
 *-------------------------------------------------------------------------
 * Purpose:
 *    Print szText and what dwErrorCode means, and log it as an event
 */
void PrintSystemErrorMessage(LPCTSTR szText, DWORD dwErrorCode)
{
  std::string strMessage = ServiceHost::SystemErrorText(dwErrorCode);
  printf("%s%s", szText, strMessage.c_str());

  g_events.TriggerEvent(NULL, AllsrvEventID_SystemError, "", -1, -1, -1, 2,
      "Text", VT_LPSTR, szText,
      "Message", VT_LPSTR, strMessage.c_str());
}


/*-------------------------------------------------------------------------
 * CServiceModule::InitEvents / TermEvents
 *-------------------------------------------------------------------------
 * Purpose:
 *    Start and stop the server event log (serverevents.h). This was where
 *    AGC and its event logger were set up.
 */
void CServiceModule::InitEvents()
{
  g_events.Init(g.fWantInt3);
}

void CServiceModule::TermEvents()
{
  g_events.Term();
}


/*-------------------------------------------------------------------------
 * CServiceModule::RunAsExecutable
 *-------------------------------------------------------------------------
 * Purpose:
 *    Run in this console until 'Q'; or, for the client's training mission
 *    (started without a console), until the client tells it to stop
 */
void CServiceModule::RunAsExecutable()
{
  ZVersionInfo vi;
  printf("%s\n%s\n\n", vi.GetFileDescription().c_str(), vi.GetLegalCopyright().c_str());

  if (NA != g.civTraining)
  {
    HRESULT hr = Start();
    if (SUCCEEDED(hr))
    {
      WaitForSingleObject(StopEvent(), INFINITE);
      Stop();
    }
    return;
  }

  printf("Running as an executable.\n");
  ServiceHost::RunInConsole(this);
}


/*-------------------------------------------------------------------------
 * CServiceModule::StopAllsrv
 *-------------------------------------------------------------------------
 * Purpose:
 *    cause allsrv to terminate
 */
void CServiceModule::StopAllsrv()
{
  ServiceHost::RequestStop(this);
}


/*-------------------------------------------------------------------------
 * IServiceApp
 *-------------------------------------------------------------------------
 */
HRESULT CServiceModule::Start()
{
  return FedSrv_Init();
}

void CServiceModule::Stop()
{
  FedSrv_Terminate();
}

void CServiceModule::Pause()
{
  FedSrv_Pause();
}

void CServiceModule::Continue()
{
  FedSrv_Continue();
}

HANDLE CServiceModule::StopEvent()
{
  return g.hKillReceiveEvent;
}
