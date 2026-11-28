#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Engine Implementation
//
// The DirectDraw 4 engine enumerated display adapters and 16-bit modes,
// switched the display into exclusive fullscreen, and owned the primary
// surface and back buffer. Now every surface lives in system memory, the
// D3D11 device (created on first use) renders and presents, and "fullscreen"
// is a borderless window that covers the monitor, showing the game's chosen
// resolution scaled to fit (see EngineWindow).
//
//////////////////////////////////////////////////////////////////////////////

class EngineImpl : public PrivateEngine {
private:
    typedef TList<PrivateSurface*>  SurfaceList;

    //
    // State
    //

    bool                      m_bValid;
    bool                      m_bFullscreen;
    bool                      m_bAllowSecondary;
    bool                      m_bAllow3DAcceleration;
    bool                      m_b3DAccelerationImportant;

    TRef<PixelFormat>         m_ppf;
    HWND                      m_hwndFocus;
    WinPoint                  m_pointFullscreen;
    float                     m_gamma;

    TRef<D3D11Device>         m_pdevice;
    bool                      m_bDeviceFailed;

    //
    // Surface Cache
    //

    SurfaceList               m_listSurfaces;
    SurfaceList               m_listDeviceFormatSurfaces;
    std::vector<TRef<PixelFormat>>  m_ppfs;

public:
    EngineImpl(bool bAllow3DAcceleration, bool bAllowSecondary) :
        m_bValid(false),
        m_bFullscreen(false),
        m_bAllowSecondary(bAllowSecondary),
        m_bAllow3DAcceleration(bAllow3DAcceleration),
        m_b3DAccelerationImportant(false),
        m_hwndFocus(nullptr),
        m_pointFullscreen(640, 480),
        m_gamma(1.0f),
        m_bDeviceFailed(false)
    {
        //
        // 32-bit BGRA with premultiplied alpha, the format of the DDS images
        // (so drawing them into a CPU surface needs no conversion) and of the
        // GPU copies. See MODERNIZATION.md, "Soft alpha".
        //

        m_ppf = GetPixelFormat(32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }

private:
    ~EngineImpl()
    {
    }

public:
    bool IsValid() override
    {
        return true;
    }

    void Terminate() override
    {
        if (m_pdevice != nullptr && m_hwndFocus != nullptr) {
            m_pdevice->ReleaseSwapChain(m_hwndFocus);
        }

        m_pdevice = nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // D3D11 device
    //
    //////////////////////////////////////////////////////////////////////////////

    D3D11Device* GetD3D11Device() override
    {
        if (m_pdevice == nullptr && !m_bDeviceFailed) {
            m_pdevice = CreateD3D11Device();

            if (m_pdevice == nullptr) {
                m_bDeviceFailed = true;
            }
        }

        return m_pdevice;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Focus window
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetFocusWindow(Window* pwindow, bool bStartFullscreen) override
    {
        //
        // This function can only be called once
        //

        ZAssert(m_hwndFocus == NULL && pwindow->GetHWND() != NULL);
        ZAssert(!m_bValid);

        m_hwndFocus   = pwindow->GetHWND();
        m_bFullscreen = bStartFullscreen;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Gamma: applied by the presenter's pixel shader instead of a DirectDraw
    // gamma ramp, with the same curve (value^(1/gamma), black stays black).
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetGammaLevel(float value) override
    {
        m_gamma = bound(value, 1.0f, 2.0f);
    }

    float GetGammaLevel() override
    {
        return m_gamma;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Exception handler hook: nothing to undo without exclusive mode
    //
    //////////////////////////////////////////////////////////////////////////////

    void DebugSetWindowed() override
    {
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Set Attributes
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetAllowSecondary(bool bAllowSecondary) override
    {
        //
        // Secondary (3dfx-style add-on) adapters no longer exist as a
        // separate path; kept so preferences and menus still work.
        //

        if (m_bAllowSecondary != bAllowSecondary) {
            m_bAllowSecondary = bAllowSecondary;
            m_bValid          = false;
        }
    }

    void SetAllow3DAcceleration(bool bAllow3DAcceleration) override
    {
        //
        // Rendering always goes through D3D11 (hardware, or WARP when there
        // is no usable GPU). The flag is kept for the preferences and menus.
        //

        if (m_bAllow3DAcceleration != bAllow3DAcceleration) {
            m_bAllow3DAcceleration = bAllow3DAcceleration;
            m_bValid               = false;
        }
    }

    void Set3DAccelerationImportant(bool b3DAccelerationImportant) override
    {
        m_b3DAccelerationImportant = b3DAccelerationImportant;
    }

    void SetFullscreen(bool bFullscreen) override
    {
        if (m_bFullscreen != bFullscreen) {
            m_bFullscreen = bFullscreen;
            m_bValid      = false;
        }
    }

    void SetFullscreenSize(const WinPoint& pointIn) override
    {
        //
        // A size of zero (no saved preference) means the native one
        //

        WinPoint point = pointIn.X() <= 0 || pointIn.Y() <= 0 ? GetNativeSize() : pointIn;

        if (g_bWindowLog) {
            ZDebugOutput(("Engine::SetFullscreenSize(" + GetString(point) + ")\n").c_str());
        }

        if (m_pointFullscreen != point) {
            m_pointFullscreen = point;
            m_bValid          = false;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Fullscreen sizes: the original 4:3 mode list, limited to what fits on
    // the monitor the game window is on, plus the monitor's native size.
    // A native size wider than 21:9 is cut to 21:9 (the presenter
    // pillarboxes it), the widest view the camera widens for (Hor+).
    //
    //////////////////////////////////////////////////////////////////////////////

    WinPoint GetMonitorSize()
    {
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);

        HMONITOR hmonitor =
            m_hwndFocus != nullptr
                ? ::MonitorFromWindow(m_hwndFocus, MONITOR_DEFAULTTONEAREST)
                : ::MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

        if (::GetMonitorInfo(hmonitor, &mi)) {
            return
                WinPoint(
                    mi.rcMonitor.right  - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top
                );
        }

        return WinPoint(1600, 1200);
    }

    WinPoint GetNativeSize()
    {
        WinPoint size = GetMonitorSize();

        //
        // Even widths, so the pillarboxed frame centres on whole pixels
        //

        int widthMax = (size.Y() * 21 / 9) & ~1;

        return WinPoint(size.X() < widthMax ? size.X() : widthMax, size.Y());
    }

    void GetModes(std::vector<WinPoint>& modes)
    {
        static const WinPoint s_modes[] = {
            WinPoint( 640,  480),
            WinPoint( 800,  600),
            WinPoint(1024,  768),
            WinPoint(1280, 1024),
            WinPoint(1600, 1200)
        };

        WinPoint sizeMonitor = GetMonitorSize();
        WinPoint sizeNative  = GetNativeSize();

        for (int index = 0; index < ArrayCount(s_modes); index++) {
            if (
                   index == 0
                || (   s_modes[index].X() <= sizeMonitor.X()
                    && s_modes[index].Y() <= sizeMonitor.Y())
            ) {
                modes.push_back(s_modes[index]);
            }
        }

        //
        // The native size goes where it sorts by width, then height, as
        // ChangeFullscreenSize steps through the list in order
        //

        auto iter = modes.begin();

        while (
               iter != modes.end()
            && (   iter->X() < sizeNative.X()
                || (iter->X() == sizeNative.X() && iter->Y() < sizeNative.Y()))
        ) {
            ++iter;
        }

        if (iter == modes.end() || *iter != sizeNative) {
            modes.insert(iter, sizeNative);
        }
    }

    void ChangeFullscreenSize(bool bLarger) override
    {
        std::vector<WinPoint> modes;
        GetModes(modes);

        int count = (int)modes.size();
        WinPoint point = m_pointFullscreen;

        if (bLarger) {
            point = modes[count - 1];
            for (int index = 0; index < count; index++) {
                if (modes[index].X() > m_pointFullscreen.X() || modes[index].Y() > m_pointFullscreen.Y()) {
                    point = modes[index];
                    break;
                }
            }
        } else {
            point = modes[0];
            for (int index = count - 1; index >= 0; index--) {
                if (modes[index].X() < m_pointFullscreen.X() || modes[index].Y() < m_pointFullscreen.Y()) {
                    point = modes[index];
                    break;
                }
            }
        }

        SetFullscreenSize(point);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Get Attributes
    //
    //////////////////////////////////////////////////////////////////////////////

    bool GetAllowSecondary() override          { return m_bAllowSecondary;          }
    bool GetAllow3DAcceleration() override     { return m_bAllow3DAcceleration;     }
    bool Get3DAccelerationImportant() override { return m_b3DAccelerationImportant; }
    bool IsFullscreen() override               { return m_bFullscreen;              }
    bool PrimaryHas3DAcceleration() override   { return true;                       }
    bool GetUsing3DAcceleration() override     { return true;                       }

    const WinPoint& GetFullscreenSize() override
    {
        return m_pointFullscreen;
    }

    ZString GetDeviceName() override
    {
        D3D11Device* pdevice = GetD3D11Device();

        if (pdevice) {
            return pdevice->GetName();
        }

        return "no Direct3D 11 device";
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Device state: a window or size change is reported once as "changes",
    // so EngineWindow restyles itself and re-creates its render surface.
    //
    //////////////////////////////////////////////////////////////////////////////

    bool IsDeviceReady(bool& bChanges) override
    {
        bChanges = false;

        if (!m_bValid) {
            m_bValid = true;
            bChanges = true;
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Presentation
    //
    //////////////////////////////////////////////////////////////////////////////

    void Flip() override
    {
        //
        // There is no flipping chain any more; EngineWindow presents through
        // BltToWindow in both windowed and fullscreen mode.
        //
    }

    void BltToWindow(Window* pwindow, const WinPoint& point, Surface* psurface, const WinRect& rectSource) override
    {
        D3D11Device* pdevice = GetD3D11Device();

        if (pdevice) {
            PrivateSurface* pprivateSurface; CastTo(pprivateSurface, psurface);
            pdevice->Present(pwindow->GetHWND(), pprivateSurface, rectSource, m_gamma);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Pixel Format cache: surfaces compare formats by pointer, so equal
    // formats must be the same object.
    //
    //////////////////////////////////////////////////////////////////////////////

    TRef<PixelFormat> GetPixelFormat(
        int   bits,
        DWORD redMask,
        DWORD greenMask,
        DWORD blueMask,
        DWORD alphaMask
    ) override {
        int count = (int)m_ppfs.size();

        for(int index = 0; index < count; index++) {
            if (m_ppfs[index]->Equivalent(bits, redMask, greenMask, blueMask, alphaMask)) {
                return m_ppfs[index];
            }
        }

        TRef<PixelFormat> ppf = new PixelFormat(bits, redMask, greenMask, blueMask, alphaMask);

        m_ppfs.push_back(ppf);

        return ppf;
    }

    PixelFormat* GetPrimaryPixelFormat() override
    {
        return m_ppf;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Performance Counters
    //
    //////////////////////////////////////////////////////////////////////////////

    int GetTotalTextureMemory() override     { return GetTotalVideoMemory(); }
    int GetAvailableTextureMemory() override { return GetTotalVideoMemory(); }
    int GetAvailableVideoMemory() override   { return GetTotalVideoMemory(); }

    int GetTotalVideoMemory() override
    {
        return m_pdevice != nullptr ? m_pdevice->GetTotalVideoMemory() : 0;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Surface Cache
    //
    //////////////////////////////////////////////////////////////////////////////

    void RemovePrivateSurface(PrivateSurface* psurface) override
    {
        m_listDeviceFormatSurfaces.Remove(psurface);
        m_listSurfaces.Remove(psurface);

        if (m_pdevice != nullptr) {
            m_pdevice->RemoveSurface(psurface);
        }
    }

    TRef<PrivateSurface> AddSurface(PrivateSurface* psurface, bool bDevice)
    {
        m_listSurfaces.PushEnd(psurface);

        if (bDevice) {
            m_listDeviceFormatSurfaces.PushEnd(psurface);
        }

        return psurface;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Surface Constructors
    //
    //////////////////////////////////////////////////////////////////////////////

    static SurfaceType WithoutVideo(SurfaceType stype)
    {
        //
        // "Video" meant DirectDraw video memory; every surface is in system
        // memory now.
        //

        stype.Clear(SurfaceTypeVideo());
        return stype;
    }

    TRef<Surface> CreateSurface(
        const WinPoint& size,
        PixelFormat*    ppf,
        Palette*        ppalette,
        SurfaceType     stype,
        SurfaceSite*    psite
    ) override {
        ZAssert(ppalette == NULL);

        return
            AddSurface(
                CreatePrivateSurface(this, ppf, size, WithoutVideo(stype), psite),
                false
            );
    }

    TRef<Surface> CreateSurface(
        const WinPoint& size, 
        SurfaceType stype, 
        SurfaceSite* psite
    ) override {
        return 
            AddSurface(
                CreatePrivateSurface(this, m_ppf, size, WithoutVideo(stype), psite),
                true
            );
    }

    TRef<PrivateSurface> CreateCompatibleSurface(
        PrivateSurface* psurface, 
        const WinPoint& size,
        SurfaceType     stype, 
        SurfaceSite*    psite
    ) override {
        return
            AddSurface(
                CreatePrivateSurface(this, psurface->GetPixelFormat(), size, WithoutVideo(stype), psite),
                false
            );
    }

    TRef<Surface> CreateSurface(
    const WinPoint&    size,
          PixelFormat* ppf,
          Palette*     ppalette,
          int          pitch,
          BYTE*        pdata,
          IObject*     pobjectMemory
    ) override {
        ZAssert(ppalette == NULL);

        return
            AddSurface(
                CreatePrivateSurface(this, ppf, size, pitch, pdata, pobjectMemory),
                false
            );
    }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

TRef<Engine> CreateEngine(bool bAllow3DAcceleration, bool bAllowSecondary)
{
    return new EngineImpl(bAllow3DAcceleration, bAllowSecondary);
}
