#ifndef _d3d11engine_h_
#define _d3d11engine_h_

//////////////////////////////////////////////////////////////////////////////
//
// Private to the D3D11 backend (d3d11device.cpp, d3d11raster.cpp)
//
//////////////////////////////////////////////////////////////////////////////

#include <d3d11.h>
#include <dxgi1_2.h>

#include <vector>

//
// windows.h defines min and max as macros, which rules out std::min/max
//

template<class T> inline T MinOf(T a, T b) { return a < b ? a : b; }
template<class T> inline T MaxOf(T a, T b) { return a > b ? a : b; }

//
// Surface pixels <-> BGRA8. bColorKey puts the (black) colour key into alpha.
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

#endif
