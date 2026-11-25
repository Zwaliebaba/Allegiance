#ifndef _pixelformat_h_
#define _pixelformat_h_

//////////////////////////////////////////////////////////////////////////////
//
// Pixel Formats
//
//////////////////////////////////////////////////////////////////////////////

class PixelFormat : public IObject {
private:
    DWORD m_bits;
    DWORD m_redMask;
    DWORD m_greenMask;
    DWORD m_blueMask;
    DWORD m_alphaMask;

public:
    PixelFormat(
        int   bits,
        DWORD redMask,
        DWORD greenMask,
        DWORD blueMask,
        DWORD alphaMask
    );

    DWORD PixelBits()  const { return m_bits;      }
    DWORD PixelBytes() const { return m_bits / 8;  }
    DWORD RedMask()    const { return m_redMask;   }
    DWORD GreenMask()  const { return m_greenMask; }
    DWORD BlueMask()   const { return m_blueMask;  }
    DWORD AlphaMask()  const { return m_alphaMask; }

    DWORD RedSize()    const;
    DWORD GreenSize()  const;
    DWORD BlueSize()   const;
    DWORD AlphaSize()  const;

    DWORD RedShift()   const;
    DWORD GreenShift() const;
    DWORD BlueShift()  const;
    DWORD AlphaShift() const;

    Pixel MakePixel(DWORD red, DWORD green, DWORD blue) const;
    Pixel MakePixel(const Color& color)                 const;
    Color MakeColor(Pixel pixel)                        const;
    void  SetPixel(BYTE* pb, Pixel pixel)               const;
    Pixel GetPixel(const BYTE* pb)                      const;
    void  SetColor(BYTE* pb, const Color& color)        const;
    Color GetColor(const BYTE* pb)                      const;

    bool  Is565()                                       const;

    bool Equivalent(int bits, DWORD redMask, DWORD greenMask, DWORD blueMask, DWORD alphaMask) const;
};

#endif
