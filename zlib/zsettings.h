#ifndef _ZSETTINGS_H_
#define _ZSETTINGS_H_

//////////////////////////////////////////////////////////////////////////////
//
// Settings files: JSON key/value stores that replace the old registry keys
// (HKLM\Software\Microsoft\Microsoft Games\Allegiance\...). See
// MODERNIZATION.md, "Settings: a JSON file replaces the registry".
//
// A file holds one JSON object. Values sit at the top level or one level
// down in a named section (an object), e.g.
//
//   {
//     "CombatXSize": 800,
//     "CharacterName": "Pilot",
//     "SquadMemberships": { "Pilot": [12, 40] }
//   }
//
// Numbers stand in for the old REG_DWORD values, so flags are 0 or 1. The
// getters are lenient for hand-edited files: true/false, and numeric
// strings, read as numbers.
//
// Names, sections and string values are in the ANSI code page at this
// interface, as the game uses them; the file itself is UTF-8.
//
// Every Set re-reads the file and writes it back whole (to a temporary file,
// then renamed over the original), under a named mutex, so several readers
// and writers of one file, in one process or several, don't lose each
// other's changes. Gets are served from what was last read.
//
// Deliberately free of ZString and the rest of zlib, so any module can use it.
//
//////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <string>
#include <vector>

class ZSettingsFile
{
public:
    explicit ZSettingsFile(const wchar_t* wszPath);
    ~ZSettingsFile();

    const std::wstring& GetPath() const;

    //
    // szSection NULL means the top level.
    //

    bool        Has      (const char* szSection, const char* szName);
    DWORD       GetDWORD (const char* szSection, const char* szName, DWORD dwDefault);
    std::string GetString(const char* szSection, const char* szName, const char* szDefault);
    bool        GetDWORDs(const char* szSection, const char* szName, std::vector<DWORD>& values);

    void        SetDWORD (const char* szSection, const char* szName, DWORD dwValue);
    void        SetString(const char* szSection, const char* szName, const char* szValue);
    void        SetDWORDs(const char* szSection, const char* szName, const std::vector<DWORD>& values);
    void        Remove   (const char* szSection, const char* szName);

private:
    ZSettingsFile(const ZSettingsFile&);
    ZSettingsFile& operator=(const ZSettingsFile&);

    struct Impl;
    Impl* m_pimpl;
};

//////////////////////////////////////////////////////////////////////////////
//
// The process's own settings file
//
// The client uses the default, settings.json per user. The server calls
// Configure("server.json", AllUsers) before anything reads a setting.
//
// Where the file lives: next to the executable if a file of that name is
// there (a portable install), otherwise in %LOCALAPPDATA%\Allegiance
// (PerUser) or %ProgramData%\Allegiance (AllUsers).
//
//////////////////////////////////////////////////////////////////////////////

namespace ZSettings
{
    enum Scope { PerUser, AllUsers };

    void           Configure(const char* szFileName, Scope scope);
    std::wstring   PathFor(const char* szFileName, Scope scope);
    ZSettingsFile& Get();

    inline DWORD GetDWORD(const char* szName, DWORD dwDefault)
    {
        return Get().GetDWORD(NULL, szName, dwDefault);
    }

    inline std::string GetString(const char* szName, const char* szDefault)
    {
        return Get().GetString(NULL, szName, szDefault);
    }

    inline void SetDWORD(const char* szName, DWORD dwValue)
    {
        Get().SetDWORD(NULL, szName, dwValue);
    }

    inline void SetString(const char* szName, const char* szValue)
    {
        Get().SetString(NULL, szName, szValue);
    }
}

#endif
