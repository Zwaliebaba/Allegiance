#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// ZString (a std::string) and its Str* helpers (zlib/zstring.h). The helpers
// keep the semantics of the ZString members they replaced, quirks included;
// the tests named Quirk* pin those, so changing one is a deliberate act.
//
//////////////////////////////////////////////////////////////////////////////

// What must not compile: ZString plus a number (std::string's += would append
// one char), and the implicit const char* conversion ZString used to have
template<class T> concept AppendsInt = requires(T t) { t += 1; };
template<class T> concept AddsIntRight = requires(T t) { t + 1; };
template<class T> concept AddsIntLeft = requires(T t) { 1 + t; };

static_assert(AppendsInt<std::string>, "the AppendsInt check itself");
static_assert(!AppendsInt<ZString>);
static_assert(!AddsIntRight<ZString>);
static_assert(!AddsIntLeft<ZString>);
static_assert(!std::is_convertible_v<ZString, PCC>);
static_assert(std::is_convertible_v<ZString, const std::string&>);

namespace ZStringTests
{
    TEST_CLASS(Construction)
    {
    public:
        TEST_METHOD(NullPointerIsEmpty)
        {
            ZString str((PCC)nullptr);
            Assert::IsTrue(str.empty());

            ZString strAssigned = "x";
            strAssigned = (PCC)nullptr;
            Assert::IsTrue(strAssigned.empty());
        }

        TEST_METHOD(PointerAndLength)
        {
            AreEqualStr("abc", ZString("abcdef", 3));
        }

        TEST_METHOD(AppendsStrings)
        {
            ZString str = "a";
            str += "b";
            str += 'c';
            str += ZString("d");
            AreEqualStr("abcd", str);
            AreEqualStr("abcde", str + "e");
        }
    };

    TEST_CLASS(Numbers)
    {
    public:
        TEST_METHOD(Int)
        {
            AreEqualStr("0", StrFromInt(0));
            AreEqualStr("-42", StrFromInt(-42));
            AreEqualStr("2147483647", StrFromInt(2147483647));
        }

        TEST_METHOD(FloatIsPercentG)
        {
            AreEqualStr("1.5", StrFromFloat(1.5f));
            AreEqualStr("0.1", StrFromFloat(0.1f));
            AreEqualStr("-2", StrFromFloat(-2.0f));
            AreEqualStr("100000", StrFromFloat(100000.0f));
            AreEqualStr("1e+06", StrFromFloat(1000000.0f));
        }

        TEST_METHOD(FloatWithWidthAndPrecision)
        {
            AreEqualStr("  3.14", StrFromFloat(3.14159f, 6, 2));
            AreEqualStr("1", StrFromFloat(1.0f, 0, 0));
        }

        TEST_METHOD(Bool)
        {
            AreEqualStr("true", StrFromBool(true));
            AreEqualStr("false", StrFromBool(false));
        }

        TEST_METHOD(Repeat)
        {
            AreEqualStr("---", StrRepeat('-', 3));
            AreEqualStr("", StrRepeat('-', 0));
            AreEqualStr("", StrRepeat('-', -1));
        }
    };

    TEST_CLASS(Searches)
    {
    public:
        TEST_METHOD(FindChar)
        {
            Assert::AreEqual(2, StrFind("hello", 'l'));
            Assert::AreEqual(3, StrFind("hello", 'l', 3));
            Assert::AreEqual(-1, StrFind("hello", 'z'));
            Assert::AreEqual(-1, StrFind("hello", 'h', 5));
            Assert::AreEqual(-1, StrFind("hello", 'h', -1));
        }

        TEST_METHOD(FindString)
        {
            Assert::AreEqual(6, StrFind("hello world", ZString("world")));
            Assert::AreEqual(-1, StrFind("hello world", ZString("xyz")));
            Assert::AreEqual(0, StrFind("abc", ZString("")));
        }

        TEST_METHOD(FindAny)
        {
            Assert::AreEqual(1, StrFindAny("a,b;c", ";,"));
            Assert::AreEqual(3, StrFindAny("a,b;c", ";,", 2));
            Assert::AreEqual(-1, StrFindAny("abc", "xyz"));
        }

        TEST_METHOD(ReverseFindChar)
        {
            Assert::AreEqual(3, StrReverseFind("a/b/c", '/'));
            Assert::AreEqual(3, StrReverseFind("a/b/c", '/', 2));
            Assert::AreEqual(1, StrReverseFind("a/b/c", '/', 3));
            Assert::AreEqual(-1, StrReverseFind("abc", 'z'));
        }

        TEST_METHOD(ReverseFindAny)
        {
            Assert::AreEqual(3, StrReverseFindAny("a/b\\c", "/\\"));
            Assert::AreEqual(-1, StrReverseFindAny("abc", "/\\"));
        }

        // At index 0 the search starts at the terminator, as ZString's did
        TEST_METHOD(QuirkReverseFindMatchesTerminator)
        {
            Assert::AreEqual(3, StrReverseFind("abc", '\0'));
        }

        // ZString::ReverseFind(const ZString&) searched forwards
        TEST_METHOD(QuirkReverseFindStringSearchesForwards)
        {
            Assert::AreEqual(1, StrReverseFind("abcabc", ZString("bc")));
        }
    };

