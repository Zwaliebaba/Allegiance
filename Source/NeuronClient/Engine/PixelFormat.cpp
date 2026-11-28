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
    DWORD rgb =
          ((red   << RedShift()  ) & RedMask()  )
        | ((green << GreenShift()) & GreenMask())
        | ((blue  << BlueShift() ) & BlueMask() );

    //
    // In a format with alpha (the DDS images), black is the colour key, so it
    // gets alpha 0 and everything else is opaque: the rule the art build
    // bakes into the files, kept when code draws into such a surface.
    //

    return Pixel::Create(rgb == 0 ? 0 : rgb | AlphaMask());
}

Pixel PixelFormat::MakePixel(const Color& color) const
{
    //
    // In a format with alpha, a colour that 565 would have made black is the
    // colour key, as when everything was 565 (and as the art build keyed the
    // DDS images): transparent, alpha 0.
    //

    if (AlphaMask() != 0 && IsKeyBlack(color)) {
        return Pixel::Create(0);
    }

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

bool PixelFormat::IsKeyBlack(const Color& color)
{
    return
           int(color.GetRed()   * 31) == 0
        && int(color.GetGreen() * 63) == 0
        && int(color.GetBlue()  * 31) == 0;
}

//
// 8-bit BGRA, premultiplied like the DDS images, for converting between
// formats. A format without alpha is opaque except for the colour key (pixel
// 0); going back, alpha is dropped (premultiplied colour is what such a
// format would have shown).
//

DWORD PixelFormat::GetBGRA(Pixel pixel) const
{
    DWORD value = pixel.Value();

    if (
           m_bits      == 32
        && m_redMask   == 0x00ff0000
        && m_greenMask == 0x0000ff00
        && m_blueMask  == 0x000000ff
    ) {
        return m_alphaMask == 0xff000000 ? value : ((value & 0x00ffffff) == 0 ? 0 : (value | 0xff000000));
    }

    DWORD r = RedMask()   == 0 ? 0 : ((value & RedMask())   >> RedShift())   * 255 / RedSize();
    DWORD g = GreenMask() == 0 ? 0 : ((value & GreenMask()) >> GreenShift()) * 255 / GreenSize();
    DWORD b = BlueMask()  == 0 ? 0 : ((value & BlueMask())  >> BlueShift())  * 255 / BlueSize();
    DWORD a;

    if (AlphaMask() != 0) {
        a = ((value & AlphaMask()) >> AlphaShift()) * 255 / AlphaSize();
    } else {
        a = (value & (RedMask() | GreenMask() | BlueMask())) == 0 ? 0 : 255;
    }

    return (a << 24) | (r << 16) | (g << 8) | b;
}

Pixel PixelFormat::MakePixelBGRA(DWORD bgra) const
{
    if (
           m_bits      == 32
        && m_redMask   == 0x00ff0000
        && m_greenMask == 0x0000ff00
        && m_blueMask  == 0x000000ff
    ) {
        return Pixel::Create(m_alphaMask == 0xff000000 ? bgra : (bgra & 0x00ffffff));
    }

    DWORD a = (bgra >> 24) & 0xff;
    DWORD r = (bgra >> 16) & 0xff;
    DWORD g = (bgra >>  8) & 0xff;
    DWORD b =  bgra        & 0xff;

    DWORD value =
          (((r * RedSize()   + 127) / 255) << RedShift())
        | (((g * GreenSize() + 127) / 255) << GreenShift())
        | (((b * BlueSize()  + 127) / 255) << BlueShift());

    if (AlphaMask() != 0) {
        value |= ((a * AlphaSize() + 127) / 255) << AlphaShift();
    }

    return Pixel::Create(value);
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
