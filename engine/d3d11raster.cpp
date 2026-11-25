#include "pch.h"
#include "d3d11engine.h"

#include "raster_vs.h"
#include "raster_vs_flat.h"
#include "raster_ps.h"
#include "raster_ps_flat.h"

//////////////////////////////////////////////////////////////////////////////
//
// D3D11 Rasterizer
//
// Replaces the DirectX 6 D3DRasterizer (raster.cpp) and the software
// rasterizer (softraster.cpp). It takes the same pre-transformed screen
// vertices from device3d.cpp's CPU transform, lighting and clipping, and
// reproduces the render states that path set.
//
// The surface's system-memory pixels stay the master copy, because all 2D
// drawing (blits, fills, text) is done on the CPU. BeginScene uploads them to
// the GPU render target if they changed; EndScene reads the rendered result
// back if anything was drawn. The Context already ends the scene before every
// 2D operation, so those are the only synchronisation points needed.
//
//////////////////////////////////////////////////////////////////////////////

namespace {

//
// Pipeline objects shared by every rasterizer, re-created with the device
//

struct Pipeline {
    D3D11Device*                   m_pdevice;
    int                            m_generation;

    TRef<ID3D11VertexShader>       m_pvs;
    TRef<ID3D11VertexShader>       m_pvsFlat;
    TRef<ID3D11PixelShader>        m_pps;
    TRef<ID3D11PixelShader>        m_ppsFlat;
    TRef<ID3D11InputLayout>        m_playout;
    TRef<ID3D11Buffer>             m_pcb;
    TRef<ID3D11Buffer>             m_pvb;
    TRef<ID3D11Buffer>             m_pib;
    int                            m_vbCapacity;
    int                            m_ibCapacity;

    TRef<ID3D11BlendState>         m_pblend[3];         // BlendMode
    TRef<ID3D11BlendState>         m_pblendNoColor;
    TRef<ID3D11DepthStencilState>  m_pdepth[2][2];      // [test][write]
    TRef<ID3D11DepthStencilState>  m_pdepthAlways;
    TRef<ID3D11RasterizerState>    m_praster[3];        // none, cw, ccw
    TRef<ID3D11SamplerState>       m_psampler[2];       // point, linear

    bool                           m_b565Target;