    TEST_CLASS(Pieces)
    {
    public:
        TEST_METHOD(Left)
        {
            AreEqualStr("he", StrLeft("hello", 2));
            AreEqualStr("", StrLeft("hello", 0));
            AreEqualStr("", StrLeft("hello", -1));
            AreEqualStr("hello", StrLeft("hello", 10));
        }

        TEST_METHOD(Right)
        {
            AreEqualStr("lo", StrRight("hello", 2));
            AreEqualStr("", StrRight("hello", 0));
            AreEqualStr("hello", StrRight("hello", 10));
        }

        TEST_METHOD(Middle)
        {
            AreEqualStr("ell", StrMiddle("hello", 1, 3));
            AreEqualStr("ello", StrMiddle("hello", 1, 10));
            AreEqualStr("", StrMiddle("hello", 5, 1));
            AreEqualStr("", StrMiddle("hello", -1, 2));
            AreEqualStr("", StrMiddle("hello", 0, 0));
        }

        TEST_METHOD(LeftOfAndRightOfCount)
        {
            AreEqualStr("hel", StrLeftOf("hello", 2));
            AreEqualStr("llo", StrRightOf("hello", 2));
            AreEqualStr("", StrLeftOf("hi", 5));
        }

        TEST_METHOD(LeftOfAndRightOfChars)
        {
            AreEqualStr("key", StrLeftOf("key=value", "="));
            AreEqualStr("value", StrRightOf("key=value", "="));
            AreEqualStr("", StrRightOf("key=", "="));
            AreEqualStr("", StrRightOf("novalue", "="));
        }

        // With no match LeftOf gives the delimiter set, as ZString::LeftOf did
        TEST_METHOD(QuirkLeftOfWithoutMatchGivesDelimiters)
        {
            AreEqualStr("=", StrLeftOf("novalue", "="));
        }
    };

    TEST_CLASS(Edits)
    {
    public:
        TEST_METHOD(Case)
        {
            AreEqualStr("mixed 123", StrToLower("MiXeD 123"));
            AreEqualStr("MIXED 123", StrToUpper("MiXeD 123"));
            AreEqualStr("\xC4", StrToLower("\xC4"));    // ASCII only

            char sz[16];
            ZToUpper(sz, "abc");
            AreEqualStr("ABC", sz);
            ZToLower(sz, "ABC");
            AreEqualStr("abc", sz);
        }

        TEST_METHOD(ReplaceRemoveSet)
        {
            ZString str = "a\\b\\c";
            StrReplaceAll(str, "\\", '/');
            AreEqualStr("a/b/c", str);

            StrRemoveAll(str, '/');
            AreEqualStr("abc", str);

            StrSetChar(str, 1, 'X');
            AreEqualStr("aXc", str);
            StrSetChar(str, 3, 'Y');
            StrSetChar(str, -1, 'Y');
            AreEqualStr("aXc", str);
        }

        TEST_METHOD(GetTokenSplitsOnSpaces)
        {
            ZString str = "first second  third";

            AreEqualStr("first", StrGetToken(str));
            AreEqualStr("second  third", str);
            AreEqualStr("second", StrGetToken(str));
            AreEqualStr("third", str);
            AreEqualStr("third", StrGetToken(str));
            AreEqualStr("", str);
            AreEqualStr("", StrGetToken(str));
        }

        TEST_METHOD(GetTokenQuoted)
        {
            ZString str = "\"two words\" rest";

            AreEqualStr("two words", StrGetToken(str));
            AreEqualStr("rest", str);

            // An unterminated quote takes the rest, quote included
            str = "\"open";
            AreEqualStr("\"open", StrGetToken(str));
            AreEqualStr("", str);
        }
    };

    TEST_CLASS(Scramble)
    {
    public:
        TEST_METHOD(RoundTrip)
        {
            ZString str = "secret password";
            ZString strScrambled = StrScramble(str, "Mic");

            Assert::AreEqual((int)str.size(), (int)strScrambled.size());
            Assert::IsTrue(str != strScrambled);
            AreEqualStr("secret password", StrUnscramble(strScrambled, "Mic"));
        }

        // The scrambled form is stored and sent, so the tables must not change
        TEST_METHOD(KnownVector)
        {
            AreEqualStr("\xBB\x7D\x15\x1D\x1A\x36\xCC\x0F\x89\x69", StrScramble("Allegiance", "key"));
            AreEqualStr("Allegiance", StrUnscramble("\xBB\x7D\x15\x1D\x1A\x36\xCC\x0F\x89\x69", "key"));
        }
    };
}
