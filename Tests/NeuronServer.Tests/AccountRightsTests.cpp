#include "pch.h"
#include "AccountRights.h"

#include <sddl.h>

//////////////////////////////////////////////////////////////////////////////
//
// Account rights (Service/AccountRights.h), which -install uses to let the
// service's account log on as a service. Only the reading side is tested:
// granting a right changes the machine's security policy. Account names are
// localised, so the tests start from well-known SIDs, not names.
//
//////////////////////////////////////////////////////////////////////////////

namespace AccountRightsTests
{
    static std::vector<BYTE> WellKnownSid(WELL_KNOWN_SID_TYPE type)
    {
        DWORD             cb = SECURITY_MAX_SID_SIZE;
        std::vector<BYTE> sid(cb);
        Assert::IsTrue(!!CreateWellKnownSid(type, NULL, sid.data(), &cb));
        sid.resize(cb);
        return sid;
    }

    static std::string NameOf(const std::vector<BYTE> & sid)
    {
        char         szName[256];
        char         szDomain[256];
        DWORD        cchName   = sizeof(szName);
        DWORD        cchDomain = sizeof(szDomain);
        SID_NAME_USE use;
        Assert::IsTrue(!!LookupAccountSidA(NULL, (PSID)sid.data(), szName, &cchName, szDomain, &cchDomain, &use));
        return *szDomain ? std::string(szDomain) + "\\" + szName : std::string(szName);
    }

    TEST_CLASS(Accounts)
    {
    public:
        TEST_METHOD(LooksUpAnAccountByName)
        {
            std::vector<BYTE> sid    = WellKnownSid(WinBuiltinAdministratorsSid);
            std::string       strName = NameOf(sid);

            AccountInfo info;
            Assert::AreEqual(S_OK, LookupAccount(strName.c_str(), info));
            Assert::IsTrue(!!EqualSid((PSID)info.m_sid.data(), (PSID)sid.data()));
            AreEqualStr(strName.c_str(), info.m_strName);
        }

        TEST_METHOD(DotIsThisComputer)
        {
            // The current user, if it is an account on this computer (not a domain's)
            HANDLE hToken = NULL;
            Assert::IsTrue(!!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken));
            BYTE  rgb[256];
            DWORD cb = 0;
            Assert::IsTrue(!!GetTokenInformation(hToken, TokenUser, rgb, sizeof(rgb), &cb));
            CloseHandle(hToken);

            PSID              psid = ((TOKEN_USER *)rgb)->User.Sid;
            std::vector<BYTE> sid((BYTE *)psid, (BYTE *)psid + GetLengthSid(psid));
            std::string       strName = NameOf(sid);

            char  szComputer[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD cch = sizeof(szComputer);
            Assert::IsTrue(!!GetComputerNameA(szComputer, &cch));
            std::string strPrefix = std::string(szComputer) + "\\";
            if (_strnicmp(strName.c_str(), strPrefix.c_str(), strPrefix.size()))
            {
                Logger::WriteMessage("The current user is a domain account; nothing to test");
                return;
            }

            AccountInfo info;
            Assert::AreEqual(S_OK, LookupAccount((".\\" + strName.substr(strPrefix.size())).c_str(), info));
            Assert::IsTrue(!!EqualSid((PSID)info.m_sid.data(), psid));
        }

        TEST_METHOD(AnUnknownAccountIsNotFound)
        {
            AccountInfo info;
            Assert::AreEqual(HRESULT_FROM_WIN32(ERROR_NONE_MAPPED), LookupAccount("no-such-account-7e57c0de", info));
        }
    };

    TEST_CLASS(Rights)
    {
        static bool Skip(HRESULT hr)
        {
            // Reading the policy needs an administrator
            if (hr == E_ACCESSDENIED)
            {
                Logger::WriteMessage("Not an administrator; nothing to test");
                return true;
            }
            return false;
        }

    public:
        TEST_METHOD(ARightNobodyHolds)
        {
            AccountInfo info;
            info.m_sid = WellKnownSid(WinBuiltinAdministratorsSid);

            bool    fHas = true;
            HRESULT hr   = HasAccountRight(info, L"SeNoSuchRight7e57", fHas);
            if (Skip(hr))
                return;
            Assert::AreEqual(S_OK, hr);
            Assert::IsFalse(fHas);
        }

        TEST_METHOD(AnAccountWithNoRightsAtAll)
        {
            // A SID no account has: the policy has no entry for it
            PSID psid = NULL;
            Assert::IsTrue(!!ConvertStringSidToSidA("S-1-5-21-1-2-3-4242", &psid));
            AccountInfo info;
            info.m_sid.assign((BYTE *)psid, (BYTE *)psid + GetLengthSid(psid));
            LocalFree(psid);

            bool    fHas = true;
            HRESULT hr   = HasAccountRight(info, L"SeServiceLogonRight", fHas);
            if (Skip(hr))
                return;
            Assert::AreEqual(S_OK, hr);
            Assert::IsFalse(fHas);
        }
    };
}
