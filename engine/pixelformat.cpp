#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

PixelFormat::PixelFormat(
    int   bits,
    DWORD redMask,
    DWORD greenMask,
    DWORD blueMask,
    DWORD alphaMask
) :
    m_bits(bits),
    m_redMask(redMask),
    m_greenMask(greenMask),
    m_blueMask(blueMask),
    m_alphaMask(alphaMask)
{
}

DWORD PixelFormat::RedSize()    const { return RedMask()   >> RedShift();   }
DWORD PixelFormat::GreenSize()  const { return GreenMask() >> GreenShift(); }
DWORD PixelFormat::BlueSize()   const { return BlueMask()  >> BlueShift();  }
DWORD PixelFormat::AlphaSize()  const { return AlphaMask() >> AlphaShift(); }

DWORD PixelFormat::RedShift()   const { return GetShift(RedMask());   }
DWORD PixelFormat::GreenShift() const { return GetShift(GreenMask()); }
DWORD PixelFormat::BlueShift()  const { return GetShift(BlueMask());  }
DWORD PixelFormat::AlphaShift() const { return GetShift(AlphaMask()); }

Pixel PixelFormat::MakePixel(DWORD red, DWORD green, DWORD blue) const
{
    return Pixel::Create(
          ((red   << RedShift()  ) & RedMask()  )
        | ((green << GreenShift()) & GreenMask())
        | ((blue  << BlueShift() ) & BlueMask() ));
}

Pixel PixelFormat::MakePixel(const Color& color) const
{
    return
        MakePixel(
            int(color.GetRed()   * RedSize()  ),
            int(color.GetGreen() * GreenSize()),
            int(color.GetBlue()  * BlueSize() )
        );

    // !!! this causes all of the artwork to change slightly
    //     since the rounding mode is different

    /*
    return
        MakePixel(
            MakeInt(color.GetRed()   * RedSize()  ),
            MakeInt(color.GetGreen() * GreenSize()),
            MakeInt(color.GetBlue()  * BlueSize() )
        );
    */
}

Color PixelFormat::MakeColor(Pixel pixel) const
{
    return
        Color(
            (float)((pixel.Value() &   RedMask()) >>   RedShift()) /   RedSize(),
            (float)((pixel.Value() & GreenMask()) >> GreenShift()) / GreenSize(),
            (float)((pixel.Value() &  BlueMask()) >>  BlueShift()) /  BlueSize()
        );
}

void PixelFormat::SetPixel(BYTE* pb, Pixel pixel) const
{
    switch (PixelBytes()) {
        case 1:         *pb = (BYTE)(pixel.Value()); break;
        case 2:  *(WORD*)pb = (WORD)(pixel.Value()); break;
        case 3:
            pb[0] = (BYTE)(((pixel.Value()) >>  0) & 0xff);
            pb[1] = (BYTE)(((pixel.Value()) >>  8) & 0xff);
            pb[2] = (BYTE)(((pixel.Value()) >> 16) & 0xff);
            break;
        case 4: *(DWORD*)pb = (pixel.Value()); break;
    }
}

Pixel PixelFormat::GetPixel(const BYTE* pb) const
{
    switch (PixelBytes()) {
        case 1: return Pixel::Create(*pb);
        case 2: return Pixel::Create(*(WORD*)pb);
        case 3:
            return
                Pixel::Create(
                      (DWORD(pb[0]) <<  0)
                    | (DWORD(pb[1]) <<  8)
                    | (DWORD(pb[2]) << 16)
                );
        case 4: return Pixel::Create(*(DWORD*)pb);
    }

    return Pixel::Create(0);
}

void PixelFormat::SetColor(BYTE* pb, const Color& color) const
{
    SetPixel(pb, MakePixel(color));
}

Color PixelFormat::GetColor(const BYTE* pb) const
{
    return MakeColor(GetPixel(pb));
}

bool PixelFormat::Is565() const
{
    return
           m_bits      == 16
        && m_redMask   == 0xf800
        && m_greenMask == 0x07e0
        && m_blueMask  == 0x001f;
}

bool PixelFormat::Equivalent(int bits, DWORD redMask, DWORD greenMask, DWORD blueMask, DWORD alphaMask) const
{
    // The DirectDraw-era version compared the green mask twice and never the
    // blue mask.
    return
           m_bits      == (DWORD)bits
        && m_redMask   == redMask
        && m_greenMask == greenMask
        && m_blueMask  == blueMask
        && m_alphaMask == alphaMask;
}
