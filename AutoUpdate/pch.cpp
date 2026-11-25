#include "pch.h"

CComModule _Module;

// #include <atlimpl.cpp>

/////////////////////////////////////////////////////////////////////////////

void _debugf(const char* format, ...)
{
    // _TRUNCATE: a longer message is cut short, but always null-terminated
    const size_t size = 2048;
    char         bfr[size];

    va_list vl;
    va_start(vl, format);
    _vsnprintf_s(bfr, size, _TRUNCATE, format, vl);
    va_end(vl);

    // the text is already formatted: never pass it on as a format string
    debugf("%s", bfr);
}

