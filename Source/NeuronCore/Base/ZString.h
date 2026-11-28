
#ifndef _ZString_H_
#define _ZString_H_

typedef const char* PCC;

//////////////////////////////////////////////////////////////////////////////
//
// ZString
//
//////////////////////////////////////////////////////////////////////////////

#include <string>

//
// ZString is std::string. The class adds only what keeps old call sites
// safe: constructing from or assigning a null const char* gives the empty
// string (as ZString always did; std::string's is undefined), and ZString
// plus a number doesn't compile (std::string's += would append one char).
// Anything that takes a const std::string& takes a ZString. See
// MODERNIZATION.md, "Library cleanup".
//

class ZString : public std::string {
public:
    ZString() {}
    ZString(PCC pcc)                 : std::string(pcc ? pcc : "") {}
    ZString(PCC pcc, int length)     : std::string(pcc, (size_t)length) {}
    ZString(const std::string& str)  : std::string(str) {}
    ZString(std::string&& str)       : std::string(std::move(str)) {}

    ZString& operator=(PCC pcc)                 { std::string::operator=(pcc ? pcc : ""); return *this; }
    ZString& operator=(const std::string& str)  { std::string::operator=(str); return *this; }
    ZString& operator=(std::string&& str)       { std::string::operator=(std::move(str)); return *this; }

    using std::string::operator+=;
    ZString& operator+=(int) = delete;
};

ZString operator+(const ZString&, int) = delete;
ZString operator+(int, const ZString&) = delete;

//////////////////////////////////////////////////////////////////////////////
//
// FilenameString
//
//////////////////////////////////////////////////////////////////////////////

class FilenameString;
class PathString;
class URLString;

class FilenameString : public ZString {
protected:
    FilenameString(const ZString& str) : ZString(str)
    {
    }

    friend PathString;

public:
    FilenameString() : ZString() {}

    ZString GetExtension() const;
    ZString GetName() const;
};

//////////////////////////////////////////////////////////////////////////////
//
// PathString
//
//////////////////////////////////////////////////////////////////////////////

class PathString : public ZString {
public:
    PathString() : ZString() {}
    PathString(PCC pcc);
    PathString(const ZString& str) : ZString(str) {}
    PathString(const std::string& str) : ZString(str) {}

    static PathString GetCurrentDirectory();
    static PathString GetModulePath();

    PathString operator+(const PathString&) const;
    PathString operator+(PCC pcc) const { return *this + PathString(pcc); }   // a path join, not std::string's +

    FilenameString GetFilename()  const;
    ZString        GetExtension() const;
    ZString        GetName()      const;
    PathString     GetDirectory() const;
    PathString     GetDrive()     const;
};

//////////////////////////////////////////////////////////////////////////////
//
// URLString
//
//////////////////////////////////////////////////////////////////////////////

class URLString : public ZString {
public:
    URLString() : ZString() {}
    URLString(const PathString& pathStr) : ZString("file://" + pathStr) {}
};

//////////////////////////////////////////////////////////////////////////////
//
// Fast encryption functions (separated from ZString to prevent heap 
// fragmentation in the lobby).
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// ZString's own operations as free functions, which keep their exact
// semantics (int indices, -1 for not found, the quirks noted below) and work
// the same once ZString is std::string
//
//////////////////////////////////////////////////////////////////////////////

// Numbers: decimal (itoa), "%g", "%<total>.<precision>f", "true"/"false"
ZString StrFromInt(int value);
ZString StrFromFloat(float value);
ZString StrFromFloat(float value, int total, int precision);
ZString StrFromBool(bool value);

// length copies of ch (ZString(ch, length); std::string's order is the reverse)
ZString StrRepeat(char ch, int length);

// Searches: the index, or -1
int StrFind(const ZString& str, char ch, int index = 0);
int StrFind(const ZString& str, const ZString& strFind);
int StrFindAny(const ZString& str, const ZString& strChars, int index = 0);
// Backwards, starting `index` characters before the end (at the terminator
// for 0)
int StrReverseFind(const ZString& str, char ch, int index = 0);
int StrReverseFindAny(const ZString& str, const ZString& strChars, int index = 0);
// Searches forwards: ZString::ReverseFind(const ZString&) always did
int StrReverseFind(const ZString& str, const ZString& strFind);

// Pieces, by count (unchecked, as before: callers keep them in range)
ZString StrLeft(const ZString& str, int count);
ZString StrRight(const ZString& str, int count);
ZString StrMiddle(const ZString& str, int index, int count);
// All but the last / first `count` characters
ZString StrLeftOf(const ZString& str, int count);
ZString StrRightOf(const ZString& str, int count);
// Before / after the first of strChars; with none, LeftOf gives strChars
// itself (sic) and RightOf the empty string
ZString StrLeftOf(const ZString& str, const ZString& strChars);
ZString StrRightOf(const ZString& str, const ZString& strChars);

// ASCII case
ZString StrToLower(const ZString& str);
ZString StrToUpper(const ZString& str);

ZString StrScramble(const ZString& str, const ZString& strKey);
ZString StrUnscramble(const ZString& str, const ZString& strKey);

// In place: each of strChars becomes ch; every ch is removed; one character
// (ignored past the end); the first token (quoted, or up to a space) is
// removed and returned
void    StrReplaceAll(ZString& str, const ZString& strChars, char ch);
void    StrRemoveAll(ZString& str, char ch);
void    StrSetChar(ZString& str, int index, char ch);
ZString StrGetToken(ZString& str);

void ZScramble(char* szDest, PCC szSource, PCC szKey);
void ZUnscramble(char* szDest, PCC szSource, PCC szKey);

void ZToUpper(char* szDest, PCC szSource);
void ZToLower(char* szDest, PCC szSource);

#endif
