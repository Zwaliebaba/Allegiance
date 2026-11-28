//////////////////////////////////////////////////////////////////////////////
//
// GameCommon unit tests, run with Microsoft's CppUnitTest framework. See
// MODERNIZATION.md, "Unit tests".
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "GameCommon.Tests"

// The framework before zlib.h, whose assert and min/max macros it must not
// see. (The project also defines ZLIB_SKIP_NEW_REDEF: no debug new macro.)
#include "CppUnitTest.h"

#include <windows.h>
#include <mmsystem.h>

#include "guids.h"
#include "zlib.h"
#include "utility.h"
#include "igc.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// For assertion messages
inline std::wstring Wide(const std::string& str)
{
    return std::wstring(str.begin(), str.end());
}

// Strings compare as char*, so a failure prints both values
inline void AreEqualStr(const char* expected, const std::string& actual, const wchar_t* message = nullptr)
{
    Assert::AreEqual(expected, actual.c_str(), false, message);
}

#endif
