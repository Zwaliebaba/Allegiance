#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// ZString's own operations (ZString.h), on std::string. They keep the
// semantics the ZString members had: int indices, -1 for not found, and the
// quirks noted in ZString.h. Out-of-range counts are clamped where ZString
// read past the end.
//
//////////////////////////////////////////////////////////////////////////////

ZString StrFromInt(int value)
{
    char buf[32];
    _itoa(value, buf, 10);
    return buf;
}

ZString StrFromFloat(float value)
{
    char buf[32];
    Bounded::Format(buf, "%g", value);
    return buf;
}

ZString StrFromFloat(float value, int total, int precision)
{
    char buf[64];
    Bounded::Format(buf, "%*.*f", total, precision, value);
    return buf;
}

ZString StrFromBool(bool value)
{
    return value ? "true" : "false";
}

ZString StrRepeat(char ch, int length)
{
    return length > 0 ? ZString(std::string((size_t)length, ch)) : ZString();
}

static int ToIndex(size_t pos)
{
    return pos == std::string::npos ? -1 : (int)pos;
}

int StrFind(const ZString& str, char ch, int index)
{
    return index < 0 || index >= (int)str.size() ? -1 : ToIndex(str.find(ch, (size_t)index));
}

int StrFind(const ZString& str, const ZString& strFind)
{
    return ToIndex(str.find(strFind));
}

int StrFindAny(const ZString& str, const ZString& strChars, int index)
{
    return index < 0 || index >= (int)str.size() ? -1 : ToIndex(str.find_first_of(strChars, (size_t)index));
}

int StrReverseFind(const ZString& str, char ch, int index)
{
    //
    // From `index` characters before the end; at the end itself the
    // terminator is what ZString compared
    //

    for (int i = (int)str.size() - index; i >= 0; i--) {
        char c = i < (int)str.size() ? str[i] : '\0';

        if (c == ch) {
            return i;
        }
    }

    return -1;
}

int StrReverseFindAny(const ZString& str, const ZString& strChars, int index)
{
    for (int i = (int)str.size() - index; i >= 0; i--) {
        if (i < (int)str.size() && strChars.find(str[i]) != std::string::npos) {
            return i;
        }
    }

    return -1;
}

int StrReverseFind(const ZString& str, const ZString& strFind)
{
    return StrFind(str, strFind);   // forwards, as ZString::ReverseFind did
}

ZString StrLeft(const ZString& str, int count)
{
    return count <= 0 ? ZString() : ZString(str.substr(0, (size_t)count));
}

ZString StrRight(const ZString& str, int count)
{
    int length = (int)str.size();

    if (count <= 0) {
        return ZString();
    } else if (count >= length) {
        return str;
    }

    return str.substr((size_t)(length - count));
}

ZString StrMiddle(const ZString& str, int index, int count)
{
    if (index < 0 || index >= (int)str.size() || count <= 0) {
        return ZString();
    }

    return str.substr((size_t)index, (size_t)count);
}

ZString StrLeftOf(const ZString& str, int count)
{
    return StrLeft(str, (int)str.size() - count);
}

ZString StrRightOf(const ZString& str, int count)
{
    return StrRight(str, (int)str.size() - count);
}

ZString StrLeftOf(const ZString& str, const ZString& strChars)
{
    int index = StrFindAny(str, strChars);

    return index == -1 ? strChars : StrLeft(str, index);
}

ZString StrRightOf(const ZString& str, const ZString& strChars)
{
    int index = StrFindAny(str, strChars);

    return index == -1 ? ZString() : StrRight(str, (int)str.size() - index - 1);
}

ZString StrToLower(const ZString& str)
{
    ZString result = str;

    for (char& ch : result) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }
    }

    return result;
}

ZString StrToUpper(const ZString& str)
{
    ZString result = str;

    for (char& ch : result) {
        if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }
    }

    return result;
}

ZString StrScramble(const ZString& str, const ZString& strKey)
{
    std::string result(str.size() + 1, '\0');
    ZScramble(result.data(), str.c_str(), strKey.c_str());
    result.resize(strlen(result.c_str()));
    return result;
}

ZString StrUnscramble(const ZString& str, const ZString& strKey)
{
    std::string result(str.size() + 1, '\0');
    ZUnscramble(result.data(), str.c_str(), strKey.c_str());
    result.resize(strlen(result.c_str()));
    return result;
}

void StrReplaceAll(ZString& str, const ZString& strChars, char ch)
{
    for (char& c : str) {
        if (strChars.find(c) != std::string::npos) {
            c = ch;
        }
    }
}

void StrRemoveAll(ZString& str, char ch)
{
    str.erase(std::remove(str.begin(), str.end(), ch), str.end());
}

