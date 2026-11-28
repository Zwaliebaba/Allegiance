#include "pch.h"
#include "StrManip.h"

//////////////////////////////////////////////////////////////////////////////
//
// TCLib's search and replace (TCLib/StrManip.h), which TCLib's relative
// paths use in place to turn '/' into '\'.
//
//////////////////////////////////////////////////////////////////////////////

namespace StrManipTests
{
    TEST_CLASS(ReplaceText)
    {
    public:
        TEST_METHOD(ReplacesEveryOccurrence)
        {
            char sz[32];
            int  n = 0;
            Assert::AreEqual(7, TCReplaceText("a/b/c", "/", "::", sz, sizeof(sz), &n));
            Assert::AreEqual("a::b::c", sz);
            Assert::AreEqual(2, n);
        }

        TEST_METHOD(InPlace)
        {
            char sz[] = "dir/sub/file.txt";
            Assert::AreEqual(16, TCReplaceText(sz, "/", "\\", sz, -1));
            Assert::AreEqual("dir\\sub\\file.txt", sz);
        }

        TEST_METHOD(InPlaceCannotGrow)
        {
            char sz[] = "a/b";
            Assert::AreEqual(-1, TCReplaceText(sz, "/", "//", sz, -1));
            Assert::AreEqual("a/b", sz);
        }

        TEST_METHOD(CountsWithoutADestination)
        {
            int n = 0;
            Assert::AreEqual(7, TCReplaceText("aXbXc", "X", "yy", NULL, 0, &n));
            Assert::AreEqual(2, n);
        }

        TEST_METHOD(TruncatesToTheBuffer)
        {
            // The return value is what the whole result needs
            char sz[4];
            Assert::AreEqual(11, TCReplaceText("hello world", "o", "0", sz, sizeof(sz)));
            Assert::AreEqual("hel", sz);
        }

        TEST_METHOD(EmptySearchReplacesEverything)
        {
            char sz[16];
            int  n = 0;
            Assert::AreEqual(3, TCReplaceText("anything", "", "new", sz, sizeof(sz), &n));
            Assert::AreEqual("new", sz);
            Assert::AreEqual(1, n);
        }

        TEST_METHOD(IgnoringCase)
        {
            char sz[32];
            int  n = 0;
            Assert::AreEqual(7, TCReplaceTextNoCase("Hello HELLO", "hello", "bye", sz, sizeof(sz), &n));
            Assert::AreEqual("bye bye", sz);
            Assert::AreEqual(2, n);
        }
    };
}
