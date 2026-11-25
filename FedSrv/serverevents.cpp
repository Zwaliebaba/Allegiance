//////////////////////////////////////////////////////////////////////////////
//
// Server events (see serverevents.h)
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "serverevents.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CServerEvents g_events;

namespace
{
    const ServerEventDef s_rgDefs[] =
    {
        SERVER_EVENT_DEFS
    };

    const char* SeverityName(EventSeverity severity)
    {
        switch (severity)
        {
            case EventSeverity_Warning: return "Warning";
            case EventSeverity_Error:   return "Error";
            default:                    return "Info";
        }
    }

    std::string AnsiToUtf8(const std::string& str)
    {
        if (str.empty())
            return str;

        int cwch = MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), NULL, 0);
        if (cwch <= 0)
            return std::string();

        std::wstring wstr(cwch, L'\0');
        MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), &wstr[0], cwch);

        int cch = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), cwch, NULL, 0, NULL, NULL);
        std::string result(cch > 0 ? cch : 0, '\0');
        if (cch > 0)
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), cwch, &result[0], cch, NULL, NULL);
        return result;
    }

    std::string TimeString(const SYSTEMTIME& st)
    {
        char sz[32];
        sprintf_s(sz, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return sz;
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Construction
//
//////////////////////////////////////////////////////////////////////////////

CServerEvents::CServerEvents() :
    m_fLogAll(false),
    m_fBreakOnErrors(false),
    m_hfile(INVALID_HANDLE_VALUE),
    m_dwDayOpen(0)
{
    InitializeCriticalSection(&m_cs);

    char  szName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD cchName = sizeof(szName);
    if (GetComputerNameA(szName, &cchName))
        m_strComputerName = szName;
}

CServerEvents::~CServerEvents()
{
    Term();
    DeleteCriticalSection(&m_cs);
}

void CServerEvents::Init(bool fBreakOnErrors)
{
    EnterCriticalSection(&m_cs);
    m_fBreakOnErrors = fBreakOnErrors;

    // LogAllEvents = 1: log every event, chat, kills and logins included
    m_fLogAll = ZSettings::GetDWORD("LogAllEvents", 0) != 0;
    LeaveCriticalSection(&m_cs);
}

void CServerEvents::Term()
{
    EnterCriticalSection(&m_cs);
    if (m_hfile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hfile);
        m_hfile = INVALID_HANDLE_VALUE;
    }
    m_dwDayOpen = 0;
    LeaveCriticalSection(&m_cs);
}


//////////////////////////////////////////////////////////////////////////////
//
// Which events
//
//////////////////////////////////////////////////////////////////////////////

const ServerEventDef* CServerEvents::Find(AGCEventID idEvent) const
{
    for (size_t i = 0; i < sizeof(s_rgDefs) / sizeof(s_rgDefs[0]); i++)
    {
        if (s_rgDefs[i].m_id == idEvent)
            return &s_rgDefs[i];
    }
    return NULL;
}

bool CServerEvents::IsWanted(const ServerEventDef* pdef) const
{
    // An unknown event is logged, so that it gets noticed
    return !pdef || m_fLogAll || pdef->m_fLogByDefault || pdef->m_severity == EventSeverity_Error;
}

HAGCLISTENERS CServerEvents::EventListeners(AGCEventID idEvent, AGCUniqueID, AGCUniqueID, AGCUniqueID)
{
    return IsWanted(Find(idEvent)) ? this : NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// Argument values
//
//////////////////////////////////////////////////////////////////////////////

std::string CServerEvents::ToString(VARTYPE, const char* sz)
{
    return sz ? std::string(sz) : std::string();
}

std::string CServerEvents::ToString(VARTYPE, const wchar_t* wsz)
{
    if (!wsz || !*wsz)
        return std::string();

    int cch = WideCharToMultiByte(CP_ACP, 0, wsz, -1, NULL, 0, NULL, NULL);
    if (cch <= 1)
        return std::string();

    std::string result(cch - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, wsz, -1, &result[0], cch, NULL, NULL);
    return result;
}

std::string CServerEvents::ToString(VARTYPE, double d)
{
    char sz[64];
    sprintf_s(sz, "%g", d);
    return sz;
}

std::string CServerEvents::ToString(VARTYPE vt, long long n, bool fSigned)
{
    if (VT_BOOL == vt)
        return n ? "True" : "False";

    char sz[32];
    if (fSigned)
        sprintf_s(sz, "%lld", n);
    else
        sprintf_s(sz, "%llu", (unsigned long long)n);
    return sz;
}


//////////////////////////////////////////////////////////////////////////////
//
// Formatting and logging
//
//////////////////////////////////////////////////////////////////////////////

std::string CServerEvents::Expand(AGCEventID idEvent, const ServerEventDef* pdef, const char* szContext,
    const char* szSubject, AGCUniqueID idSubject, const std::vector<Arg>& vargs, const SYSTEMTIME& st) const
{
    const char* szFormat = (pdef && pdef->m_szFormat && *pdef->m_szFormat) ? pdef->m_szFormat : NULL;

    //
    // No format: list the arguments
    //

    if (!szFormat)
    {
        std::string str;
        if (szSubject && *szSubject)
            str = szSubject;
        for (size_t i = 0; i < vargs.size(); i++)
        {
            if (!str.empty())
                str += ", ";
            str += vargs[i].m_szName;
            str += " = ";
            str += vargs[i].m_strValue;
        }
        return str;
    }

    //
    // %Name% is an argument or one of the event's own fields; %% is a '%'.
    // A name that matches nothing is written as it is, as AGC did.
    //

    std::string str;
    for (const char* pch = szFormat; *pch; )
    {
        if (*pch != '%')
        {
            str += *pch++;
            continue;
        }

        const char* pchEnd = strchr(pch + 1, '%');
        if (!pchEnd)
        {
            str += pch;
            break;
        }

        if (pchEnd == pch + 1)
        {
            str += '%';
            pch = pchEnd + 1;
            continue;
        }

        std::string strName(pch + 1, pchEnd);
        const char* szName = strName.c_str();
        bool        fFound = true;
        char        sz[64];

        if (!_stricmp(szName, ".SubjectName"))
            str += szSubject ? szSubject : "";
        else if (!_stricmp(szName, ".SubjectID"))
            { sprintf_s(sz, "%ld", idSubject); str += sz; }
        else if (!_stricmp(szName, ".ComputerName"))
            str += m_strComputerName;
        else if (!_stricmp(szName, ".Context"))
            str += szContext ? szContext : "";
        else if (!_stricmp(szName, ".ID"))
            { sprintf_s(sz, "%ld", idEvent); str += sz; }
        else if (!_stricmp(szName, ".Time"))
            str += TimeString(st);
        else
        {
            fFound = false;
            for (size_t i = 0; i < vargs.size(); i++)
            {
                if (!_stricmp(szName, vargs[i].m_szName))
                {
                    str += vargs[i].m_strValue;
                    fFound = true;
                    break;
                }
            }
        }

        if (!fFound)
            str.append(pch, pchEnd + 1);

        pch = pchEnd + 1;
    }
    return str;
}

void CServerEvents::Log(AGCEventID idEvent, const ServerEventDef* pdef, const char* szContext, const char* szSubject,
    AGCUniqueID idSubject, const std::vector<Arg>& vargs)
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    EventSeverity severity = pdef ? pdef->m_severity : EventSeverity_Warning;
    std::string   strMessage = Expand(idEvent, pdef, szContext, szSubject, idSubject, vargs, st);

    // One event per line
    for (size_t i = 0; i < strMessage.size(); i++)
    {
        if (strMessage[i] == '\n')
            strMessage.replace(i, 1, " | ");
        else if (strMessage[i] == '\r' || strMessage[i] == '\t')
            strMessage[i] = ' ';
    }

    char szName[32];
    if (pdef)
        sprintf_s(szName, "%s", pdef->m_szName);
    else
        sprintf_s(szName, "Event%ld", idEvent);

    std::string strLine = TimeString(st);
    strLine += "  ";
    strLine += SeverityName(severity);
    strLine.append(8 - strlen(SeverityName(severity)), ' ');
    strLine += szName;
    if (szContext && *szContext)
    {
        strLine += " [";
        strLine += szContext;
        strLine += "]";
    }
    strLine += ": ";
    strLine += strMessage;

    EnterCriticalSection(&m_cs);
    WriteLine(st, strLine);
    LeaveCriticalSection(&m_cs);

    if (severity != EventSeverity_Info)
        printf("%s\n", strLine.c_str());
    debugf("%s\n", strLine.c_str());

    //
    // Errors (other than asserts) end the process, as they did with AGC
    //

    if (severity == EventSeverity_Error && idEvent != EventID_AGCAssert)
    {
        Term();

        #ifdef _DEBUG
            if (m_fBreakOnErrors && IsDebuggerPresent())
                DebugBreak();
        #endif

        exit(EXIT_FAILURE);
    }
}

void CServerEvents::WriteLine(const SYSTEMTIME& st, const std::string& strLine)
{
    //
    // logs\server-<date>.log in the settings file's folder; a new file each day
    //

    DWORD dwDay = st.wYear * 512u + st.wMonth * 32u + st.wDay;
    if (m_hfile == INVALID_HANDLE_VALUE || m_dwDayOpen != dwDay)
    {
        if (m_hfile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hfile);
            m_hfile = INVALID_HANDLE_VALUE;
        }

        std::wstring wstrFolder = ZSettings::Get().GetPath();
        size_t ichSlash = wstrFolder.find_last_of(L"\\/");
        wstrFolder = (ichSlash != std::wstring::npos) ? wstrFolder.substr(0, ichSlash + 1) : std::wstring();
        CreateDirectoryW(wstrFolder.c_str(), NULL);
        wstrFolder += L"logs";
        CreateDirectoryW(wstrFolder.c_str(), NULL);

        wchar_t wszFile[64];
        swprintf_s(wszFile, L"\\server-%04u-%02u-%02u.log", st.wYear, st.wMonth, st.wDay);

        m_hfile = CreateFileW((wstrFolder + wszFile).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        m_dwDayOpen = (m_hfile != INVALID_HANDLE_VALUE) ? dwDay : 0;
    }

    if (m_hfile != INVALID_HANDLE_VALUE)
    {
        std::string strUtf8 = AnsiToUtf8(strLine) + "\r\n";
        DWORD cbWritten = 0;
        WriteFile(m_hfile, strUtf8.data(), (DWORD)strUtf8.size(), &cbWritten, NULL);
    }
}
