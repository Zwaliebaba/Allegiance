#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// Server events: what AGC's event system did for the server, in plain C++.
// See MODERNIZATION.md, "AGC removed: server events go to a log file".
//
// Code raises an event with g_events.TriggerEvent or TriggerContextEvent,
// in the shape AGC used, so the call sites stayed as they were:
//
//   g_events.TriggerEvent(NULL, AllsrvEventID_Connecting, "", -1, -1, -1, 1,
//       "LobbyServer", VT_LPSTR, PCC(g.strLobbyServer));
//
// The arguments after the count come in (name, VT_ type, value) triplets.
// The values are taken by their C++ type, so a mismatched VT_ tag can no
// longer misread the stack; the tag only marks VT_BOOL values.
//
// An event is written to the server log (logs\server-<date>.log next to the
// settings file) if it is logged by default (ServerEventsDefs.h) or the
// server.json setting LogAllEvents is 1. Warnings and errors also go to the
// console. An event of severity Error ends the process, as it did with AGC.
//
//////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <wtypes.h>     // VARTYPE and the VT_ constants
#include <string>
#include <vector>
#include <type_traits>

enum EventSeverity
{
    EventSeverity_Info,
    EventSeverity_Warning,
    EventSeverity_Error
};

#include "ServerEventsDefs.h"

typedef long AGCEventID;            // a ServerEventIDs value (name kept from AGC)
typedef long AGCUniqueID;           // the ID of an event's subject or object
typedef const void* HAGCLISTENERS;  // from EventListeners: non-NULL if the event is logged

const long AGC_AdminUser = 35;      // object type in the high word of a player's event ID

struct ServerEventDef
{
    AGCEventID    m_id;
    const char*   m_szName;
    EventSeverity m_severity;
    bool          m_fLogByDefault;
    const char*   m_szFormat;
};

class CServerEvents
{
public:
    CServerEvents();
    ~CServerEvents();

    //
    // Read the settings (LogAllEvents); fBreakOnErrors breaks into an attached
    // debugger before an Error event ends the process (debug builds)
    //

    void Init(bool fBreakOnErrors);
    void Term();

    //
    // Whether anyone wants the event: lets a caller skip preparing arguments
    //

    HAGCLISTENERS EventListeners(AGCEventID idEvent, AGCUniqueID = -1, AGCUniqueID = -1, AGCUniqueID = -1);

    template <class... Args>
    void TriggerEvent(HAGCLISTENERS, AGCEventID idEvent, const char* szSubject,
        AGCUniqueID idSubject, AGCUniqueID, AGCUniqueID, long, const Args&... args)
    {
        Trigger(idEvent, nullptr, szSubject, idSubject, args...);
    }

    template <class... Args>
    void TriggerContextEvent(HAGCLISTENERS, AGCEventID idEvent, const char* szContext, const char* szSubject,
        AGCUniqueID idSubject, AGCUniqueID, AGCUniqueID, long, const Args&... args)
    {
        Trigger(idEvent, szContext, szSubject, idSubject, args...);
    }

private:
    struct Arg
    {
        const char* m_szName;
        std::string m_strValue;
    };

    template <class... Args>
    void Trigger(AGCEventID idEvent, const char* szContext, const char* szSubject, AGCUniqueID idSubject, const Args&... args)
    {
        static_assert(sizeof...(Args) % 3 == 0, "event arguments come in (name, VT_ type, value) triplets");

        const ServerEventDef* pdef = Find(idEvent);
        if (!IsWanted(pdef))
            return;

        std::vector<Arg> vargs;
        vargs.reserve(sizeof...(Args) / 3);
        AddArgs(vargs, args...);
        Log(idEvent, pdef, szContext, szSubject, idSubject, vargs);
    }

    static void AddArgs(std::vector<Arg>&) {}

    template <class V, class... Rest>
    static void AddArgs(std::vector<Arg>& vargs, const char* szName, VARTYPE vt, const V& value, const Rest&... rest)
    {
        Arg arg;
        arg.m_szName   = szName;
        arg.m_strValue = ToString(vt, static_cast<typename std::decay<const V>::type>(value));
        vargs.push_back(arg);
        AddArgs(vargs, rest...);
    }

    static std::string ToString(VARTYPE, const char* sz);
    static std::string ToString(VARTYPE, const wchar_t* wsz);
    static std::string ToString(VARTYPE, char* sz)    { return ToString(VT_LPSTR,  (const char*)sz); }
    static std::string ToString(VARTYPE, wchar_t* wsz) { return ToString(VT_LPWSTR, (const wchar_t*)wsz); }
    static std::string ToString(VARTYPE, double d);
    static std::string ToString(VARTYPE vt, float f)   { return ToString(vt, (double)f); }
    static std::string ToString(VARTYPE vt, long long n, bool fSigned);

    template <class T>
    static typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, std::string>::type
    ToString(VARTYPE vt, T n)
    {
        return ToString(vt, (long long)n, std::is_signed<T>::value);
    }

    const ServerEventDef* Find(AGCEventID idEvent) const;
    bool IsWanted(const ServerEventDef* pdef) const;
    void Log(AGCEventID idEvent, const ServerEventDef* pdef, const char* szContext, const char* szSubject,
        AGCUniqueID idSubject, const std::vector<Arg>& vargs);
    std::string Expand(AGCEventID idEvent, const ServerEventDef* pdef, const char* szContext,
        const char* szSubject, AGCUniqueID idSubject, const std::vector<Arg>& vargs, const SYSTEMTIME& st) const;
    void WriteLine(const SYSTEMTIME& st, const std::string& strLine);

    CRITICAL_SECTION m_cs;
    bool             m_fLogAll;
    bool             m_fBreakOnErrors;
    HANDLE           m_hfile;
    DWORD            m_dwDayOpen;    // the day m_hfile is for (year * 512 + month * 32 + day)
    std::string      m_strComputerName;
};

extern CServerEvents g_events;
