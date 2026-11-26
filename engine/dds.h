#ifndef _dds_h_
#define _dds_h_

//////////////////////////////////////////////////////////////////////////////
//
// DDS files for the game's images: uncompressed 32-bit BGRA (DDS A8R8G8B8),
// optionally with a mip chain. See MODERNIZATION.md, "Textures: 32-bit DDS".
//
// Alpha is premultiplied (RGB never exceeds alpha; alpha 0 means RGB 0). The
// converted art has only alpha 0 and 255: the RGB565 colour key was baked into
// alpha 0, RGB 0, so the files say the same thing to the engine and to other
// tools. Soft alpha works wherever the image is colour keyed (see
// MODERNIZATION.md, "Soft alpha").
//
// They are the source art (Artwork/256, converted once from the original BMPs
// by tools/art/bmp2dds.py, which also rebuilds a model texture's mip levels),
// and the modeler reads them: an image namespace "<name>bmp" is loaded from
// <name>.dds.
//
//////////////////////////////////////////////////////////////////////////////

#include <vector>

struct DDSLevel {
    int         m_width;
    int         m_height;
    const BYTE* m_pdata;    // tightly packed rows, m_width * 4 bytes each
};

//
// Parse an in-memory DDS file. Fails (returns false) for anything but the
// uncompressed 32-bit BGRA layout above; the levels point into pdata.
//

bool ReadDDS(const BYTE* pdata, int size, std::vector<DDSLevel>& levels);

//
// The size of mip level n of a width x height image.
//

inline int DDSLevelSize(int size, int level)
{
    size >>= level;
    return size < 1 ? 1 : size;
}

#endif
