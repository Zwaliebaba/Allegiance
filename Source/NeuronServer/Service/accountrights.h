//////////////////////////////////////////////////////////////////////////////
//
// Account rights (LSA): what -install needs to let the account it names run
// the service. Plain Win32; it replaces TCAtl's TCUserAccount.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <string>
#include <vector>

//
// An account's SID and its DOMAIN\name, from a name as LookupAccountName
// takes it (name, DOMAIN\name, .\name for this computer, name@domain)
//

struct AccountInfo
{
  std::vector<BYTE> m_sid;
  std::string       m_strName;      // DOMAIN\name
};

HRESULT LookupAccount(const char * szAccount, AccountInfo & info);

//
// Whether the account holds a right (e.g. L"SeServiceLogonRight"), and
// granting it one. Both need to be an administrator.
//

HRESULT HasAccountRight(const AccountInfo & info, const wchar_t * szRight, bool & fHas);
HRESULT AddAccountRight(const AccountInfo & info, const wchar_t * szRight);
