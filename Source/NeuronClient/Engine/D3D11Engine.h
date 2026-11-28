#ifndef _d3d11engine_h_
#define _d3d11engine_h_

//////////////////////////////////////////////////////////////////////////////
//
// Private to the D3D11 backend (D3D11Device.cpp, D3D11Raster.cpp)
//
//////////////////////////////////////////////////////////////////////////////

#include <d3d11.h>
#include <dxgi1_3.h>      // IDXGISwapChain2 (frame latency waitable object)

#include <vector>

//
// windows.h defines min and max as macros, which rules out std::min/max
//

template<class T> inline T MinOf(T a, T b) { return a < b ? a : b; }
template<class T> inline T MaxOf(T a, T b) { return a > b ? a : b; }

//
// Surface pixels <-> BGRA8, premultiplied alpha. bColorKey keeps alpha (or,
// for a format without it, puts the black colour key into alpha); otherwise
// the result is opaque. Going back, an opaque black pixel becomes alpha 0.
//

void ConvertSurfaceToBGRA(
    const BYTE*     psource,
    int             pitchSource,
    PixelFormat*    ppf,
    const WinPoint& size,
    bool            bColorKey,
    DWORD*          pdest,
    int             pitchDest
);

//
// The colour key in BGRA: black once quantised to 565 (r < 8, g < 4, b < 8),
// the threshold the shaders use too
//

inline bool D3D11IsKeyBlack(DWORD bgra)
{
    return
           ((bgra >> 16) & 0xff) < 8
        && ((bgra >>  8) & 0xff) < 4
        && ( bgra        & 0xff) < 8;
}

void ConvertBGRAToSurface(
    const DWORD*    psource,
    int             pitchSource,
    PixelFormat*    ppf,
    const WinPoint& size,
    BYTE*           pdest,
    int             pitchDest
);

//
// Rasterizers share the immediate context and bind their own state lazily;
// anything else that changes context state (the presenter) calls this so the
// next draw rebinds everything.
//

void D3D11BoundRasterizerChanged();

//////////////////////////////////////////////////////////////////////////////
//
// GPU copy of a surface the GPU draws into (the window frame and 3D panes).
// The surface owns it and decides when it is current (Surface.cpp); the
// functions below create, fill and draw into it (D3D11Draw2D.cpp).
//
//////////////////////////////////////////////////////////////////////////////

class D3D11Target : public IObject {
public:
    TRef<ID3D11Texture2D>          m_ptexture;     // BGRA8, render target and shader resource
    TRef<ID3D11RenderTargetView>   m_prtv;
    TRef<ID3D11ShaderResourceView> m_psrv;
    TRef<ID3D11Texture2D>          m_pdepth;       // D24S8, created on demand
    TRef<ID3D11DepthStencilView>   m_pdsv;
    TRef<ID3D11Texture2D>          m_pstaging;     // created on the first readback
    WinPoint                       m_size;
    int                            m_generation;

    D3D11Target() :
        m_size(0, 0),
        m_generation(-1)
    {
    }

    bool IsValid(D3D11Device* pdevice, const WinPoint& size);
};

//
// (Re)create the colour texture if the device or size changed. Returns false
// if the target can't be created. Afterwards bRecreated says whether the
// contents are new (undefined).
//

bool D3D11EnsureTarget(D3D11Device* pdevice, D3D11Target* ptarget, const WinPoint& size, bool& bRecreated);
bool D3D11EnsureDepth(D3D11Device* pdevice, D3D11Target* ptarget);

//
// CPU pixels <-> GPU copy
//

void D3D11Upload(D3D11Device* pdevice, D3D11Target* ptarget, const BYTE* psource, int pitch, PixelFormat* ppf);
bool D3D11Readback(D3D11Device* pdevice, D3D11Target* ptarget, BYTE* pdest, int pitch, PixelFormat* ppf);

//
// 2D drawing into a target. Rects are in pixels, y down, already clipped;
// rectScissor limits what is touched.
//
// Colour keys: D3D11KeyAlpha for texture-cache textures (the key is alpha 0),
// D3D11KeyBlack for another target used as a source (pixels that were black
// in 565, the only key the game uses).
//

enum D3D11KeyMode {
    D3D11KeyNone  = 0,
    D3D11KeyAlpha = 1,
    D3D11KeyBlack = 2
};

void D3D11Blit(
    D3D11Device*              pdevice,
    D3D11Target*              ptarget,
    const WinRect&            rectDest,
    ID3D11ShaderResourceView* psrv,
    const WinPoint&           sizeSource,
    const WinRect&            rectSource,
    D3D11KeyMode              keyMode,
    const WinRect&            rectScissor
);

void D3D11Fill(D3D11Device* pdevice, D3D11Target* ptarget, const WinRect& rect, const Color& color);

//
// Text: one quad per glyph cell from a font's atlas (R8, set bits 255)
//

struct D3D11Glyph {
    WinRect m_rectDest;     // cell on the target
    WinRect m_rectAtlas;    // the same-sized cell in the atlas
    DWORD   m_color;        // BGRA
};

TRef<ID3D11ShaderResourceView> D3D11CreateAtlas(D3D11Device* pdevice, const BYTE* pdata, const WinPoint& size);

void D3D11DrawGlyphs(
    D3D11Device*              pdevice,
    D3D11Target*              ptarget,
    ID3D11ShaderResourceView* patlas,
    const WinPoint&           sizeAtlas,
    const D3D11Glyph*         pglyphs,
    int                       count,
    const WinRect&            rectScissor
);

DWORD D3D11MakeBGRA(const Color& color);

//
// Per-frame counters for the frame-rate overlay
//

struct D3D11Stats {
    int m_uploads;          // CPU pixels copied to a render target
    int m_readbacks;        // render target copied back to CPU pixels
    int m_textureUploads;   // texture cache refreshes
    int m_draws2D;
    int m_draws3D;
    int m_scenes;           // 3D layers drawn through an HDR scene target
};

extern D3D11Stats g_d3d11Stats;

#endif
