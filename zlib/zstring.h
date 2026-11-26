
#ifndef _ZString_H_
#define _ZString_H_

typedef const char* PCC;

//////////////////////////////////////////////////////////////////////////////
//
// ZString
//
//////////////////////////////////////////////////////////////////////////////

class StringMDLType;

class ZString {
private:
    friend class StringMDLType;
    class ZStringData : public IObjectSingle {
    protected:
        PCC   m_pcc;
        char* m_pch;
        int   m_length;

    public:
        ZStringData(PCC pch);
        ZStringData(PCC pch, bool bStatic);
        ZStringData(PCC pch, int length);
        ZStringData(char ch, int length);
        ZStringData(const ZStringData* pdata1, const ZStringData* pdata2);
        ~ZStringData();

        void Set(int index, char ch);
        PCC  GetPointer() { return m_pcc; }

        int GetLength() const
        {
            return m_length;
        }

        int        Find(const ZStringData* pdata) const;
        int ReverseFind(const ZStringData* pdata) const;

        int        Find(char ch, int index) const;
        int ReverseFind(char ch, int index) const;

        int        FindAny(const ZStringData* pdata, int index) const;
        int ReverseFindAny(const ZStringData* pdata, int index) const;

        //BSTR MakeBSTR() const;
    };

    static TRef<ZStringData> s_pemptyData;

    TRef<ZStringData> m_pdata;

    void UniqueData();

    ZString(ZStringData*);

public:
    ZString();
    ZString(PCC);
    ZString(PCC, bool bStatic);
    ZString(PCC, int length);
    ZString(PCC, int length, bool bStatic);
    ZString(char ch, int length);
    ZString(const ZString&);
    //
    // Numbers format only when asked: ZString(n). Implicit, they turned chars
    // into their codes and would become std::string's append-one-char.
    //

    explicit ZString(int);
    explicit ZString(float value);
    ZString(float value, int total, int precision);
    explicit ZString(bool);


    const char& operator[](int index) const { return m_pdata->GetPointer()[index]; }

    //
    // std::string's names, so call sites can move to them while ZString is
    // still the type (see MODERNIZATION.md, "Library cleanup")
    //

    size_t size()   const { return (size_t)GetLength(); }
    size_t length() const { return (size_t)GetLength(); }
    bool   empty()  const { return GetLength() == 0; }
    PCC    c_str()  const { return m_pdata->GetPointer(); }
    PCC    data()   const { return m_pdata->GetPointer(); }
    void   clear()        { clear(); }
    int GetLength() const { return m_pdata->GetLength(); }
    operator PCC() const { return m_pdata->GetPointer(); }
    bool IsEmpty() const { return GetLength() == 0; }
    BSTR MakeBSTR() const;

    int Find(char ch, int index = 0) const;
    int Find(const ZString& str) const;
    int ReverseFind(char ch, int index = 0) const;
    int ReverseFind(const ZString& str) const;
    int FindAny(const ZString& str, int index = 0) const;
    int ReverseFindAny(const ZString& str, int index = 0) const;

    int GetInteger() const;

    ZString ToLower() const;
    ZString ToUpper() const;

    ZString Scramble(const ZString& strKey) const;
    ZString Unscramble(const ZString& strKey) const;

    ZString Left(int index) const;
    ZString Right(int index) const;
    ZString Middle(int index, int length) const;

    ZString LeftOf(int index) const;
    ZString RightOf(int index) const;

    ZString LeftOf(const ZString& str) const;
    ZString RightOf(const ZString& str) const;

    friend ZString operator+(const ZString&, const ZString&);

    //
    // ZString + a number: the number constructors are explicit, so without
    // these it would compile as pointer arithmetic through operator PCC (and a
    // char used to become its code, "47" for '/'). Write ZString(n).
    //

    friend ZString operator+(const ZString&, int) = delete;
    friend ZString operator+(int, const ZString&) = delete;

    friend bool operator==(const ZString&, const ZString&);
    friend bool operator==(const ZString&, PCC);
    friend bool operator==(           PCC, const ZString&);
    friend bool operator!=(const ZString&, const ZString&);
    friend bool operator!=(const ZString&, PCC);
    friend bool operator!=(           PCC, const ZString&);
    friend bool operator<(const ZString&, const ZString&);
    friend bool operator<(const ZString&, PCC);
    friend bool operator<(           PCC, const ZString&);

    // define the other comparison operators in terms of less than.
    inline friend bool operator>(const ZString& str1, const ZString& str2)   { return str2 < str1; }
    inline friend bool operator>(const ZString& str, PCC pcc)    { return pcc < str; }
    inline friend bool operator>(           PCC pcc, const ZString& str)    { return str < pcc; }
    inline friend bool operator<=(const ZString& str1, const ZString& str2)     { return !(str2 < str1); }
    inline friend bool operator<=(const ZString& str, PCC pcc)   { return !(pcc < str); }
    inline friend bool operator<=(           PCC pcc, const ZString& str)     { return !(str < pcc); }
    inline friend bool operator>=(const ZString& str1, const ZString& str2)     { return !(str1 < str2); }
    inline friend bool operator>=(const ZString& str, PCC pcc)     { return !(str < pcc); }
    inline friend bool operator>=(           PCC pcc, const ZString& str)     { return !(pcc < str); }

    void ReplaceAll(const ZString& str, char ch);
    void RemoveAll(char chToRemove);
    ZString& operator=(const ZString&);
    ZString& operator+=(const ZString&);
    void Set(int index, char ch);
    void SetEmpty();
    ZString GetToken();
};

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

    static PathString GetCurrentDirectory();
    static PathString GetModulePath();

    PathString operator+(const PathString&) const;

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