void StrSetChar(ZString& str, int index, char ch)
{
    if (index >= 0 && index < (int)str.size()) {
        str[index] = ch;
    }
}

ZString StrGetToken(ZString& str)
{
    ZString strToken;
    int     length = (int)str.size();
    int     index;

    if (length > 0 && str[0] == '"') {
        index = 1;

        while (index < length && str[index] != '"')
            index++;

        if (index < length)
            strToken = StrMiddle(str, 1, index++ - 1);
        else
            strToken = str;
    } else {
        index = 0;

        while (index < length && str[index] != ' ')
            index++;

        strToken = StrLeft(str, index);
    }

    while (index < length && str[index] == ' ')
        index++;

    str = StrRight(str, length - index);

    return strToken;
}

void ZToLower(char* szDest, PCC szSource)
{
    int index;
    for (index = 0; szSource[index] != '\0'; ++index) {
        char ch = szSource[index];
        if (ch >= 'A' && ch <= 'Z')
            szDest[index] = (ch - ('A' - 'a'));
        else
            szDest[index] = ch;
    }
    szDest[index] = '\0';
}

void ZToUpper(char* szDest, PCC szSource)
{
    int index;
    for (index = 0; szSource[index] != '\0'; ++index) {
        char ch = szSource[index];
        if (ch >= 'a' && ch <= 'z')
            szDest[index] = (ch - ('a' - 'A'));
        else
            szDest[index] = ch;
    }
    szDest[index] = '\0';
}

//////////////////////////////////////////////////////////////////////////////
//
// FilenameString
//
// foo.ext
//
//////////////////////////////////////////////////////////////////////////////

ZString FilenameString::GetExtension() const
{
    return StrRightOf(*this, ".");
}

