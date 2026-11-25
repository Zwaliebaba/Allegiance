/*-------------------------------------------------------------------------
  zauth.cpp

  IZoneAuthClient / IZoneAuthServer without the Zone.

  The original implementation sat on the MSN Gaming Zone authentication SDK
  (zoneauth.h, zoneauthdecrypt.h), which is not part of the source release and
  whose service no longer exists. The only callers are the Zone club screens,
  which Phase 2 of MODERNIZATION.md removes. Until then this implementation
  keeps the interface and reports Zone authentication as unavailable:
  every authentication attempt fails, and no ticket is ever produced or accepted.

  The original code is in git history (initial import).
 *-----------------------------------------------------------------------*/

#include "pch.h"
#include "zauth.h"

class CZoneAuthServer : public IZoneAuthServer
{
public:
  HRESULT DecryptTicket(LPBYTE pbTicket, DWORD cbTicket) { return E_FAIL; }
  bool    HasToken(LPSTR szToken, bool * pfValidNow)
  {
    if (pfValidNow)
      *pfValidNow = false;
    return false;
  }
  LPCSTR  GetName()       { return ""; }
  LPCSTR  GetAuthServer() { return ""; }
  unsigned long GetAccountID() { return 0; }
};

class CZoneAuthClient : public IZoneAuthClient
{
public:
  HRESULT  IsAuthenticated(DWORD msTimeout) { return E_FAIL; }
  HRESULT  Authenticate(LPSTR szName, LPSTR szPW, bool fPWChanged, BOOL fRememberPW, DWORD msTimeout) { return E_FAIL; }
  void     GetDefaultLogonInfo(OUT char * szName, OUT char * szPW, OUT BOOL * pfRememberPW)
  {
    if (szName)       *szName = '\0';
    if (szPW)         *szPW   = '\0';
    if (pfRememberPW) *pfRememberPW = FALSE;
  }
  HRESULT  GetTicket(OUT LPBYTE * ppBuffer, OUT DWORD * pcbBuffer, OUT LPSTR szLogin, IN OUT DWORD * pcbLogin)
  {
    if (ppBuffer)  *ppBuffer  = NULL;
    if (pcbBuffer) *pcbBuffer = 0;
    return E_FAIL;
  }
  ZString & GetAuthServer()                    { return m_strAuthServer; }
  void     SetAuthServer(ZString strAuthServer) { m_strAuthServer = strAuthServer; }
  bool     HasInterface(REFIID iid)            { return false; }

private:
  ZString m_strAuthServer;
};

TRef<IZoneAuthClient> CreateZoneAuthClient()
{
  return new CZoneAuthClient();
}

TRef<IZoneAuthServer> CreateZoneAuthServer()
{
  return new CZoneAuthServer();
}