    Pipeline() :
        m_pdevice(NULL),
        m_generation(-1),
        m_vbCapacity(0),
        m_ibCapacity(0),
        m_b565Target(false)
    {
    }
};

Pipeline   g_pipeline;
Rasterizer* g_pboundRasterizer = NULL;

bool CreatePipeline(D3D11Device* pdevice)
{
    Pipeline& p = g_pipeline;

    if (p.m_pdevice == pdevice && p.m_generation == pdevice->GetGeneration() && p.m_pvs != NULL) {
        return true;
    }

    p = Pipeline();
    g_pboundRasterizer = NULL;

    ID3D11Device* pd3d = pdevice->GetDevice();

    if (pd3d == NULL) {
        return false;
    }

    if (FAILED(pd3d->CreateVertexShader(g_raster_vs,      sizeof(g_raster_vs),      NULL, &p.m_pvs    ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_raster_vs_flat, sizeof(g_raster_vs_flat), NULL, &p.m_pvsFlat))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_raster_ps,      sizeof(g_raster_ps),      NULL, &p.m_pps    ))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_raster_ps_flat, sizeof(g_raster_ps_flat), NULL, &p.m_ppsFlat))) return false;

    //
    // VertexScreen: x y z rhw, color, specular (unused), u v
    //

    static const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(pd3d->CreateInputLayout(layout, ArrayCount(layout), g_raster_vs, sizeof(g_raster_vs), &p.m_playout))) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = 8 * sizeof(DWORD);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pd3d->CreateBuffer(&bd, NULL, &p.m_pcb))) return false;

    //
    // Blend: BlendModeSource (off), BlendModeAdd (one, one),
    // BlendModeSourceAlpha (one, inverse source alpha)
    //

    for (int mode = 0; mode < 3; mode++) {
        D3D11_BLEND_DESC desc = {};
        D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];

        rt.BlendEnable           = mode != BlendModeSource;
        rt.SrcBlend              = D3D11_BLEND_ONE;
        rt.DestBlend             = mode == BlendModeAdd ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp               = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
        rt.DestBlendAlpha        = rt.DestBlend == D3D11_BLEND_ONE ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblend[mode]))) return false;
    }

    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendNoColor))) return false;
    }

    //
    // Depth: ZENABLE off means neither test nor write, as in DirectX 6
    //

    for (int test = 0; test < 2; test++) {
        for (int write = 0; write < 2; write++) {
            D3D11_DEPTH_STENCIL_DESC desc = {};
            desc.DepthEnable    = test ? TRUE : FALSE;
            desc.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;

            if (FAILED(pd3d->CreateDepthStencilState(&desc, &p.m_pdepth[test][write]))) return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable    = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
        if (FAILED(pd3d->CreateDepthStencilState(&desc, &p.m_pdepthAlways))) return false;
    }

    //
    // Culling is decided in screen space, y down, as in DirectX 6:
    // D3DCULL_CCW culls counter-clockwise triangles, which are Direct3D 11's
    // back faces with FrontCounterClockwise = FALSE.
    //
    // Depth clipping is off so out-of-range depths clamp, like the old
    // rasterizer's CheckVertices did; the scissor enforces the clip rect.
    //

    static const D3D11_CULL_MODE culls[3] = { D3D11_CULL_NONE, D3D11_CULL_FRONT, D3D11_CULL_BACK };

    for (int index = 0; index < 3; index++) {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode              = D3D11_FILL_SOLID;
        desc.CullMode              = culls[index];
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable       = FALSE;
        desc.ScissorEnable         = TRUE;

        if (FAILED(pd3d->CreateRasterizerState(&desc, &p.m_praster[index]))) return false;
    }

    //
    // Samplers: default (wrap) addressing, no mip maps, as before
    //

    for (int linear = 0; linear < 2; linear++) {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter         = linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD         = D3D11_FLOAT32_MAX;

        if (FAILED(pd3d->CreateSamplerState(&desc, &p.m_psampler[linear]))) return false;
    }

    //
    // A 565 render target makes the CPU <-> GPU copies plain memcpys. It is
    // optional before Direct3D 11.1, so check, and fall back to BGRA8.
    //

    UINT support = 0;
    UINT needed  =
          D3D11_FORMAT_SUPPORT_TEXTURE2D
        | D3D11_FORMAT_SUPPORT_RENDER_TARGET
        | D3D11_FORMAT_SUPPORT_BLENDABLE;

    p.m_b565Target =
           SUCCEEDED(pd3d->CheckFormatSupport(DXGI_FORMAT_B5G6R5_UNORM, &support))
        && (support & needed) == needed;

    p.m_pdevice    = pdevice;
    p.m_generation = pdevice->GetGeneration();

    return true;
}

bool EnsureBuffer(TRef<ID3D11Buffer>& pbuffer, int& capacity, int bytes, UINT bind)
{
    if (pbuffer != NULL && capacity >= bytes) {
        return true;
    }

    int size = MaxOf(bytes, 64 * 1024);

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = size;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    pbuffer  = NULL;
    capacity = 0;

    if (FAILED(g_pipeline.m_pdevice->GetDevice()->CreateBuffer(&desc, NULL, &pbuffer))) {
        return false;
    }

    capacity = size;
    return true;
}

} // namespace

