//////////////////////////////////////////////////////////////////////////////
//
// Settings files (see ZSettings.h)
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "ZSettings.h"

#include <stdlib.h>

namespace
{

//////////////////////////////////////////////////////////////////////////////
//
// A minimal JSON value: enough for settings files, strict on input (RFC 8259,
// plus a leading UTF-8 byte order mark), and keeping object members in file
// order so hand-edited files keep their layout.
//
//////////////////////////////////////////////////////////////////////////////

struct JMember;

struct JValue
{
    enum Kind { Null, Bool, Number, String, Array, Object };

    Kind                 m_kind;
    bool                 m_b;
    double               m_n;
    std::string          m_s;       // UTF-8
    std::vector<JValue>  m_a;
    std::vector<JMember> m_o;

    JValue() : m_kind(Null), m_b(false), m_n(0) {}

    JValue*       Find(const std::string& key);
    void          Set(const std::string& key, const JValue& value);
    bool          Erase(const std::string& key);
    bool          operator==(const JValue& other) const;
    bool          operator!=(const JValue& other) const { return !(*this == other); }
};

struct JMember
{
    std::string m_key;
    JValue      m_value;
};

JValue* JValue::Find(const std::string& key)
{
    for (size_t i = 0; i < m_o.size(); i++) {
        if (m_o[i].m_key == key) {
            return &m_o[i].m_value;
        }
    }
    return NULL;
}

void JValue::Set(const std::string& key, const JValue& value)
{
    JValue* pvalue = Find(key);
    if (pvalue) {
        *pvalue = value;
    } else {
        JMember member;
        member.m_key   = key;
        member.m_value = value;
        m_o.push_back(member);
    }
}

bool JValue::Erase(const std::string& key)
{
    for (size_t i = 0; i < m_o.size(); i++) {
        if (m_o[i].m_key == key) {
            m_o.erase(m_o.begin() + i);
            return true;
        }
    }
    return false;
}

bool JValue::operator==(const JValue& other) const
{
    if (m_kind != other.m_kind) {
        return false;
    }

    switch (m_kind) {
        case Null:   return true;
        case Bool:   return m_b == other.m_b;
        case Number: return m_n == other.m_n;
        case String: return m_s == other.m_s;
        case Array:  return m_a == other.m_a;
        case Object:
            if (m_o.size() != other.m_o.size()) {
                return false;
            }
            for (size_t i = 0; i < m_o.size(); i++) {
                if (m_o[i].m_key != other.m_o[i].m_key || m_o[i].m_value != other.m_o[i].m_value) {
                    return false;
                }
            }
            return true;
    }
    return false;
}

JValue MakeNumber(double n)
{
    JValue value;
    value.m_kind = JValue::Number;
    value.m_n    = n;
    return value;
}

JValue MakeString(const std::string& s)
{
    JValue value;
    value.m_kind = JValue::String;
    value.m_s    = s;
    return value;
}

JValue MakeObject()
{
    JValue value;
    value.m_kind = JValue::Object;
    return value;
}

//////////////////////////////////////////////////////////////////////////////
//
// Parser
//
//////////////////////////////////////////////////////////////////////////////

class JParser
{
public:
    JParser(const char* pch, const char* pchEnd) :
        m_pch(pch),
        m_pchEnd(pchEnd),
        m_depth(0)
    {
    }

    bool ParseDocument(JValue& value)
    {
        if (m_pchEnd - m_pch >= 3
            && (unsigned char)m_pch[0] == 0xEF
            && (unsigned char)m_pch[1] == 0xBB
            && (unsigned char)m_pch[2] == 0xBF
        ) {
            m_pch += 3;
        }

        if (!ParseValue(value)) {
            return false;
        }

        SkipSpace();
        return m_pch == m_pchEnd;
    }

    size_t Offset(const char* pchStart) const { return m_pch - pchStart; }

private:
    const char* m_pch;
    const char* m_pchEnd;
    int         m_depth;

    void SkipSpace()
    {
        while (m_pch < m_pchEnd && (*m_pch == ' ' || *m_pch == '\t' || *m_pch == '\r' || *m_pch == '\n')) {
            m_pch++;
        }
    }

