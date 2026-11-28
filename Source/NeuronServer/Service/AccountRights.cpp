//////////////////////////////////////////////////////////////////////////////
//
// Account rights (AccountRights.h)
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "AccountRights.h"

#include <ntsecapi.h>

HRESULT LookupAccount(const char * szAccount, AccountInfo & info)
{
  std::string strAccount = szAccount ? szAccount : "";

  // .\name is an account on this computer, as the service control manager has it
  if (strAccount.compare(0, 2, ".\\") == 0)
  {
    char  szComputer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD cch = sizeof(szComputer);
    if (!GetComputerNameA(szComputer, &cch))
      return HRESULT_FROM_WIN32(GetLastError());
    strAccount = szComputer + strAccount.substr(1);
  }

  DWORD        cbSid     = 0;
  DWORD        cchDomain = 0;
  SID_NAME_USE use;

  if (!LookupAccountNameA(nullptr, strAccount.c_str(), nullptr, &cbSid, nullptr, &cchDomain, &use))
  {
    DWORD dwError = GetLastError();
    if (dwError != ERROR_INSUFFICIENT_BUFFER)
      return HRESULT_FROM_WIN32(dwError);
  }

  info.m_sid.resize(cbSid);
  std::string strDomain(cchDomain, '\0');
  if (!LookupAccountNameA(nullptr, strAccount.c_str(), info.m_sid.data(), &cbSid, &strDomain[0], &cchDomain, &use))
    return HRESULT_FROM_WIN32(GetLastError());

  // DOMAIN\name, spelt as Windows has it
  char  szName[256];
  char  szDomain[256];
  DWORD cchName = sizeof(szName);
  cchDomain = sizeof(szDomain);
  if (LookupAccountSidA(nullptr, info.m_sid.data(), szName, &cchName, szDomain, &cchDomain, &use))
    info.m_strName = *szDomain ? std::string(szDomain) + "\\" + szName : std::string(szName);
  else
    info.m_strName = strAccount;

  return S_OK;
}

static HRESULT OpenPolicy(ACCESS_MASK access, LSA_HANDLE & hPolicy)
{
  LSA_OBJECT_ATTRIBUTES oa = {};
  NTSTATUS status = LsaOpenPolicy(nullptr, &oa, access, &hPolicy);
  return status ? HRESULT_FROM_WIN32(LsaNtStatusToWinError(status)) : S_OK;
}

HRESULT HasAccountRight(const AccountInfo & info, const wchar_t * szRight, bool & fHas)
{
  fHas = false;

  LSA_HANDLE hPolicy = nullptr;
  HRESULT    hr      = OpenPolicy(POLICY_LOOKUP_NAMES, hPolicy);
  if (FAILED(hr))
    return hr;

  PLSA_UNICODE_STRING prights = nullptr;
  ULONG               cRights = 0;
  NTSTATUS            status  = LsaEnumerateAccountRights(hPolicy, (PSID)info.m_sid.data(), &prights, &cRights);
  DWORD               dwError = LsaNtStatusToWinError(status);

  if (status == 0)
  {
    size_t cch = wcslen(szRight);
    for (ULONG i = 0; i < cRights && !fHas; i++)
      fHas = prights[i].Length == cch * sizeof(wchar_t) && !_wcsnicmp(prights[i].Buffer, szRight, cch);
    LsaFreeMemory(prights);
  }
  else if (dwError == ERROR_FILE_NOT_FOUND)   // the account holds no rights at all
    status = 0;

  LsaClose(hPolicy);
  return status ? HRESULT_FROM_WIN32(dwError) : S_OK;
}

HRESULT AddAccountRight(const AccountInfo & info, const wchar_t * szRight)
{
  LSA_HANDLE hPolicy = nullptr;
  HRESULT    hr      = OpenPolicy(POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, hPolicy);
  if (FAILED(hr))
    return hr;

  USHORT             cb    = (USHORT)(wcslen(szRight) * sizeof(wchar_t));
  LSA_UNICODE_STRING right = { cb, (USHORT)(cb + sizeof(wchar_t)), const_cast<PWSTR>(szRight) };
  NTSTATUS           status = LsaAddAccountRights(hPolicy, (PSID)info.m_sid.data(), &right, 1);

  LsaClose(hPolicy);
  return status ? HRESULT_FROM_WIN32(LsaNtStatusToWinError(status)) : S_OK;
}
