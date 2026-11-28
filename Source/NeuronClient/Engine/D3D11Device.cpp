#include "pch.h"
#include "D3D11Engine.h"
#include "AlphaCoverage.h"
#include "Dds.h"

#include "PresentVs.h"
#include "PresentPs.h"

#include <unordered_map>

//////////////////////////////////////////////////////////////////////////////
//
// D3D11Device
//
// Owns the Direct3D 11 device, the texture cache (GPU copies of system-memory
// surfaces) and one flip-model swap chain per window. Replaces DDDevice,
// D3DDevice and the DirectDraw primary surface, clipper and gamma ramp.
//
//////////////////////////////////////////////////////////////////////////////

namespace {

ZString NarrowString(const wchar_t* pwsz)
{
    char sz[256];
    int  count = ::WideCharToMultiByte(CP_ACP, 0, pwsz, -1, sz, sizeof(sz), NULL, NULL);

    if (count <= 0) {
        return ZString();
    }

    return ZString(sz);
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
//
// Pixel conversion shared by the texture cache, the rasterizer and the
// presenter. Surfaces are 32-bit premultiplied BGRA almost always (the DDS
// images and the CPU surfaces); anything else goes through PixelFormat's
// per-pixel path.
//
//////////////////////////////////////////////////////////////////////////////

void ConvertSurfaceToBGRA(
    const BYTE*  psource,
    int          pitchSource,
    PixelFormat* ppf,
    const WinPoint& size,
    bool         bColorKey,
    DWORD*       pdest,
    int          pitchDest
) {
    for (int y = 0; y < size.Y(); y++) {
        const BYTE* ps = psource + y * pitchSource;
        DWORD*      pd = (DWORD*)((BYTE*)pdest + y * pitchDest);

        if (ppf->Is565()) {
            const WORD* pw = (const WORD*)ps;

            for (int x = 0; x < size.X(); x++) {
                DWORD w = pw[x];
                DWORD r = (w >> 11) & 0x1f;
                DWORD g = (w >>  5) & 0x3f;
                DWORD b =  w        & 0x1f;

                //
                // Replicate the high bits so full intensity stays 255
                //

                DWORD a = (bColorKey && w == 0) ? 0 : 0xff;

                pd[x] =
                      (a << 24)
                    | (((r << 3) | (r >> 2)) << 16)
                    | (((g << 2) | (g >> 4)) <<  8)
                    |  ((b << 3) | (b >> 2));
            }
        } else if (
               ppf->PixelBytes() == 4
            && ppf->RedMask()    == 0x00ff0000
            && ppf->GreenMask()  == 0x0000ff00
            && ppf->BlueMask()   == 0x000000ff
        ) {
            //
            // 32-bit BGRA: DDS images and CPU surfaces, premultiplied alpha
            // (alpha 0 is the colour key), and alpha-less 32-bit surfaces,
            // where black is the key. Alpha is used only when the surface is
            // colour keyed (as an MDL file asks with ImportImage); otherwise
            // the image is opaque, as it was before DDS.
            //

            const DWORD* pw        = (const DWORD*)ps;
            DWORD        alphaMask = ppf->AlphaMask();

            for (int x = 0; x < size.X(); x++) {
                DWORD p   = pw[x];
                DWORD rgb = p & 0x00ffffff;
                DWORD a;

                if (!bColorKey) {
                    a = 0xff000000;
                } else if (alphaMask != 0) {
                    a = p & 0xff000000;
                } else {
                    a = rgb == 0 ? 0 : 0xff000000;
                }

                pd[x] = a | rgb;
            }
        } else {
            int bytes = ppf->PixelBytes();

            for (int x = 0; x < size.X(); x++) {
                DWORD bgra = ppf->GetBGRA(ppf->GetPixel(ps + x * bytes));
                pd[x] = bColorKey ? bgra : (bgra | 0xff000000);
            }
        }
    }
}

void ConvertBGRAToSurface(
    const DWORD* psource,
    int          pitchSource,
    PixelFormat* ppf,
    const WinPoint& size,
    BYTE*        pdest,
    int          pitchDest
) {
    for (int y = 0; y < size.Y(); y++) {
        const DWORD* ps = (const DWORD*)((const BYTE*)psource + y * pitchSource);
        BYTE*        pd = pdest + y * pitchDest;

        if (ppf->Is565()) {
            WORD* pw = (WORD*)pd;

            for (int x = 0; x < size.X(); x++) {
                DWORD c = ps[x];
                pw[x] = (WORD)(
                      ((c >> 8) & 0xf800)
                    | ((c >> 5) & 0x07e0)
                    | ((c >> 3) & 0x001f)
                );
            }
        } else if (
               ppf->PixelBytes() == 4
            && ppf->RedMask()    == 0x00ff0000
            && ppf->GreenMask()  == 0x0000ff00
            && ppf->BlueMask()   == 0x000000ff
        ) {
            //
            // A render target's alpha is coverage, premultiplied; an opaque
            // pixel that is black (in 565 terms) is the colour key of the
            // pane, so it comes back as alpha 0, as pixels drawn black on
            // the CPU are (PixelFormat::MakePixel)
            //

            DWORD* pw        = (DWORD*)pd;
            bool   bAlpha    = ppf->AlphaMask() == 0xff000000;

            for (int x = 0; x < size.X(); x++) {
                DWORD c = ps[x];

                if (!bAlpha) {
                    pw[x] = c & 0x00ffffff;
                } else if ((c >> 24) == 0xff && D3D11IsKeyBlack(c)) {
                    pw[x] = 0;
                } else {
                    pw[x] = c;
                }
            }
        } else {
            int bytes = ppf->PixelBytes();

            for (int x = 0; x < size.X(); x++) {
                ppf->SetPixel(pd + x * bytes, ppf->MakePixelBGRA(ps[x]));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// D3D11DeviceImpl
//
//////////////////////////////////////////////////////////////////////////////

class D3D11DeviceImpl : public D3D11Device {
private:
    struct TextureEntry {
        TRef<ID3D11Texture2D>          m_ptexture;
        TRef<ID3D11ShaderResourceView> m_psrv;
        WinPoint                       m_size;
        int                            m_id;
        bool                           m_bColorKey;
        int                            m_mipLevels;
        bool                           m_bTranslucent;
    };

    struct SwapChainEntry {
        TRef<IDXGISwapChain1>          m_pswapChain;
        TRef<ID3D11RenderTargetView>   m_prtv;
        WinPoint                       m_size;
        UINT                           m_flags;
        HANDLE                         m_hlatency; // frame latency waitable object, owned by the swap chain

        SwapChainEntry() :
            m_flags(0),
            m_hlatency(NULL)
        {
        }

        ~SwapChainEntry()
        {
            if (m_hlatency != NULL) {
                ::CloseHandle(m_hlatency);
            }
        }

        SwapChainEntry(const SwapChainEntry&) = delete;
        SwapChainEntry& operator=(const SwapChainEntry&) = delete;
    };

    TRef<ID3D11Device>               m_pdevice;
    TRef<ID3D11DeviceContext>        m_pcontext;
    TRef<IDXGIFactory2>              m_pfactory;
    ZString                          m_strName;
    bool                             m_bHardware;
    int                              m_generation;
    int                              m_videoMemory;

    std::unordered_map<PrivateSurface*, TextureEntry> m_textures;
    std::unordered_map<HWND, SwapChainEntry>          m_swapChains;

    //
    // Presentation pipeline
    //

    TRef<ID3D11VertexShader>         m_pvsPresent;
    TRef<ID3D11PixelShader>          m_ppsPresent;
    TRef<ID3D11Buffer>               m_pcbPresent;
    TRef<ID3D11SamplerState>         m_psamplerPresent;
    TRef<ID3D11RasterizerState>      m_prasterPresent;
    TRef<ID3D11BlendState>           m_pblendPresent;
    TRef<ID3D11DepthStencilState>    m_pdepthPresent;

    std::vector<DWORD>               m_vconvert;

public:
    D3D11DeviceImpl() :
        m_bHardware(false),
        m_generation(0),
        m_videoMemory(0)
    {
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Creation: a hardware device if there is one, else WARP
    //
    //////////////////////////////////////////////////////////////////////////////

    bool CreateDeviceObjects()
    {
        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        m_pdevice  = NULL;
        m_pcontext = NULL;
        m_pfactory = NULL;

        HRESULT hr = E_FAIL;
        D3D_FEATURE_LEVEL level;

        for (int pass = 0; pass < 2 && FAILED(hr); pass++) {
            D3D_DRIVER_TYPE type = pass == 0 ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;

            //
            // D3D11.1 runtimes reject the 11_1 level on older drivers with
            // E_INVALIDARG; retry without it.
            //

            for (int first = 0; first < 2 && FAILED(hr); first++) {
                hr = D3D11CreateDevice(
                    NULL,
                    type,
                    NULL,
                    0,
                    levels + first,
                    ArrayCount(levels) - first,
                    D3D11_SDK_VERSION,
                    &m_pdevice,
                    &level,
                    &m_pcontext
                );
            }

            m_bHardware = (pass == 0);
        }

        if (FAILED(hr)) {
            return false;
        }

        //
        // Adapter name and memory, and the factory for swap chains
        //

        TRef<IDXGIDevice>  pdxgiDevice;
        TRef<IDXGIAdapter> padapter;

        if (
               SUCCEEDED(m_pdevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pdxgiDevice))
            && SUCCEEDED(pdxgiDevice->GetAdapter(&padapter))
        ) {
            DXGI_ADAPTER_DESC desc;

            if (SUCCEEDED(padapter->GetDesc(&desc))) {
                m_strName     = NarrowString(desc.Description);
                m_videoMemory = (int)MinOf(desc.DedicatedVideoMemory, (SIZE_T)0x7fffffff);
            }

            padapter->GetParent(__uuidof(IDXGIFactory2), (void**)&m_pfactory);
        }

        if (!m_bHardware) {
            m_strName += " (WARP)";
        }

        return m_pfactory != NULL && CreatePresentObjects();
    }

    bool CreatePresentObjects()
    {
        if (FAILED(m_pdevice->CreateVertexShader(g_PresentVs, sizeof(g_PresentVs), NULL, &m_pvsPresent))) return false;
        if (FAILED(m_pdevice->CreatePixelShader (g_PresentPs, sizeof(g_PresentPs), NULL, &m_ppsPresent))) return false;

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = 3 * 4 * sizeof(float);
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_pdevice->CreateBuffer(&bd, NULL, &m_pcbPresent))) return false;

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        if (FAILED(m_pdevice->CreateSamplerState(&sd, &m_psamplerPresent))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode        = D3D11_FILL_SOLID;
        rd.CullMode        = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        if (FAILED(m_pdevice->CreateRasterizerState(&rd, &m_prasterPresent))) return false;

        D3D11_BLEND_DESC bld = {};
        bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(m_pdevice->CreateBlendState(&bld, &m_pblendPresent))) return false;

        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = FALSE;
        if (FAILED(m_pdevice->CreateDepthStencilState(&dsd, &m_pdepthPresent))) return false;

        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Device removal: re-create everything. Surfaces keep their pixels in
    // system memory, so nothing is lost but the GPU copies.
    //
    //////////////////////////////////////////////////////////////////////////////

    bool CheckDeviceLost(long hr) override // true if the device was lost and re-created
    {
        if (
               hr == DXGI_ERROR_DEVICE_REMOVED
            || hr == DXGI_ERROR_DEVICE_RESET
            || hr == DXGI_ERROR_DEVICE_HUNG
            || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
        ) {
            m_textures.clear();
            m_swapChains.clear();

            m_pvsPresent      = NULL;
            m_ppsPresent      = NULL;
            m_pcbPresent      = NULL;
            m_psamplerPresent = NULL;
            m_prasterPresent  = NULL;
            m_pblendPresent   = NULL;
            m_pdepthPresent   = NULL;

            if (m_pcontext) {
                m_pcontext->ClearState();
                m_pcontext->Flush();
            }

            m_generation++;
            CreateDeviceObjects();
            return true;
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Attributes
    //
    //////////////////////////////////////////////////////////////////////////////

    bool IsValid() override                     { return m_pdevice != NULL; }
    ID3D11Device*        GetDevice() override   { return m_pdevice;         }
    ID3D11DeviceContext* GetContext() override  { return m_pcontext;        }
    ZString GetName() override                  { return m_strName;         }
    bool IsHardware() override                  { return m_bHardware;       }
    int  GetGeneration() override               { return m_generation;      }
    int  GetTotalVideoMemory() override         { return m_videoMemory;     }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Texture cache. Textures are BGRA8, premultiplied alpha, as the DDS
    // images are; a colour keyed surface keeps its alpha (alpha 0 is the
    // key), anything else is opaque. The DirectX 6 path capped textures at
    // 256x256 and halved anything larger; full size is kept now.
    //
    //////////////////////////////////////////////////////////////////////////////

    ID3D11ShaderResourceView* GetTexture(PrivateSurface* psurface) override
    {
        if (m_pdevice == NULL) {
            return NULL;
        }

        TextureEntry& entry     = m_textures[psurface];
        WinPoint      size      = psurface->GetSize();
        bool          bColorKey = psurface->HasColorKey();
        int           mipLevels = psurface->GetMipLevelCount();

        if (
               entry.m_psrv != NULL
            && entry.m_id == psurface->GetChangeID()
            && entry.m_bColorKey == bColorKey
            && entry.m_size == size
            && entry.m_mipLevels == mipLevels
        ) {
            return entry.m_psrv;
        }

        if (size.X() <= 0 || size.Y() <= 0) {
            return NULL;
        }

        //
        // Convert to BGRA: the surface, and the mip levels below it that the
        // art build made (model textures from DDS files)
        //

        g_d3d11Stats.m_textureUploads++;

        m_vconvert.resize(size.X() * size.Y());

        ConvertSurfaceToBGRA(
            psurface->GetPointer(),
            psurface->GetPitch(),
            psurface->GetPixelFormat(),
            size,
            bColorKey,
            m_vconvert.data(),
            size.X() * 4
        );
        psurface->ReleasePointer();

        std::vector<std::vector<DWORD>> vmips(mipLevels - 1);

        for (int level = 1; level < mipLevels; level++) {
            WinPoint sizeLevel(DDSLevelSize(size.X(), level), DDSLevelSize(size.Y(), level));
            vmips[level - 1].resize(sizeLevel.X() * sizeLevel.Y());

            ConvertSurfaceToBGRA(
                psurface->GetMipLevel(level),
                sizeLevel.X() * psurface->GetPixelFormat()->PixelBytes(),
                psurface->GetPixelFormat(),
                sizeLevel,
                bColorKey,
                vmips[level - 1].data(),
                sizeLevel.X() * 4
            );
        }

        //
        // A cut-out's mip levels keep its coverage (AlphaCoverage.h)
        //

        if (bColorKey) {
            AlphaCoverage::PreserveCoverage(m_vconvert.data(), size.X() * size.Y(), vmips);
        }

        entry.m_bTranslucent =
               bColorKey
            && AlphaCoverage::Classify(m_vconvert.data(), size.X() * size.Y()) == AlphaCoverage::Kind::Translucent;

        if (entry.m_ptexture != NULL && entry.m_size == size && entry.m_mipLevels == mipLevels && mipLevels == 1) {
            m_pcontext->UpdateSubresource(entry.m_ptexture, 0, NULL, m_vconvert.data(), size.X() * 4, 0);
        } else {
            D3D11_TEXTURE2D_DESC td = {};
            td.Width            = size.X();
            td.Height           = size.Y();
            td.MipLevels        = mipLevels;
            td.ArraySize        = 1;
            td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage            = D3D11_USAGE_DEFAULT;
            td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

            std::vector<D3D11_SUBRESOURCE_DATA> vdata(mipLevels);
            vdata[0].pSysMem     = m_vconvert.data();
            vdata[0].SysMemPitch = size.X() * 4;

            for (int level = 1; level < mipLevels; level++) {
                vdata[level].pSysMem     = vmips[level - 1].data();
                vdata[level].SysMemPitch = DDSLevelSize(size.X(), level) * 4;
            }

            entry.m_ptexture = NULL;
            entry.m_psrv     = NULL;

            HRESULT hr = m_pdevice->CreateTexture2D(&td, vdata.data(), &entry.m_ptexture);

            if (FAILED(hr)) {
                CheckDeviceLost(hr);
                m_textures.erase(psurface);
                return NULL;
            }

            hr = m_pdevice->CreateShaderResourceView(entry.m_ptexture, NULL, &entry.m_psrv);

            if (FAILED(hr)) {
                m_textures.erase(psurface);
                return NULL;
            }
        }

        entry.m_mipLevels = mipLevels;
        entry.m_size      = size;
        entry.m_id        = psurface->GetChangeID();
        entry.m_bColorKey = bColorKey;

        return entry.m_psrv;
    }

    bool IsTranslucent(PrivateSurface* psurface) override
    {
        return GetTexture(psurface) != NULL && m_textures[psurface].m_bTranslucent;
    }

    void RemoveSurface(PrivateSurface* psurface) override
    {
        m_textures.erase(psurface);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Presentation
    //
    //////////////////////////////////////////////////////////////////////////////

    SwapChainEntry* GetSwapChain(HWND hwnd, const WinPoint& size)
    {
        SwapChainEntry& entry = m_swapChains[hwnd];

        if (entry.m_pswapChain == NULL) {
            DXGI_SWAP_CHAIN_DESC1 desc = {};
            desc.Width            = size.X();
            desc.Height           = size.Y();
            desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount      = 2;
            desc.Scaling          = DXGI_SCALING_STRETCH;
            desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;

            //
            // A frame latency of one: the game waits after each present until
            // that frame is on its way to the screen, so input is read at most
            // one frame before it is shown (the default queue is three)
            //

            desc.Flags            = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            HRESULT hr = m_pfactory->CreateSwapChainForHwnd(m_pdevice, hwnd, &desc, NULL, NULL, &entry.m_pswapChain);

            if (FAILED(hr)) {
                //
                // Flip-model swap chains need Windows 10; fall back to the
                // blt model, which has no waitable object.
                //

                desc.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
                desc.BufferCount = 1;
                desc.Flags       = 0;
                hr = m_pfactory->CreateSwapChainForHwnd(m_pdevice, hwnd, &desc, NULL, NULL, &entry.m_pswapChain);
            }

            if (SUCCEEDED(hr) && desc.Flags != 0) {
                TRef<IDXGISwapChain2> pswapChain2;

                if (SUCCEEDED(entry.m_pswapChain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&pswapChain2))) {
                    pswapChain2->SetMaximumFrameLatency(1);
                    entry.m_hlatency = pswapChain2->GetFrameLatencyWaitableObject();
                }
            }

            entry.m_flags = SUCCEEDED(hr) ? desc.Flags : 0;

            if (FAILED(hr)) {
                CheckDeviceLost(hr);
                m_swapChains.erase(hwnd);
                return NULL;
            }

            //
            // The engine handles Alt+Enter itself (EngineWindow)
            //

            m_pfactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
            entry.m_size = size;
        } else if (entry.m_size != size) {
            entry.m_prtv = NULL;
            m_pcontext->OMSetRenderTargets(0, NULL, NULL);

            HRESULT hr = entry.m_pswapChain->ResizeBuffers(0, size.X(), size.Y(), DXGI_FORMAT_UNKNOWN, entry.m_flags);

            if (FAILED(hr)) {
                CheckDeviceLost(hr);
                m_swapChains.erase(hwnd);
                return NULL;
            }

            entry.m_size = size;
        }

        if (entry.m_prtv == NULL) {
            TRef<ID3D11Texture2D> pbackBuffer;

            if (
                   FAILED(entry.m_pswapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pbackBuffer))
                || FAILED(m_pdevice->CreateRenderTargetView(pbackBuffer, NULL, &entry.m_prtv))
            ) {
                m_swapChains.erase(hwnd);
                return NULL;
            }
        }

        return &entry;
    }

    void ReleaseSwapChain(HWND hwnd) override
    {
        m_swapChains.erase(hwnd);
    }

    void Present(HWND hwnd, PrivateSurface* psurface, const WinRect& rectSource, float gamma) override
    {
        if (m_pdevice == NULL || psurface == NULL || ::IsIconic(hwnd)) {
            return;
        }

        RECT rc;
        ::GetClientRect(hwnd, &rc);
        WinPoint sizeClient(rc.right - rc.left, rc.bottom - rc.top);

        if (sizeClient.X() <= 0 || sizeClient.Y() <= 0 || rectSource.IsEmpty()) {
            return;
        }

        SwapChainEntry* pentry = GetSwapChain(hwnd, sizeClient);

        if (pentry == NULL) {
            return;
        }

        //
        // The frame is a GPU surface: draw its GPU copy. The first present
        // gives it one, so from then on its 2D drawing is done on the GPU too.
        //

        ID3D11ShaderResourceView* psrv = NULL;

        if (psurface->MakeGpuResident()) {
            psrv = psurface->GetGpuSource();
        }

        if (psrv == NULL) {
            psrv = GetTexture(psurface);
        }

        if (psrv == NULL) {
            return;
        }

        //
        // Scale the source rectangle to fit the client area, keeping its
        // aspect ratio, and centre it. Windowed the two are the same size,
        // so this is a 1:1 copy.
        //

        float scale =
            MinOf(
                (float)sizeClient.X() / rectSource.XSize(),
                (float)sizeClient.Y() / rectSource.YSize()
            );

        float width  = rectSource.XSize() * scale;
        float height = rectSource.YSize() * scale;

        D3D11_VIEWPORT vp;
        vp.TopLeftX = floorf((sizeClient.X() - width ) / 2);
        vp.TopLeftY = floorf((sizeClient.Y() - height) / 2);
        vp.Width    = width;
        vp.Height   = height;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;

        //
        // Constants
        //

        WinPoint sizeSurface = psurface->GetSize();

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_pcontext->Map(m_pcbPresent, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* pf = (float*)mapped.pData;
            pf[0] = (float)rectSource.XMin() / sizeSurface.X();
            pf[1] = (float)rectSource.YMin() / sizeSurface.Y();
            pf[2] = (float)rectSource.XMax() / sizeSurface.X();
            pf[3] = (float)rectSource.YMax() / sizeSurface.Y();
            pf[4] = 1.0f / MaxOf(gamma, 0.01f);
            pf[5] = 0;
            pf[6] = 0;
            pf[7] = 0;

            //
            // Sharp scaling: the frame is scaled up by the largest whole
            // factor with square pixels, and only the rest is filtered,
            // so pillarboxed 800x600 screens stay crisp at any size
            //

            pf[8]  = (float)sizeSurface.X();
            pf[9]  = (float)sizeSurface.Y();
            pf[10] = MaxOf(1.0f, floorf(scale));
            pf[11] = 0;
            m_pcontext->Unmap(m_pcbPresent, 0);
        }

        //
        // Draw
        //

        static const float black[4] = { 0, 0, 0, 1 };

        ID3D11RenderTargetView* prtv = pentry->m_prtv;
        m_pcontext->ClearRenderTargetView(prtv, black);
        m_pcontext->OMSetRenderTargets(1, &prtv, NULL);
        m_pcontext->OMSetBlendState(m_pblendPresent, NULL, 0xffffffff);
        m_pcontext->OMSetDepthStencilState(m_pdepthPresent, 0);
        m_pcontext->RSSetState(m_prasterPresent);
        m_pcontext->RSSetViewports(1, &vp);
        m_pcontext->IASetInputLayout(NULL);
        m_pcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        //
        // Note: TRef's operator& releases (it is for out parameters), so
        // input arrays go through plain pointers.
        //

        ID3D11Buffer*       pcb      = m_pcbPresent;
        ID3D11SamplerState* psampler = m_psamplerPresent;

        m_pcontext->VSSetShader(m_pvsPresent, NULL, 0);
        m_pcontext->VSSetConstantBuffers(0, 1, &pcb);
        m_pcontext->PSSetShader(m_ppsPresent, NULL, 0);
        m_pcontext->PSSetConstantBuffers(0, 1, &pcb);
        m_pcontext->PSSetShaderResources(0, 1, &psrv);
        m_pcontext->PSSetSamplers(0, 1, &psampler);
        m_pcontext->Draw(3, 0);

        ID3D11ShaderResourceView* pnull = NULL;
        m_pcontext->PSSetShaderResources(0, 1, &pnull);

        //
        // Present on the next vertical blank, as DirectDraw's Flip did
        //

        HRESULT hr = pentry->m_pswapChain->Present(1, 0);

        D3D11BoundRasterizerChanged();

        if (CheckDeviceLost(hr)) {
            return;
        }

        //
        // Wait until the queue has room for the next frame (at most a
        // refresh), so the next frame starts from fresh input
        //

        if (pentry->m_hlatency != NULL) {
            ::WaitForSingleObjectEx(pentry->m_hlatency, 100, TRUE);
        }
    }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

TRef<D3D11Device> CreateD3D11Device()
{
    D3D11DeviceImpl*  pimpl   = new D3D11DeviceImpl();
    TRef<D3D11Device> pdevice = pimpl;

    if (pimpl->CreateDeviceObjects()) {
        return pdevice;
    }

    return NULL;
}
