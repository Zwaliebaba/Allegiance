#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Allocation
//
//////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    #undef new
    #undef delete

    void* ZAlloc(
       size_t s,
       const char* pszFile,
       int line
    ) {
        void* pv = ::operator new(s, _CLIENT_BLOCK, pszFile, line);
        return pv;
    }

    // ZAlloc allocates client blocks. (This used to read the block type from
    // a copy of the CRT's private block header, whose layout has changed.)
    void ZFree(void* pv)
    {
        if (pv != NULL) {
            _free_dbg(pv, _CLIENT_BLOCK);
        }
    }

#endif

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

DWORD CountBits(DWORD dw)
{
    int count = 0;

    while (dw != 0) {
        if (dw & 1) {
            count++;
        }

        dw = dw >> 1;
    }

    return count;
}

DWORD GetShift(DWORD dw)
{
    if (dw == 0) {
        return -1;
    }

    DWORD index = 0;

    while ((dw & 1) == 0) {
        index++;
        dw = dw >> 1;
    }

    return index;
}

DWORD NextPowerOf2(DWORD x)
{
    DWORD p = 1;

    while (p < x) {
        p *= 2;
        ZAssert(p != 0);
    }

    return p;
}

//////////////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////////////

float g_0      =   0.0f;
float g_1      =   1.0f;
float g_0_5    =   0.5f;
float g_255    = 255.0f;
float g_Inv255 = 1.0f / 255.0f;

