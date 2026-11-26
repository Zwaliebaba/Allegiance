#include "pch.h"
#include "dds.h"

//////////////////////////////////////////////////////////////////////////////
//
// The DDS header (the classic DirectX 9 layout, which every DDS reader takes;
// tools/art/bmp2dds.py writes it)
//
//////////////////////////////////////////////////////////////////////////////

namespace {

const DWORD c_ddsMagic          = 0x20534444;   // "DDS "

const DWORD c_DDSD_MIPMAPCOUNT    = 0x00020000;
const DWORD c_DDPF_RGB            = 0x00000040;

struct DDSPixelFormat {
    DWORD m_size;
    DWORD m_flags;
    DWORD m_fourCC;
    DWORD m_rgbBitCount;
    DWORD m_redMask;
    DWORD m_greenMask;
    DWORD m_blueMask;
    DWORD m_alphaMask;
};

struct DDSHeader {
    DWORD          m_size;
    DWORD          m_flags;
    DWORD          m_height;
    DWORD          m_width;
    DWORD          m_pitchOrLinearSize;
    DWORD          m_depth;
    DWORD          m_mipMapCount;
    DWORD          m_reserved1[11];
    DDSPixelFormat m_ddspf;
    DWORD          m_caps;
    DWORD          m_caps2;
    DWORD          m_caps3;
    DWORD          m_caps4;
    DWORD          m_reserved2;
};

static_assert(sizeof(DDSPixelFormat) == 32,  "DDS_PIXELFORMAT is 32 bytes");
static_assert(sizeof(DDSHeader)      == 124, "DDS_HEADER is 124 bytes");

// A8R8G8B8: BGRA in memory
const DWORD c_redMask   = 0x00ff0000;
const DWORD c_greenMask = 0x0000ff00;
const DWORD c_blueMask  = 0x000000ff;
const DWORD c_alphaMask = 0xff000000;

}

//////////////////////////////////////////////////////////////////////////////
//
// Read
//
//////////////////////////////////////////////////////////////////////////////

bool ReadDDS(const BYTE* pdata, int size, std::vector<DDSLevel>& levels)
{
    levels.clear();

    if (size < (int)(sizeof(DWORD) + sizeof(DDSHeader)) || *(const DWORD*)pdata != c_ddsMagic) {
        return false;
    }

    DDSHeader header;
    memcpy(&header, pdata + sizeof(DWORD), sizeof(header));

    const DDSPixelFormat& pf = header.m_ddspf;

    if (
           header.m_size        != sizeof(DDSHeader)
        || pf.m_size            != sizeof(DDSPixelFormat)
        || (pf.m_flags & c_DDPF_RGB) == 0
        || pf.m_rgbBitCount     != 32
        || pf.m_redMask         != c_redMask
        || pf.m_greenMask       != c_greenMask
        || pf.m_blueMask        != c_blueMask
        || pf.m_alphaMask       != c_alphaMask
        || header.m_width       == 0
        || header.m_height      == 0
        || header.m_width       >  16384
        || header.m_height      >  16384
    ) {
        return false;
    }

    int count = (header.m_flags & c_DDSD_MIPMAPCOUNT) && header.m_mipMapCount > 1 ? (int)header.m_mipMapCount : 1;
    if (count > 15) {
        return false;
    }

    const BYTE* p    = pdata + sizeof(DWORD) + sizeof(DDSHeader);
    const BYTE* pend = pdata + size;

    for (int level = 0; level < count; level++) {
        DDSLevel l;
        l.m_width  = DDSLevelSize(header.m_width,  level);
        l.m_height = DDSLevelSize(header.m_height, level);
        l.m_pdata  = p;

        p += (size_t)l.m_width * l.m_height * 4;
        if (p > pend) {
            levels.clear();
            return false;
        }

        levels.push_back(l);
    }

    return true;
}