    bool Literal(const char* sz)
    {
        size_t cch = strlen(sz);
        if ((size_t)(m_pchEnd - m_pch) < cch || memcmp(m_pch, sz, cch) != 0) {
            return false;
        }
        m_pch += cch;
        return true;
    }

    bool ParseValue(JValue& value)
    {
        SkipSpace();
        if (m_pch >= m_pchEnd) {
            return false;
        }

        switch (*m_pch) {
            case '{': return ParseObject(value);
            case '[': return ParseArray(value);
            case '"': value.m_kind = JValue::String; return ParseString(value.m_s);
            case 't': value.m_kind = JValue::Bool; value.m_b = true;  return Literal("true");
            case 'f': value.m_kind = JValue::Bool; value.m_b = false; return Literal("false");
            case 'n': value.m_kind = JValue::Null; return Literal("null");
            default:  return ParseNumber(value);
        }
    }

    bool ParseObject(JValue& value)
    {
        if (++m_depth > 64) {
            return false;
        }

        value.m_kind = JValue::Object;
        m_pch++; // '{'
        SkipSpace();

        if (m_pch < m_pchEnd && *m_pch == '}') {
            m_pch++;
            m_depth--;
            return true;
        }

        for (;;) {
            SkipSpace();
            JMember member;
            if (m_pch >= m_pchEnd || *m_pch != '"' || !ParseString(member.m_key)) {
                return false;
            }

            SkipSpace();
            if (m_pch >= m_pchEnd || *m_pch != ':') {
                return false;
            }
            m_pch++;

            if (!ParseValue(member.m_value)) {
                return false;
            }

            //
            // A repeated key: the last one wins, as most readers do.
            //

            value.Set(member.m_key, member.m_value);

            SkipSpace();
            if (m_pch >= m_pchEnd) {
                return false;
            }
            if (*m_pch == ',') {
                m_pch++;
                continue;
            }
            if (*m_pch == '}') {
                m_pch++;
                m_depth--;
                return true;
            }
            return false;
        }
    }

    bool ParseArray(JValue& value)
    {
        if (++m_depth > 64) {
            return false;
        }

        value.m_kind = JValue::Array;
        m_pch++; // '['
        SkipSpace();

        if (m_pch < m_pchEnd && *m_pch == ']') {
            m_pch++;
            m_depth--;
            return true;
        }

        for (;;) {
            JValue element;
            if (!ParseValue(element)) {
                return false;
            }
            value.m_a.push_back(element);

            SkipSpace();
            if (m_pch >= m_pchEnd) {
                return false;
            }
            if (*m_pch == ',') {
                m_pch++;
                continue;
            }
            if (*m_pch == ']') {
                m_pch++;
                m_depth--;
                return true;
            }
            return false;
        }
    }

    static int HexDigit(char ch)
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }

    bool Hex4(unsigned& u)
    {
        if (m_pchEnd - m_pch < 4) {
            return false;
        }

        u = 0;
        for (int i = 0; i < 4; i++) {
            int digit = HexDigit(m_pch[i]);
            if (digit < 0) {
                return false;
            }
            u = (u << 4) | (unsigned)digit;
        }
        m_pch += 4;
        return true;
    }

