#include "pch.h"
#include "BadWords.h"

//////////////////////////////////////////////////////////////////////////////
//
// The client's chat and name censor (ClientLib/BadWords.cpp). The word list
// comes from allbad.dll; BuildFilterAutomata takes its decoded form: a
// header, then each word ended by "\r\n", then a space.
//
//////////////////////////////////////////////////////////////////////////////

namespace BadWordsTests
{
    TEST_CLASS(Censor)
    {
    public:
        TEST_CLASS_INITIALIZE(LoadWords)
        {
            // The table keeps pointers into the buffer, so it lives as long as the tests
            static char s_szList[] = "header\r\ndarn\r\nheck\r\n ";
            BuildFilterAutomata(s_szList);
        }

        TEST_METHOD(CleanTextIsUnchanged)
        {
            AreEqualStr("hello there", std::string(CensorBadWords("hello there").c_str()));
        }

        TEST_METHOD(WordsAreObscuredWhateverTheirCase)
        {
            std::string str = CensorBadWords("Oh DARN it, heck").c_str();
            Assert::AreEqual((size_t)16, str.size());
            Assert::IsTrue(str.find("DARN") == std::string::npos && str.find("heck") == std::string::npos, Wide(str).c_str());
            AreEqualStr("Oh ", str.substr(0, 3));
            AreEqualStr(" it, ", str.substr(7, 5));
        }

        TEST_METHOD(EveryOccurrence)
        {
            std::string str = CensorBadWords("darndarn").c_str();
            Assert::IsTrue(str.find("darn") == std::string::npos, Wide(str).c_str());
        }

        TEST_METHOD(NonAsciiText)
        {
            // Characters above 0x7F index the lower-case table as unsigned
            std::string str = CensorBadWords("caf\xE9 darn \xFC").c_str();
            AreEqualStr("caf\xE9 ", str.substr(0, 5));
            AreEqualStr(" \xFC", str.substr(9));
            Assert::IsTrue(str.find("darn") == std::string::npos, Wide(str).c_str());
        }

        TEST_METHOD(TheCensorCanBeTurnedOff)
        {
            Assert::IsTrue(CensorDisplay());
            ToggleCensorDisplay();
            AreEqualStr("darn", std::string(CensorBadWords("darn").c_str()));
            ToggleCensorDisplay();
            Assert::IsTrue(CensorDisplay());
        }
    };
}