ZString FilenameString::GetName() const
{
    int index = StrFind(*this, '.', 0);

    if (index == -1) {
        return *this;
    } else {
        return StrLeft(*this, index);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// PathString
//
// foo
// ./foo
// ../foo
// /foo
//
// d:
// d:foo
// d:/foo
// //server/share
//
//  , current code doesn't handle c:\ correctly
//
//////////////////////////////////////////////////////////////////////////////

    const char* g_szDirSep = "/";

PathString::PathString(PCC pcc) :
    ZString(pcc)
{
    //  : make sure str is valid

        StrReplaceAll(*this, "\\", '/');
}

PathString PathString::GetCurrentDirectory()
{
    int size = ::GetCurrentDirectory(0, nullptr);
    char* pch = new char[size];
    ::GetCurrentDirectory(size, pch);
    PathString str(pch);
    delete[] pch;

    return str;
}

PathString PathString::GetModulePath()
{
    char ch[128];
    GetModuleFileNameA(nullptr, ch, sizeof(ch) / sizeof(*ch));
    return PathString(ch);
}

FilenameString PathString::GetFilename() const
{
    int index = StrReverseFindAny(*this, "/\\:");

    if (index == -1) {
        return *this;
    } else {
        return StrRight(*this, (int)size() - index - 1);
    }
}

ZString PathString::GetExtension() const
{
    return GetFilename().GetExtension();
}

ZString PathString::GetName() const
{
    return GetFilename().GetName();
}

PathString PathString::GetDirectory() const
{
    int index = StrReverseFindAny(*this, "/\\:");

    if (index == -1) {
        return PathString();
    } else {
        return StrLeft(*this, index);
    }
}

PathString PathString::GetDrive() const
{
    char ch0 = size() > 0 ? (*this)[0] : 0;     // std::string asserts past the end
    char ch1 = size() > 1 ? (*this)[1] : 0;

    if (   (ch0 ==  '/' && ch1 ==  '/')
        || (ch0 == '\\' && ch1 == '\\')
    ) {
        // network path

        return StrLeft(*this, StrFindAny(*this, "/\\", StrFindAny(*this, "/\\", 2) + 1) - 1);
    } else if (ch1 == ':') {
        // drive letter

        return StrLeft(*this, 2);
    }

    return PathString();
}

PathString PathString::operator+(const PathString& strRelativeArg) const
{
    PathString strAbsolute = *this;
    ZString strRelative = strRelativeArg;

    int length = (int)strRelative.size();
    char ch0 = strRelative[0];
    char ch1 = length > 0 ? strRelative[1] : 0;
    char ch2 = length > 1 ? strRelative[2] : 0;

    if (ch0 == '/' || ch0 == '\\') {
        if (ch1 == ch0) {
            // absolute network path

            return strRelative;
        } else {
            // Just use the drive from strAbsolute;

            return ZString(strAbsolute.GetDrive()) + g_szDirSep + strRelative;
        }
    } else if (ch1 == ':') {
        // The relative path has a drive letter

        if (ch2 == '/' || ch2 == '\\') {
            return strRelative;
        } else {
            ZError("Current directory relative paths not supported.");
            return strAbsolute;
        }
    } else {
        // tack the relative path onto the end of the absolute path

        while (true) {
            ch0 = strRelative[0];

            if (ch0 == '.') {
                int length = (int)strRelative.size();

                if (strRelative[1] == '.') {
                    strAbsolute = strAbsolute.GetDirectory();
                    strRelative = StrRight(strRelative, (int)strRelative.size() - 2);
                } else {
                    strRelative = StrRight(strRelative, (int)strRelative.size() - 1);
                }
            } else {
                int index = StrFindAny(strRelative, "/\\");

                if (index == -1) {
                    index = (int)strRelative.size();
                }

                char chLast = strAbsolute.empty() ? 0 : strAbsolute[strAbsolute.size() - 1];

                if (chLast == '/' || chLast == '\\') {
                    strAbsolute = ZString(strAbsolute) + StrLeft(strRelative, index);
                } else {
                    strAbsolute = ZString(strAbsolute) + g_szDirSep + StrLeft(strRelative, index);
                }

                strRelative = StrRight(strRelative, (int)strRelative.size() - index);
            }

            ch0 = strRelative[0];

            if (ch0 == '/' || ch0 == '\\') {
                strRelative = StrRight(strRelative, (int)strRelative.size() - 1);
            } else {
                return strAbsolute;
            }
        }
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// Encryption Stuff
//
//////////////////////////////////////////////////////////////////////////////

static const unsigned char vcScramble[256] = 
{
      0,  26,  45,  66, 155,  83, 137, 133, 
     71, 158,   4, 128, 124, 106,  59,  32, 
    244,   2, 100, 193,  42,  25, 228,  76, 
    110, 116,  72, 163, 214,  97,  61,  11, 
     75, 225,  33,  10,  48,  60,  14,   8, 
    101,  67, 179,   7,  82,  69, 165, 152, 
    222,   6,  44,  23,  35, 246, 180, 120, 
    129, 248, 105,  55,  24, 173, 112, 234, 
      9, 171,  99,  27,  28,  54,  22, 135, 
    102,  51, 113,  18,   3,  29,  80, 191, 
    169, 107,  46,  73,  31, 218, 177,  34, 
    176, 186,  93,  36, 153, 147, 178, 160, 
    148,  43, 182,  84, 154, 141, 164,  77, 
    233, 136,  12, 103, 156,  50,  58, 223, 
    118, 209,  13, 119, 204, 188,  52,  41, 
     21, 189, 241, 151, 192, 172, 104,  57, 
     38, 131,   5,  91, 181,  64, 219,  39, 
    205, 130,  63,  94, 109,  81, 254, 255, 
    174, 220, 142, 161, 162, 132, 211,  49, 
    202, 249, 123, 114,  95,  78, 140,  74, 
     86, 115, 251,  15,   1,  65,  62, 231, 
    125, 194, 201,  40, 143,  30, 159,  96, 
    145, 245, 200,  56,  17, 199, 183,  16, 
    134, 229, 121,  87, 149, 235, 122,  92, 
     37, 166, 157, 215, 144, 213, 126,  89, 
    187,  53, 232, 212,  19,  20, 175, 210, 
    207, 168, 150, 203, 216, 197, 252,  68, 
    247,  70, 117, 138, 146, 224, 139, 190, 
    253,  90, 185, 238, 111, 196, 206, 167, 
    108,  85, 226, 221, 208, 243, 170, 195, 
    236, 240, 242,  79, 184, 237, 239,  47, 
    217, 250, 230,  98,  88, 198, 127, 227
};

static const unsigned char vcUnscramble[256] = 
{
      0, 164,  17,  76,  10, 130,  49,  43, 
     39,  64,  35,  31, 106, 114,  38, 163, 
    183, 180,  75, 204, 205, 120,  70,  51, 
     60,  21,   1,  67,  68,  77, 173,  84, 
     15,  34,  87,  52,  91, 192, 128, 135, 
    171, 119,  20,  97,  50,   2,  82, 247, 
     36, 151, 109,  73, 118, 201,  69,  59, 
    179, 127, 110,  14,  37,  30, 166, 138, 
    133, 165,   3,  41, 215,  45, 217,   8, 
     26,  83, 159,  32,  23, 103, 157, 243, 
     78, 141,  44,   5,  99, 233, 160, 187, 
    252, 199, 225, 131, 191,  90, 139, 156, 
    175,  29, 251,  66,  18,  40,  72, 107, 
    126,  58,  13,  81, 232, 140,  24, 228, 
     62,  74, 155, 161,  25, 218, 112, 115, 
     55, 186, 190, 154,  12, 168, 198, 254, 
     11,  56, 137, 129, 149,   7, 184,  71, 
    105,   6, 219, 222, 158, 101, 146, 172, 
    196, 176, 220,  93,  96, 188, 210, 123, 
     47,  92, 100,   4, 108, 194,   9, 174, 
     95, 147, 148,  27, 102,  46, 193, 231, 
    209,  80, 238,  65, 125,  61, 144, 206, 
     88,  86,  94,  42,  54, 132,  98, 182, 
    244, 226,  89, 200, 117, 121, 223,  79, 
    124,  19, 169, 239, 229, 213, 253, 181, 
    178, 170, 152, 211, 116, 136, 230, 208, 
    236, 113, 207, 150, 203, 197,  28, 195, 
    212, 248,  85, 134, 145, 235,  48, 111, 
    221,  33, 234, 255,  22, 185, 250, 167, 
    202, 104,  63, 189, 240, 245, 227, 246, 
    241, 122, 242, 237,  16, 177,  53, 216, 
     57, 153, 249, 162, 214, 224, 142, 143
};

static unsigned char ScrambleMunge(unsigned char cbStart, unsigned char cbKey)
{
    unsigned char c = cbStart;
    
    ZAssert(c != 0);
    ZAssert(cbKey != 0);

    // (note: beware XOR, since that could generate a NULL character which 
    // would truncate the string). 

    // do an add mod 255 + 1
    //c = (unsigned char)((short(c) + short(cbKey) - 2) % 255 + 1);
    int n = (int(c) + int(cbKey));
    c = (unsigned char)(n - ((n <= 256) ? 1 : 256));

    // do a substitution
    c = vcScramble[c];

    // do a rotation  
    c = (c << (cbKey & 7)) | (c >> (8 - (cbKey & 7)));

    ZAssert(c != 0);

    return c;
}

static unsigned char ScrambleUnmunge(unsigned char cbStart, unsigned char cbKey)
{
    unsigned char c = cbStart;

    ZAssert(c != 0);
    ZAssert(cbKey > 0);

    // (note: beware XOR, since that could generate a NULL character which 
    // would truncate the string). 

    // undo the rotation  
    c = (c >> (cbKey & 7)) | (c << (8 - (cbKey & 7)));

    // undo the substitution
    c = vcUnscramble[c];

    // undo the add mod 255 + 1
    //c = (unsigned char)((short(c) - short(cbKey) + 255) % 255 + 1);
    int n = (int(c) - int(cbKey));
    c = (unsigned char)(n + ((n >= 0) ? 1 : 256));

    ZAssert(c != 0);

    return c;
}

const int c_cScrambleRounds = 12;

void ZScramble(char* szDest, PCC szSource, PCC szKey)
{
    int length = strlen(szSource);
    int lengthKey = strlen(szKey);

    memcpy(szDest, szSource, length + 1);

    for (int nRound = 0; nRound < c_cScrambleRounds; nRound++)
    {
        int index;

        // do a pass to unscramble it by the key
        for (index = 0; index < length; index++) 
            szDest[index] = (char)(ScrambleMunge((unsigned char)szDest[index], 
                (unsigned char)szKey[(index + 3*nRound) % lengthKey]));

        // cypher block chaining forwards
        for (index = 1; index < length; index++) 
            szDest[index] = (char)(ScrambleMunge((unsigned char)szDest[index], 
                (unsigned char)szDest[index-1]));
        
        // cypher block chaining backwards
        for (index = length - 2; index >= 0; index--) 
            szDest[index] = (char)(ScrambleMunge((unsigned char)szDest[index], 
                (unsigned char)szDest[index+1]));
    }
}

void ZUnscramble(char* szDest, PCC szSource, PCC szKey)
{
    int length = strlen(szSource);
    int lengthKey = strlen(szKey);

    memcpy(szDest, szSource, length + 1);

    for (int nRound = c_cScrambleRounds - 1; nRound >= 0; nRound--)
    {
        int index;

        // undo the cypher block chaining backwards
        for (index = 0; index < length - 1; index++) 
            szDest[index] = (char)(ScrambleUnmunge((unsigned char)szDest[index], 
                (unsigned char)szDest[index+1]));

        // undo the cypher block chaining forwards
        for (index = length - 1; index > 0; index--) 
            szDest[index] = (char)(ScrambleUnmunge((unsigned char)szDest[index], 
                (unsigned char)szDest[index-1]));

        // do a pass to unscramble it by the key
        for (index = 0; index < length; index++) 
            szDest[index] = (char)(ScrambleUnmunge((unsigned char)szDest[index], 
                (unsigned char)szKey[(index + 3*nRound) % lengthKey]));
    }
}