    static void AppendUtf8(std::string& s, unsigned cp)
    {
        if (cp < 0x80) {
            s += (char)cp;
        } else if (cp < 0x800) {
            s += (char)(0xC0 | (cp >> 6));
            s += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s += (char)(0xE0 | (cp >> 12));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        } else {
            s += (char)(0xF0 | (cp >> 18));
            s += (char)(0x80 | ((cp >> 12) & 0x3F));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool ParseString(std::string& s)
    {
        s.clear();
        m_pch++; // '"'

        while (m_pch < m_pchEnd) {
            unsigned char ch = (unsigned char)*m_pch++;

            if (ch == '"') {
                return true;
            }

            if (ch < 0x20) {
                return false;
            }

            if (ch != '\\') {
                s += (char)ch;
                continue;
            }

            if (m_pch >= m_pchEnd) {
                return false;
            }

            switch (*m_pch++) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case '/':  s += '/';  break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u':
                {
                    unsigned cp;
                    if (!Hex4(cp)) {
                        return false;
                    }

                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        unsigned low;
                        if (m_pchEnd - m_pch < 2 || m_pch[0] != '\\' || m_pch[1] != 'u') {
                            return false;
                        }
                        m_pch += 2;
                        if (!Hex4(low) || low < 0xDC00 || low > 0xDFFF) {
                            return false;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;
                    }

                    AppendUtf8(s, cp);
                    break;
                }
                default:
                    return false;
            }
        }

        return false;
    }

    bool ParseNumber(JValue& value)
    {
        const char* pchStart = m_pch;

        if (m_pch < m_pchEnd && *m_pch == '-') {
            m_pch++;
        }

        if (m_pch >= m_pchEnd) {
            return false;
        }

        if (*m_pch == '0') {
            m_pch++;
        } else if (*m_pch >= '1' && *m_pch <= '9') {
            while (m_pch < m_pchEnd && *m_pch >= '0' && *m_pch <= '9') m_pch++;
        } else {
            return false;
        }

        if (m_pch < m_pchEnd && *m_pch == '.') {
            m_pch++;
            if (m_pch >= m_pchEnd || *m_pch < '0' || *m_pch > '9') {
                return false;
            }
            while (m_pch < m_pchEnd && *m_pch >= '0' && *m_pch <= '9') m_pch++;
        }

        if (m_pch < m_pchEnd && (*m_pch == 'e' || *m_pch == 'E')) {
            m_pch++;
            if (m_pch < m_pchEnd && (*m_pch == '+' || *m_pch == '-')) {
                m_pch++;
            }
            if (m_pch >= m_pchEnd || *m_pch < '0' || *m_pch > '9') {
                return false;
            }
            while (m_pch < m_pchEnd && *m_pch >= '0' && *m_pch <= '9') m_pch++;
        }

        std::string str(pchStart, m_pch);
        value.m_kind = JValue::Number;
        value.m_n    = strtod(str.c_str(), NULL);
        return true;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Writer: two-space indentation, arrays of numbers on one line
//
//////////////////////////////////////////////////////////////////////////////

void WriteString(std::string& out, const std::string& s)
{
    out += '"';
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (ch < 0x20) {
                    char sz[8];
                    sprintf_s(sz, "\\u%04x", ch);
                    out += sz;
                } else {
                    out += (char)ch;
                }
        }
    }
    out += '"';
}

void WriteNumber(std::string& out, double n)
{
    char sz[32];
    if (n == (double)(long long)n && n > -9007199254740992.0 && n < 9007199254740992.0) {
        sprintf_s(sz, "%lld", (long long)n);
    } else {
        sprintf_s(sz, "%.17g", n);
    }
    out += sz;
}

bool IsFlat(const JValue& value)
{
    for (size_t i = 0; i < value.m_a.size(); i++) {
        JValue::Kind kind = value.m_a[i].m_kind;
        if (kind == JValue::Array || kind == JValue::Object) {
            return false;
        }
    }
    return true;
}

void WriteValue(std::string& out, const JValue& value, int indent)
{
    switch (value.m_kind) {
        case JValue::Null:   out += "null"; break;
        case JValue::Bool:   out += value.m_b ? "true" : "false"; break;
        case JValue::Number: WriteNumber(out, value.m_n); break;
        case JValue::String: WriteString(out, value.m_s); break;

        case JValue::Array:
            if (value.m_a.empty()) {
                out += "[]";
            } else if (IsFlat(value)) {
                out += '[';
                for (size_t i = 0; i < value.m_a.size(); i++) {
                    if (i) out += ", ";
                    WriteValue(out, value.m_a[i], indent);
                }
                out += ']';
            } else {
                out += "[\n";
                for (size_t i = 0; i < value.m_a.size(); i++) {
                    out.append(indent + 2, ' ');
                    WriteValue(out, value.m_a[i], indent + 2);
                    out += (i + 1 < value.m_a.size()) ? ",\n" : "\n";
                }
                out.append(indent, ' ');
                out += ']';
            }
            break;

        case JValue::Object:
            if (value.m_o.empty()) {
                out += "{}";
            } else {
                out += "{\n";
                for (size_t i = 0; i < value.m_o.size(); i++) {
                    out.append(indent + 2, ' ');
                    WriteString(out, value.m_o[i].m_key);
                    out += ": ";
                    WriteValue(out, value.m_o[i].m_value, indent + 2);
                    out += (i + 1 < value.m_o.size()) ? ",\n" : "\n";
                }
                out.append(indent, ' ');
                out += '}';
            }
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Code page conversion (the game's ANSI strings <-> the file's UTF-8)
//
//////////////////////////////////////////////////////////////////////////////

std::string Convert(const std::string& s, UINT cpFrom, UINT cpTo)
{
    if (s.empty()) {
        return s;
    }

    int cwch = MultiByteToWideChar(cpFrom, 0, s.data(), (int)s.size(), NULL, 0);
    if (cwch <= 0) {
        return std::string();
    }

    std::wstring wstr(cwch, L'\0');
    MultiByteToWideChar(cpFrom, 0, s.data(), (int)s.size(), &wstr[0], cwch);

    int cch = WideCharToMultiByte(cpTo, 0, wstr.data(), cwch, NULL, 0, NULL, NULL);
    if (cch <= 0) {
        return std::string();
    }

    std::string result(cch, '\0');
    WideCharToMultiByte(cpTo, 0, wstr.data(), cwch, &result[0], cch, NULL, NULL);
    return result;
}

std::string ToUtf8(const char* sz)
{
    return Convert(sz ? sz : "", CP_ACP, CP_UTF8);
}

std::string FromUtf8(const std::string& s)
{
    return Convert(s, CP_UTF8, CP_ACP);
}

std::string Narrow(const std::wstring& wstr)
{
    if (wstr.empty()) {
        return std::string();
    }
    int cch = WideCharToMultiByte(CP_ACP, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(cch > 0 ? cch : 0, '\0');
    if (cch > 0) {
        WideCharToMultiByte(CP_ACP, 0, wstr.data(), (int)wstr.size(), &result[0], cch, NULL, NULL);
    }
    return result;
}

void Trace(const char* szFormat, const std::wstring& wstrPath, const char* szDetail = "")
{
    char sz[1024];
    sprintf_s(sz, szFormat, Narrow(wstrPath).c_str(), szDetail);
    OutputDebugStringA(sz);
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
//
// ZSettingsFile
//
//////////////////////////////////////////////////////////////////////////////

struct ZSettingsFile::Impl
{
    std::wstring     m_wstrPath;
    CRITICAL_SECTION m_cs;
    HANDLE           m_hmutex;
    JValue           m_root;
    bool             m_fLoaded;
    bool             m_fCorrupt;    // the file on disk didn't parse; back it up before writing

    explicit Impl(const wchar_t* wszPath) :
        m_wstrPath(wszPath ? wszPath : L""),
        m_hmutex(NULL),
        m_root(MakeObject()),
        m_fLoaded(false),
        m_fCorrupt(false)
    {
        InitializeCriticalSection(&m_cs);

        //
        // One mutex per file, named after its path (lower case, FNV-1a)
        //

        std::wstring wstrLower(m_wstrPath);
        if (!wstrLower.empty()) {
            CharLowerBuffW(&wstrLower[0], (DWORD)wstrLower.size());
        }

        unsigned long long hash = 14695981039346656037ULL;
        for (size_t i = 0; i < wstrLower.size(); i++) {
            hash = (hash ^ (unsigned)wstrLower[i]) * 1099511628211ULL;
        }

        wchar_t wszName[64];
        swprintf_s(wszName, L"Local\\Allegiance.Settings.%016llx", hash);
        m_hmutex = CreateMutexW(NULL, FALSE, wszName);
    }

    ~Impl()
    {
        if (m_hmutex) {
            CloseHandle(m_hmutex);
        }
        DeleteCriticalSection(&m_cs);
    }

    void LockFile()
    {
        if (m_hmutex) {
            // WAIT_ABANDONED still hands over ownership: the file is whole,
            // since it is only ever replaced by a rename.
            WaitForSingleObject(m_hmutex, 5000);
        }
    }

    void UnlockFile()
    {
        if (m_hmutex) {
            ReleaseMutex(m_hmutex);
        }
    }

    //
    // Read the file into m_root. A missing file is an empty object; a file
    // that doesn't parse leaves m_root as it was and is set aside on the
    // next write.
    //

    void Load()
    {
        m_fLoaded = true;

        HANDLE hfile = CreateFileW(m_wstrPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
                m_root     = MakeObject();
                m_fCorrupt = false;
            }
            return;
        }

        LARGE_INTEGER size;
        std::string   data;
        bool          fRead = false;

        if (GetFileSizeEx(hfile, &size) && size.QuadPart < 16 * 1024 * 1024) {
            data.resize((size_t)size.QuadPart);
            DWORD cbRead = 0;
            fRead = data.empty()
                || (ReadFile(hfile, &data[0], (DWORD)data.size(), &cbRead, NULL) && cbRead == data.size());
        }
        CloseHandle(hfile);

        if (!fRead) {
            Trace("Settings: couldn't read %s%s\n", m_wstrPath);
            return;
        }

        //
        // An empty file reads as an empty object
        //

        bool fBlank = true;
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] != ' ' && data[i] != '\t' && data[i] != '\r' && data[i] != '\n') {
                fBlank = false;
                break;
            }
        }

        if (fBlank) {
            m_root     = MakeObject();
            m_fCorrupt = false;
            return;
        }

        JValue  root;
        JParser parser(data.data(), data.data() + data.size());

        if (parser.ParseDocument(root) && root.m_kind == JValue::Object) {
            m_root     = root;
            m_fCorrupt = false;
        } else {
            char szOffset[64];
            sprintf_s(szOffset, " (near byte %u)", (unsigned)parser.Offset(data.data()));
            Trace("Settings: %s is not a JSON object%s; using defaults, and it will be kept as .bad\n", m_wstrPath, szOffset);
            m_fCorrupt = true;
        }
    }

    void EnsureLoaded()
    {
        if (!m_fLoaded) {
            LockFile();
            Load();
            UnlockFile();
        }
    }

    bool Save()
    {
        if (m_fCorrupt) {
            std::wstring wstrBad = m_wstrPath + L".bad";
            MoveFileExW(m_wstrPath.c_str(), wstrBad.c_str(), MOVEFILE_REPLACE_EXISTING);
            m_fCorrupt = false;
        }

        //
        // Create the folder (one level: %LOCALAPPDATA%\Allegiance)
        //

        size_t ichSlash = m_wstrPath.find_last_of(L"\\/");
        if (ichSlash != std::wstring::npos) {
            CreateDirectoryW(m_wstrPath.substr(0, ichSlash).c_str(), NULL);
        }

        std::string text;
        WriteValue(text, m_root, 0);
        text += "\r\n";

        std::wstring wstrTemp = m_wstrPath + L".tmp";
        HANDLE hfile = CreateFileW(wstrTemp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE) {
            Trace("Settings: couldn't write %s%s\n", wstrTemp);
            return false;
        }

        DWORD cbWritten = 0;
        bool  fOk = WriteFile(hfile, text.data(), (DWORD)text.size(), &cbWritten, NULL) && cbWritten == text.size();
        fOk = FlushFileBuffers(hfile) && fOk;
        CloseHandle(hfile);

        if (!fOk || !MoveFileExW(wstrTemp.c_str(), m_wstrPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(wstrTemp.c_str());
            Trace("Settings: couldn't replace %s%s\n", m_wstrPath);
            return false;
        }

        return true;
    }

    JValue* Find(const char* szSection, const char* szName)
    {
        JValue* pparent = &m_root;
        if (szSection) {
            pparent = m_root.Find(ToUtf8(szSection));
            if (!pparent || pparent->m_kind != JValue::Object) {
                return NULL;
            }
        }
        return pparent->Find(ToUtf8(szName));
    }

    //
    // Re-read, apply, and write if anything changed. value NULL removes.
    //

    void Update(const char* szSection, const char* szName, const JValue* pvalue)
    {
        EnterCriticalSection(&m_cs);
        LockFile();

        Load();

        JValue before = m_root;
        JValue* pparent = &m_root;

        if (szSection) {
            std::string section = ToUtf8(szSection);
            pparent = m_root.Find(section);
            if (!pparent || pparent->m_kind != JValue::Object) {
                if (pvalue) {
                    m_root.Set(section, MakeObject());
                    pparent = m_root.Find(section);
                } else {
                    pparent = NULL;
                }
            }
        }

        if (pparent) {
            if (pvalue) {
                pparent->Set(ToUtf8(szName), *pvalue);
            } else {
                pparent->Erase(ToUtf8(szName));
            }
        }

        if (m_root != before || m_fCorrupt) {
            Save();
        }

        UnlockFile();
        LeaveCriticalSection(&m_cs);
    }
};

ZSettingsFile::ZSettingsFile(const wchar_t* wszPath) :
    m_pimpl(new Impl(wszPath))
{
}

ZSettingsFile::~ZSettingsFile()
{
    delete m_pimpl;
}

const std::wstring& ZSettingsFile::GetPath() const
{
    return m_pimpl->m_wstrPath;
}

bool ZSettingsFile::Has(const char* szSection, const char* szName)
{
    EnterCriticalSection(&m_pimpl->m_cs);
    m_pimpl->EnsureLoaded();
    bool fHas = m_pimpl->Find(szSection, szName) != NULL;
    LeaveCriticalSection(&m_pimpl->m_cs);
    return fHas;
}

DWORD ZSettingsFile::GetDWORD(const char* szSection, const char* szName, DWORD dwDefault)
{
    DWORD dwResult = dwDefault;

    EnterCriticalSection(&m_pimpl->m_cs);
    m_pimpl->EnsureLoaded();

    const JValue* pvalue = m_pimpl->Find(szSection, szName);
    if (pvalue) {
        switch (pvalue->m_kind) {
            case JValue::Number:
                // negative numbers wrap, as a REG_DWORD written from an int did
                if (pvalue->m_n >= -2147483648.0 && pvalue->m_n <= 4294967295.0) {
                    dwResult = (DWORD)(long long)pvalue->m_n;
                }
                break;

            case JValue::Bool:
                dwResult = pvalue->m_b ? 1 : 0;
                break;

            case JValue::String:
            {
                const char* sz    = pvalue->m_s.c_str();
                char*       szEnd = NULL;
                unsigned long ul  = strtoul(sz, &szEnd, 0);
                if (*sz && szEnd && *szEnd == '\0') {
                    dwResult = (DWORD)ul;
                }
                break;
            }

            default:
                break;
        }
    }

    LeaveCriticalSection(&m_pimpl->m_cs);
    return dwResult;
}

std::string ZSettingsFile::GetString(const char* szSection, const char* szName, const char* szDefault)
{
    std::string result(szDefault ? szDefault : "");

    EnterCriticalSection(&m_pimpl->m_cs);
    m_pimpl->EnsureLoaded();

    const JValue* pvalue = m_pimpl->Find(szSection, szName);
    if (pvalue) {
        switch (pvalue->m_kind) {
            case JValue::String:
                result = FromUtf8(pvalue->m_s);
                break;

            case JValue::Number:
                result.clear();
                WriteNumber(result, pvalue->m_n);
                break;

            case JValue::Bool:
                result = pvalue->m_b ? "1" : "0";
                break;

            default:
                break;
        }
    }

    LeaveCriticalSection(&m_pimpl->m_cs);
    return result;
}

bool ZSettingsFile::GetDWORDs(const char* szSection, const char* szName, std::vector<DWORD>& values)
{
    bool fFound = false;
    values.clear();

    EnterCriticalSection(&m_pimpl->m_cs);
    m_pimpl->EnsureLoaded();

    const JValue* pvalue = m_pimpl->Find(szSection, szName);
    if (pvalue && pvalue->m_kind == JValue::Array) {
        fFound = true;
        for (size_t i = 0; i < pvalue->m_a.size(); i++) {
            const JValue& element = pvalue->m_a[i];
            if (element.m_kind == JValue::Number && element.m_n >= -2147483648.0 && element.m_n <= 4294967295.0) {
                values.push_back((DWORD)(long long)element.m_n);
            }
        }
    }

    LeaveCriticalSection(&m_pimpl->m_cs);
    return fFound;
}

void ZSettingsFile::SetDWORD(const char* szSection, const char* szName, DWORD dwValue)
{
    JValue value = MakeNumber((double)dwValue);
    m_pimpl->Update(szSection, szName, &value);
}

void ZSettingsFile::SetString(const char* szSection, const char* szName, const char* szValue)
{
    JValue value = MakeString(ToUtf8(szValue));
    m_pimpl->Update(szSection, szName, &value);
}

void ZSettingsFile::SetDWORDs(const char* szSection, const char* szName, const std::vector<DWORD>& values)
{
    JValue value;
    value.m_kind = JValue::Array;
    for (size_t i = 0; i < values.size(); i++) {
        value.m_a.push_back(MakeNumber((double)values[i]));
    }
    m_pimpl->Update(szSection, szName, &value);
}

void ZSettingsFile::Remove(const char* szSection, const char* szName)
{
    m_pimpl->Update(szSection, szName, NULL);
}

//////////////////////////////////////////////////////////////////////////////
//
// The process's settings file
//
//////////////////////////////////////////////////////////////////////////////

namespace
{
    const char*        g_szFileName = "settings.json";
    ZSettings::Scope   g_scope      = ZSettings::PerUser;
    ZSettingsFile*     g_pfile      = NULL;
    INIT_ONCE          g_initOnce   = INIT_ONCE_STATIC_INIT;

    BOOL CALLBACK CreateProcessSettings(PINIT_ONCE, PVOID, PVOID*)
    {
        g_pfile = new ZSettingsFile(ZSettings::PathFor(g_szFileName, g_scope).c_str());
        return TRUE;
    }
}

void ZSettings::Configure(const char* szFileName, Scope scope)
{
    // Only before the first Get(); later calls would be ignored.
    if (g_pfile == NULL) {
        g_szFileName = szFileName;
        g_scope      = scope;
    }
}

std::wstring ZSettings::PathFor(const char* szFileName, Scope scope)
{
    std::wstring wstrName;
    for (const char* pch = szFileName; *pch; pch++) {
        wstrName += (wchar_t)(unsigned char)*pch;
    }

    //
    // A file next to the executable wins: a portable install
    //

    wchar_t wszModule[MAX_PATH];
    DWORD cch = GetModuleFileNameW(NULL, wszModule, MAX_PATH);
    if (cch > 0 && cch < MAX_PATH) {
        std::wstring wstrExeDir(wszModule);
        size_t ichSlash = wstrExeDir.find_last_of(L"\\/");
        if (ichSlash != std::wstring::npos) {
            std::wstring wstrLocal = wstrExeDir.substr(0, ichSlash + 1) + wstrName;
            if (GetFileAttributesW(wstrLocal.c_str()) != INVALID_FILE_ATTRIBUTES) {
                return wstrLocal;
            }
        }
    }

    //
    // %LOCALAPPDATA% or %ProgramData% (both always set on Windows 10 and
    // later, services included); failing that, the current directory
    //

    wchar_t wszFolder[MAX_PATH];
    DWORD   cchFolder = GetEnvironmentVariableW(scope == AllUsers ? L"ProgramData" : L"LOCALAPPDATA", wszFolder, MAX_PATH);

    if (cchFolder == 0 || cchFolder >= MAX_PATH) {
        return wstrName;
    }

    return std::wstring(wszFolder) + L"\\Allegiance\\" + wstrName;
}

ZSettingsFile& ZSettings::Get()
{
    InitOnceExecuteOnce(&g_initOnce, CreateProcessSettings, NULL, NULL);
    return *g_pfile;
}