void D3D11BoundRasterizerChanged()
{
    g_pboundRasterizer = NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// D3D11RasterizerImpl
//
//////////////////////////////////////////////////////////////////////////////

class D3D11RasterizerImpl : public Rasterizer {
private:
    //
    // The surface owns the context that owns this rasterizer, so plain
    // pointers (a TRef would be a cycle).
    //

    PrivateSurface*                m_psurface;
    PrivateEngine*                 m_pengine;

    //
    // Render target
    //

    TRef<ID3D11Texture2D>          m_ptarget;
    TRef<ID3D11RenderTargetView>   m_prtv;
    TRef<ID3D11Texture2D>          m_pdepth;
    TRef<ID3D11DepthStencilView>   m_pdsv;
    TRef<ID3D11Texture2D>          m_pstaging;
    WinPoint                       m_sizeTarget;
    bool                           m_b565;
    int                            m_generation;
    int                            m_idSynced;
    bool                           m_bInScene;
    bool                           m_bDrawn;
    std::vector<DWORD>             m_vconvert;

    //
    // State
    //

    ShadeMode                      m_shadeMode;
    BlendMode                      m_blendMode;
    CullMode                       m_cullMode;
    bool                           m_bZTest;
    bool                           m_bZWrite;
    bool                           m_bLinearFilter;
    bool                           m_bColorKey;
    TRef<Surface>                  m_psurfaceTexture;
    Rect                           m_rectClip;

    bool                           m_bStateChanged;
    DWORD                          m_flagsBound;

public:
    D3D11RasterizerImpl(PrivateSurface* psurface) :
        m_psurface(psurface),
        m_b565(false),
        m_generation(-1),
        m_idSynced(-1),
        m_bInScene(false),
        m_bDrawn(false),
        m_shadeMode(ShadeModeGouraud),
        m_blendMode(BlendModeSource),
        m_cullMode(CullModeNone),
        m_bZTest(false),
        m_bZWrite(false),
        m_bLinearFilter(true),
        m_bColorKey(true),
        m_bStateChanged(true),
        m_flagsBound(0xffffffff)
    {
        CastTo(m_pengine, psurface->GetEngine());

        WinPoint size = psurface->GetSize();
        m_rectClip = Rect(0, 0, (float)size.X(), (float)size.Y());
    }

    ~D3D11RasterizerImpl()
    {
        Terminate();
    }

    bool IsValid()
    {
        D3D11Device* pdevice = m_pengine->GetD3D11Device();
        return pdevice != NULL && CreatePipeline(pdevice);
    }

    void Terminate()
    {
        if (g_pboundRasterizer == this) {
            g_pboundRasterizer = NULL;
        }

        m_prtv     = NULL;
        m_ptarget  = NULL;
        m_pdsv     = NULL;
        m_pdepth   = NULL;
        m_pstaging = NULL;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Render target management
    //
    //////////////////////////////////////////////////////////////////////////////

    D3D11Device* GetDevice()
    {
        D3D11Device* pdevice = m_pengine->GetD3D11Device();

        if (pdevice == NULL || !CreatePipeline(pdevice)) {
            return NULL;
        }

        return pdevice;
    }

    bool EnsureTarget(D3D11Device* pdevice)
    {
        WinPoint size = m_psurface->GetSize();

        if (
               m_ptarget != NULL
            && m_generation == pdevice->GetGeneration()
            && m_sizeTarget == size
        ) {
            return true;
        }

        Terminate();
        m_idSynced = -1;

        ID3D11Device* pd3d = pdevice->GetDevice();

        m_b565 = g_pipeline.m_b565Target && m_psurface->GetPixelFormat()->Is565();
        DXGI_FORMAT format = m_b565 ? DXGI_FORMAT_B5G6R5_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = size.X();
        td.Height           = size.Y();
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = format;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET;

        HRESULT hr = pd3d->CreateTexture2D(&td, NULL, &m_ptarget);
        if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(m_ptarget, NULL, &m_prtv);

        if (SUCCEEDED(hr)) {
            td.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
            td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            hr = pd3d->CreateTexture2D(&td, NULL, &m_pdepth);
        }

        if (SUCCEEDED(hr)) hr = pd3d->CreateDepthStencilView(m_pdepth, NULL, &m_pdsv);

        if (SUCCEEDED(hr)) {
            td.Format         = format;
            td.BindFlags      = 0;
            td.Usage          = D3D11_USAGE_STAGING;
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            hr = pd3d->CreateTexture2D(&td, NULL, &m_pstaging);
        }

        if (FAILED(hr)) {
            Terminate();
            pdevice->CheckDeviceLost(hr);
            return false;
        }

        //
        // Start with a cleared z-buffer, as a freshly attached DirectDraw
        // z-buffer surface was (contents undefined, cleared before use)
        //

        pdevice->GetContext()->ClearDepthStencilView(m_pdsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        m_sizeTarget = size;
        m_generation = pdevice->GetGeneration();
        m_bStateChanged = true;
        return true;
    }

    void Upload(D3D11Device* pdevice)
    {
        WinPoint size = m_psurface->GetSize();
        ID3D11DeviceContext* pcontext = pdevice->GetContext();

        if (m_b565) {
            pcontext->UpdateSubresource(m_ptarget, 0, NULL, m_psurface->GetPointer(), m_psurface->GetPitch(), 0);
        } else {
            m_vconvert.resize(size.X() * size.Y());
            ConvertSurfaceToBGRA(
                m_psurface->GetPointer(),
                m_psurface->GetPitch(),
                m_psurface->GetPixelFormat(),
                size,
                false,
                m_vconvert.data(),
                size.X() * 4
            );
            pcontext->UpdateSubresource(m_ptarget, 0, NULL, m_vconvert.data(), size.X() * 4, 0);
        }

        m_psurface->ReleasePointer();
        m_idSynced = m_psurface->GetChangeID();
    }

    void Readback(D3D11Device* pdevice)
    {
        WinPoint size = m_psurface->GetSize();
        ID3D11DeviceContext* pcontext = pdevice->GetContext();

        pcontext->CopyResource(m_pstaging, m_ptarget);

        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = pcontext->Map(m_pstaging, 0, D3D11_MAP_READ, 0, &mapped);

        if (FAILED(hr)) {
            pdevice->CheckDeviceLost(hr);
            return;
        }

        BYTE* pdest = m_psurface->GetWritablePointer();
        int   pitch = m_psurface->GetPitch();

        if (m_b565) {
            int bytes = size.X() * 2;
            for (int y = 0; y < size.Y(); y++) {
                memcpy(pdest + y * pitch, (BYTE*)mapped.pData + y * mapped.RowPitch, bytes);
            }
        } else {
            ConvertBGRAToSurface(
                (const DWORD*)mapped.pData,
                mapped.RowPitch,
                m_psurface->GetPixelFormat(),
                size,
                pdest,
                pitch
            );
        }

        pcontext->Unmap(m_pstaging, 0);
        m_psurface->ReleasePointer();

        //
        // The CPU copy now matches the GPU one
        //

        m_idSynced = m_psurface->GetChangeID();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Binding: rasterizers share the immediate context, and pane surfaces can
    // render while another surface's scene is open, so a draw rebinds
    // everything if another rasterizer (or the presenter) used the context.
    //
    //////////////////////////////////////////////////////////////////////////////

    bool Bind(D3D11Device*& pdevice)
    {
        pdevice = GetDevice();

        if (pdevice == NULL || !EnsureTarget(pdevice)) {
            return false;
        }

        if (!m_bInScene) {
            //
            // Drawing outside BeginScene/EndScene: treat as a one-draw scene
            //

            BeginScene();
        }

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        if (g_pboundRasterizer != this) {
            g_pboundRasterizer = this;
            m_bStateChanged    = true;
            m_flagsBound       = 0xffffffff;

            ID3D11RenderTargetView* prtv = m_prtv;
            pcontext->OMSetRenderTargets(1, &prtv, m_pdsv);

            D3D11_VIEWPORT vp;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            vp.Width    = (float)m_sizeTarget.X();
            vp.Height   = (float)m_sizeTarget.Y();
            vp.MinDepth = 0;
            vp.MaxDepth = 1;
            pcontext->RSSetViewports(1, &vp);

            UINT stride = sizeof(VertexScreen);
            UINT offset = 0;
            ID3D11Buffer* pvb = p.m_pvb;
            pcontext->IASetInputLayout(p.m_playout);
            pcontext->IASetVertexBuffers(0, 1, &pvb, &stride, &offset);
            pcontext->IASetIndexBuffer(p.m_pib, DXGI_FORMAT_R16_UINT, 0);

            ID3D11Buffer* pcb = p.m_pcb;
            pcontext->VSSetConstantBuffers(0, 1, &pcb);
            pcontext->PSSetConstantBuffers(0, 1, &pcb);
        }

        return true;
    }

    void ApplyState(D3D11Device* pdevice, bool bPoints)
    {
        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        //
        // DrawPoints in the old rasterizer plotted pixels on the CPU,
        // ignoring texture, blending and z; keep that behaviour.
        //

        ID3D11ShaderResourceView* psrv = NULL;

        if (!bPoints && m_psurfaceTexture != NULL) {
            PrivateSurface* ptexture; CastTo(ptexture, (Surface*)m_psurfaceTexture);
            psrv = pdevice->GetTexture(ptexture);
        }

        bool  bFlat   = !bPoints && m_shadeMode == ShadeModeFlat;
        bool  bDecal  = m_shadeMode == ShadeModeNone || m_shadeMode == ShadeModeCopy;
        DWORD flags   =
              (psrv != NULL ? 1 : 0)
            | (bDecal       ? 2 : 0)
            | (m_bColorKey  ? 4 : 0);

        if (flags != m_flagsBound) {
            D3D11_MAPPED_SUBRESOURCE mapped;

            if (SUCCEEDED(pcontext->Map(p.m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                float* pf = (float*)mapped.pData;
                pf[0] = 2.0f / m_sizeTarget.X();
                pf[1] = 2.0f / m_sizeTarget.Y();
                pf[2] = 0;
                pf[3] = 0;

                DWORD* pdw = (DWORD*)(pf + 4);
                pdw[0] = flags;
                pdw[1] = 0;
                pdw[2] = 0;
                pdw[3] = 0;

                pcontext->Unmap(p.m_pcb, 0);
            }

            m_flagsBound = flags;
        }

        pcontext->VSSetShader(bFlat ? p.m_pvsFlat : p.m_pvs, NULL, 0);
        pcontext->PSSetShader(bFlat ? p.m_ppsFlat : p.m_pps, NULL, 0);
        pcontext->PSSetShaderResources(0, 1, &psrv);

        ID3D11SamplerState* psampler = p.m_psampler[m_bLinearFilter ? 1 : 0];
        pcontext->PSSetSamplers(0, 1, &psampler);

        if (bPoints) {
            pcontext->OMSetBlendState(p.m_pblend[BlendModeSource], NULL, 0xffffffff);
            pcontext->OMSetDepthStencilState(p.m_pdepth[0][0], 0);
            pcontext->RSSetState(p.m_praster[0]);
        } else {
            pcontext->OMSetBlendState(p.m_pblend[m_blendMode], NULL, 0xffffffff);
            pcontext->OMSetDepthStencilState(p.m_pdepth[m_bZTest ? 1 : 0][m_bZWrite ? 1 : 0], 0);

            int cull = m_cullMode == CullModeCW ? 1 : (m_cullMode == CullModeCCW ? 2 : 0);
            pcontext->RSSetState(p.m_praster[cull]);
        }

        D3D11_RECT scissor;
        scissor.left   = MaxOf<LONG>(0,             (LONG)floorf(m_rectClip.XMin()));
        scissor.top    = MaxOf<LONG>(0,             (LONG)floorf(m_rectClip.YMin()));
        scissor.right  = MinOf((LONG)m_sizeTarget.X(), (LONG)ceilf(m_rectClip.XMax()));
        scissor.bottom = MinOf((LONG)m_sizeTarget.Y(), (LONG)ceilf(m_rectClip.YMax()));
        pcontext->RSSetScissorRects(1, &scissor);

        m_bStateChanged = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // State
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetShadeMode(ShadeMode shadeMode)                    { m_shadeMode     = shadeMode;      m_bStateChanged = true; }
    void SetBlendMode(BlendMode blendMode)                    { m_blendMode     = blendMode;      m_bStateChanged = true; }
    void SetCullMode(CullMode cullMode)                       { m_cullMode      = cullMode;       m_bStateChanged = true; }
    void SetZTest(bool bZTest)                                { m_bZTest        = bZTest;         m_bStateChanged = true; }
    void SetZWrite(bool bZWrite)                              { m_bZWrite       = bZWrite;        m_bStateChanged = true; }
    void SetLinearFilter(bool bLinearFilter)                  { m_bLinearFilter = bLinearFilter;  m_bStateChanged = true; }
    void SetColorKey(bool bColorKey)                          { m_bColorKey     = bColorKey;      m_bStateChanged = true; }
    void SetTexture(Surface* psurfaceTexture)                 { m_psurfaceTexture = psurfaceTexture; m_bStateChanged = true; }

    //
    // Always on with hardware, ignored, and cylindrical wrapping (unused by
    // the game), as in the DirectX 6 rasterizer
    //

    void SetPerspectiveCorrection(bool bPerspectiveCorrection) {}
    void SetDither(bool bDither)                               {}
    void SetWrapMode(WrapMode wrapMode)                        {}

    void SetClipRect(const Rect& rectClip)
    {
        m_rectClip      = rectClip;
        m_bStateChanged = true;
    }

    Point GetSurfaceSize()
    {
        return Point::Cast(m_psurface->GetSize());
    }

    bool Has3DAcceleration()
    {
        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Scenes
    //
    //////////////////////////////////////////////////////////////////////////////

    void BeginScene()
    {
        if (m_bInScene) {
            return;
        }

        D3D11Device* pdevice = GetDevice();

        if (pdevice == NULL || !EnsureTarget(pdevice)) {
            return;
        }

        if (m_idSynced != m_psurface->GetChangeID()) {
            Upload(pdevice);
        }

        m_bInScene = true;
        m_bDrawn   = false;
    }

    void EndScene()
    {
        if (!m_bInScene) {
            return;
        }

        m_bInScene = false;

        if (m_bDrawn) {
            D3D11Device* pdevice = GetDevice();

            if (pdevice != NULL && m_ptarget != NULL) {
                Readback(pdevice);
            }
        }
    }

    void ClearZBuffer()
    {
        D3D11Device* pdevice;

        if (!Bind(pdevice)) {
            return;
        }

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        WinPoint size = m_sizeTarget;

        if (
               m_rectClip.XMin() <= 0 && m_rectClip.YMin() <= 0
            && m_rectClip.XMax() >= size.X() && m_rectClip.YMax() >= size.Y()
        ) {
            pcontext->ClearDepthStencilView(m_pdsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            return;
        }

        //
        // The DirectX 6 viewport Clear only touched the clip rect: draw a
        // depth-only quad at z = 1 through the scissor.
        //

        float w = (float)size.X();
        float h = (float)size.Y();

        VertexScreen quad[4];
        memset(quad, 0, sizeof(quad));
        quad[0].x = -1; quad[0].y = -1;
        quad[1].x =  w; quad[1].y = -1;
        quad[2].x = -1; quad[2].y =  h;
        quad[3].x =  w; quad[3].y =  h;

        for (int index = 0; index < 4; index++) {
            quad[index].z  = 1;
            quad[index].qw = 1;
        }

        static const MeshIndex indices[6] = { 0, 1, 2, 2, 1, 3 };

        ApplyState(pdevice, true);
        pcontext->OMSetBlendState(g_pipeline.m_pblendNoColor, NULL, 0xffffffff);
        pcontext->OMSetDepthStencilState(g_pipeline.m_pdepthAlways, 0);

        Draw(pdevice, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, quad, 4, indices, 6);

        //
        // Not a visible change: no readback needed for this alone
        //

        m_bStateChanged = true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Rendering
    //
    //////////////////////////////////////////////////////////////////////////////

    void Draw(
        D3D11Device*             pdevice,
        D3D11_PRIMITIVE_TOPOLOGY topology,
        const VertexScreen*      pvertex,
        int                      vcount,
        const MeshIndex*         pindex,
        int                      icount
    ) {
        Pipeline&            p        = g_pipeline;
        ID3D11DeviceContext* pcontext = pdevice->GetContext();

        int vbytes = vcount * sizeof(VertexScreen);
        int ibytes = icount * sizeof(MeshIndex);

        ID3D11Buffer* pvbOld = p.m_pvb;
        ID3D11Buffer* pibOld = p.m_pib;

        if (
               !EnsureBuffer(p.m_pvb, p.m_vbCapacity, vbytes, D3D11_BIND_VERTEX_BUFFER)
            || (icount > 0 && !EnsureBuffer(p.m_pib, p.m_ibCapacity, ibytes, D3D11_BIND_INDEX_BUFFER))
        ) {
            return;
        }

        if (p.m_pvb != pvbOld || p.m_pib != pibOld) {
            UINT stride = sizeof(VertexScreen);
            UINT offset = 0;
            ID3D11Buffer* pvb = p.m_pvb;
            pcontext->IASetVertexBuffers(0, 1, &pvb, &stride, &offset);
            pcontext->IASetIndexBuffer(p.m_pib, DXGI_FORMAT_R16_UINT, 0);
        }

        D3D11_MAPPED_SUBRESOURCE mapped;

        if (FAILED(pcontext->Map(p.m_pvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            return;
        }
        memcpy(mapped.pData, pvertex, vbytes);
        pcontext->Unmap(p.m_pvb, 0);

        pcontext->IASetPrimitiveTopology(topology);

        if (icount > 0) {
            if (FAILED(pcontext->Map(p.m_pib, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                return;
            }
            memcpy(mapped.pData, pindex, ibytes);
            pcontext->Unmap(p.m_pib, 0);

            pcontext->DrawIndexed(icount, 0, 0);
        } else {
            pcontext->Draw(vcount, 0);
        }
    }

    void DrawPrimitive(
        D3D11_PRIMITIVE_TOPOLOGY topology,
        const VertexScreen*      pvertex,
        int                      vcount,
        const MeshIndex*         pindex,
        int                      icount,
        bool                     bPoints
    ) {
        if (vcount <= 0 || (pindex != NULL && icount <= 0)) {
            return;
        }

        D3D11Device* pdevice;

        if (!Bind(pdevice)) {
            return;
        }

        ApplyState(pdevice, bPoints);
        Draw(pdevice, topology, pvertex, vcount, pindex, pindex != NULL ? icount : 0);
        m_bDrawn = true;
    }

    void DrawTriangles(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, pvertex, vcount, pindex, icount, false);
    }

    void DrawLines(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_LINELIST, pvertex, vcount, pindex, icount, false);
    }

    void DrawPoints(const VertexScreen* pvertex, int vcount)
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, pvertex, vcount, NULL, 0, true);
    }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

TRef<Rasterizer> CreateD3D11Rasterizer(PrivateSurface* psurface)
{
    TRef<Rasterizer> prasterizer = new D3D11RasterizerImpl(psurface);

    if (prasterizer->IsValid()) {
        return prasterizer;
    }

    return NULL;
}
