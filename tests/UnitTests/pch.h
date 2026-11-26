//////////////////////////////////////////////////////////////////////////////
//
// Unit tests: zlib (and the engine headers that need only zlib), run with
// Microsoft's CppUnitTest framework. See MODERNIZATION.md, "Unit tests".
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "UnitTests"

// The framework before zlib.h, whose assert and min/max macros it must not
// see. (The project also defines ZLIB_SKIP_NEW_REDEF: no debug new macro.)
#include "CppUnitTest.h"

#include "windows.h"
#include "zlib.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Strings compare as char*, so a failure prints both values
inline void AreEqualStr(const char* expected, const std::string& actual, const wchar_t* message = nullptr)
{
    Assert::AreEqual(expected, actual.c_str(), false, message);
}

#endif
