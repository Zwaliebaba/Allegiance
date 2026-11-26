#include "pch.h"
#include "d3d11engine.h"

#include "raster_vs.h"
#include "raster_vs_flat.h"
#include "raster_ps.h"
#include "raster_ps_flat.h"
#include "tl_vs.h"
#include "tl_vs_flat.h"
#include "tl_vs_l.h"
#include "tl_vs_l_flat.h"

//////////////////////////////////////////////////////////////////////////////
//
// D3D11 Rasterizer
//
// Replaces the DirectX 6 D3DRasterizer (raster.cpp) and the software
// rasterizer (softraster.cpp). It takes the same pre-transformed screen
// vertices from device3d.cpp's CPU transform, lighting and clipping, and
// reproduces the render states that path set.
//
// It draws into the surface's GPU copy (PrivateSurface::BeginGpuDraw), which
// the surface keeps between scenes; 2D drawing into the same surface is done
// on the GPU too, so nothing is copied back to system memory unless
// something reads the pixels.
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
    TRef<ID3D11VertexShader>       m_pvsTL;             // Vertex: lit in the shader
    TRef<ID3D11VertexShader>       m_pvsTLFlat;
    TRef<ID3D11VertexShader>       m_pvsTLL;            // VertexL: own colours
    TRef<ID3D11VertexShader>       m_pvsTLLFlat;
    TRef<ID3D11PixelShader>        m_pps;
    TRef<ID3D11PixelShader>        m_ppsFlat;
    TRef<ID3D11InputLayout>        m_playout;
    TRef<ID3D11InputLayout>        m_playoutTL;
    TRef<ID3D11InputLayout>        m_playoutTLL;
    TRef<ID3D11Buffer>             m_pcbTL;
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
    TRef<ID3D11RasterizerState>    m_prasterTL[3];      // the same, clipping to near and far
    TRef<ID3D11SamplerState>       m_psampler[2];       // point, linear

    Pipeline() :
        m_pdevice(NULL),
        m_generation(-1),
        m_vbCapacity(0),
        m_ibCapacity(0)
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
    if (FAILED(pd3d->CreateVertexShader(g_tl_vs,          sizeof(g_tl_vs),          NULL, &p.m_pvsTL     ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_tl_vs_flat,     sizeof(g_tl_vs_flat),     NULL, &p.m_pvsTLFlat ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_tl_vs_l,        sizeof(g_tl_vs_l),        NULL, &p.m_pvsTLL    ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_tl_vs_l_flat,   sizeof(g_tl_vs_l_flat),   NULL, &p.m_pvsTLLFlat))) return false;
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

    //
    // Vertex: x y z, u v, nx ny nz. VertexL: x y z, u v, r g b a (floats).
    //

    static const D3D11_INPUT_ELEMENT_DESC layoutTL[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    static const D3D11_INPUT_ELEMENT_DESC layoutTLL[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(pd3d->CreateInputLayout(layoutTL,  ArrayCount(layoutTL),  g_tl_vs,   sizeof(g_tl_vs),   &p.m_playoutTL ))) return false;
    if (FAILED(pd3d->CreateInputLayout(layoutTLL, ArrayCount(layoutTLL), g_tl_vs_l, sizeof(g_tl_vs_l), &p.m_playoutTLL))) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = 8 * sizeof(DWORD);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pd3d->CreateBuffer(&bd, NULL, &p.m_pcb))) return false;

    bd.ByteWidth = 16 * 4 * sizeof(float);
    if (FAILED(pd3d->CreateBuffer(&bd, NULL, &p.m_pcbTL))) return false;

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

        //
        // Model-space geometry is clipped to the near and far planes, as
        // device3d.cpp's clipper did
        //

        desc.DepthClipEnable = TRUE;

        if (FAILED(pd3d->CreateRasterizerState(&desc, &p.m_prasterTL[index]))) return false;
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

    TRef<D3D11Target>              m_ptarget;       // the surface's GPU copy, as last bound
    ID3D11RenderTargetView*        m_prtvBound;
    WinPoint                       m_sizeTarget;
    bool                           m_bInScene;

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
        m_prtvBound(NULL),
        m_sizeTarget(0, 0),
        m_bInScene(false),
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

        m_ptarget   = NULL;
        m_prtvBound = NULL;
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

    //////////////////////////////////////////////////////////////////////////////
    //
    // Binding: rasterizers share the immediate context, and pane surfaces can
    // render while another surface's scene is open, so a draw rebinds
    // everything if another rasterizer, the 2D drawing or the presenter used
    // the context, or if the surface's GPU copy was re-created.
    //
    //////////////////////////////////////////////////////////////////////////////

    bool Bind(D3D11Device*& pdevice)
    {
        pdevice = GetDevice();

        if (pdevice == NULL) {
            return false;
        }

        D3D11Target* ptarget = m_psurface->BeginGpuDraw();

        if (ptarget == NULL || !D3D11EnsureDepth(pdevice, ptarget)) {
            return false;
        }

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        if (g_pboundRasterizer != this || m_prtvBound != ptarget->m_prtv) {
            g_pboundRasterizer = this;
            m_ptarget          = ptarget;
            m_prtvBound        = ptarget->m_prtv;
            m_sizeTarget       = ptarget->m_size;
            m_bStateChanged    = true;
            m_flagsBound       = 0xffffffff;

            ID3D11RenderTargetView* prtv = ptarget->m_prtv;
            pcontext->OMSetRenderTargets(1, &prtv, ptarget->m_pdsv);

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

        bool bKeyBlack = false;

        if (!bPoints && m_psurfaceTexture != NULL) {
            PrivateSurface* ptexture; CastTo(ptexture, (Surface*)m_psurfaceTexture);

            //
            // A texture that is itself a GPU surface (a pane image) is read
            // directly; its alpha is coverage, and opaque black is its colour
            // key too
            //

            psrv = ptexture->GetGpuSource();

            if (psrv != NULL) {
                bKeyBlack = true;
            } else {
                psrv = pdevice->GetTexture(ptexture);
            }
        }

        bool  bFlat   = !bPoints && m_shadeMode == ShadeModeFlat;
        bool  bDecal  = m_shadeMode == ShadeModeNone || m_shadeMode == ShadeModeCopy;
        bool  bBlend  = !bPoints && m_blendMode != BlendModeSource;
        DWORD flags   =
              (psrv != NULL ? 1 : 0)
            | (bDecal       ? 2 : 0)
            | (m_bColorKey  ? 4 : 0)
            | (bKeyBlack    ? 8 : 0)
            | (bBlend       ? 16 : 0);

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

    //
    // Scenes need no synchronisation any more: the GPU copy is current while
    // the GPU draws, and the surface reads it back only if the CPU asks for
    // the pixels.
    //

    void BeginScene()
    {
        m_bInScene = true;
    }

    void EndScene()
    {
        m_bInScene = false;
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
            pcontext->ClearDepthStencilView(m_ptarget->m_pdsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
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

        Draw(pdevice, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, quad, sizeof(VertexScreen), 4, indices, 6);

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
        const void*              pvertex,
        int                      stride,
        int                      vcount,
        const MeshIndex*         pindex,
        int                      icount
    ) {
        Pipeline&            p        = g_pipeline;
        ID3D11DeviceContext* pcontext = pdevice->GetContext();

        int vbytes = vcount * stride;
        int ibytes = icount * sizeof(MeshIndex);

        ID3D11Buffer* pibOld = p.m_pib;

        if (
               !EnsureBuffer(p.m_pvb, p.m_vbCapacity, vbytes, D3D11_BIND_VERTEX_BUFFER)
            || (icount > 0 && !EnsureBuffer(p.m_pib, p.m_ibCapacity, ibytes, D3D11_BIND_INDEX_BUFFER))
        ) {
            return;
        }

        //
        // Screen and model-space vertices share the buffer with different
        // strides, so bind it every time
        //

        UINT          ustride = stride;
        UINT          offset  = 0;
        ID3D11Buffer* pvb     = p.m_pvb;
        pcontext->IASetVertexBuffers(0, 1, &pvb, &ustride, &offset);

        if (p.m_pib != pibOld) {
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
        Draw(pdevice, topology, pvertex, sizeof(VertexScreen), vcount, pindex, pindex != NULL ? icount : 0);
        g_d3d11Stats.m_draws3D++;
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

    //////////////////////////////////////////////////////////////////////////////
    //
    // Model-space geometry: the same pixel state, with the transform and
    // lighting vertex shaders and a constant buffer from Device3D
    //
    //////////////////////////////////////////////////////////////////////////////

    struct TLConstants {
        float m_full[4][4];
        float m_post[4][4];
        float m_light[4];
        float m_color[4];
        float m_colorAlt[4];
        float m_global[4];
        float m_params[4];
        UINT  m_mode[4];
        float m_planes[c_maxTLClipPlanes][4];
    };

    void DrawModel(
        D3D11_PRIMITIVE_TOPOLOGY  topology,
        const TransformAndLight&  tl,
        const void*               pvertex,
        int                       stride,
        bool                      bNormal,
        int                       vcount,
        const MeshIndex*          pindex,
        int                       icount
    ) {
        if (vcount <= 0 || pindex == NULL || icount <= 0) {
            return;
        }

        D3D11Device* pdevice;

        if (!Bind(pdevice)) {
            return;
        }

        ApplyState(pdevice, false);

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        D3D11_MAPPED_SUBRESOURCE mapped;

        if (FAILED(pcontext->Map(p.m_pcbTL, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            return;
        }

        TLConstants* pc = (TLConstants*)mapped.pData;

        memcpy(pc->m_full,     tl.m_full,     sizeof(pc->m_full));
        memcpy(pc->m_post,     tl.m_post,     sizeof(pc->m_post));
        memcpy(pc->m_light,    tl.m_light,    sizeof(pc->m_light));
        memcpy(pc->m_color,    tl.m_color,    sizeof(pc->m_color));
        memcpy(pc->m_colorAlt, tl.m_colorAlt, sizeof(pc->m_colorAlt));
        memcpy(pc->m_global,   tl.m_global,   sizeof(pc->m_global));
        memcpy(pc->m_planes,   tl.m_planes,   sizeof(pc->m_planes));

        pc->m_params[0] = tl.m_ambient;
        pc->m_params[1] = 0;
        pc->m_params[2] = 0;
        pc->m_params[3] = 0;
        pc->m_mode[0]   = tl.m_mode;
        pc->m_mode[1]   = MinOf(tl.m_countPlanes, c_maxTLClipPlanes);
        pc->m_mode[2]   = 0;
        pc->m_mode[3]   = 0;

        pcontext->Unmap(p.m_pcbTL, 0);

        bool bFlat = m_shadeMode == ShadeModeFlat;
        int  cull  = m_cullMode == CullModeCW ? 1 : (m_cullMode == CullModeCCW ? 2 : 0);

        ID3D11Buffer* pcb = p.m_pcbTL;
        pcontext->VSSetConstantBuffers(1, 1, &pcb);
        pcontext->VSSetShader(
            bNormal
                ? (bFlat ? p.m_pvsTLFlat  : p.m_pvsTL )
                : (bFlat ? p.m_pvsTLLFlat : p.m_pvsTLL),
            NULL,
            0
        );
        pcontext->IASetInputLayout(bNormal ? p.m_playoutTL : p.m_playoutTLL);
        pcontext->RSSetState(p.m_prasterTL[cull]);

        Draw(pdevice, topology, pvertex, stride, vcount, pindex, icount);
        g_d3d11Stats.m_draws3D++;

        //
        // Screen-vertex draws only bind their layout when the rasterizer is
        // rebound
        //

        pcontext->IASetInputLayout(p.m_playout);
    }

    void DrawTriangles(const TransformAndLight& tl, const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, tl, pvertex, sizeof(Vertex), true, vcount, pindex, icount);
    }

    void DrawTriangles(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, tl, pvertex, sizeof(VertexL), false, vcount, pindex, icount);
    }

    void DrawLines(const TransformAndLight& tl, const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_LINELIST, tl, pvertex, sizeof(Vertex), true, vcount, pindex, icount);
    }

    void DrawLines(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_LINELIST, tl, pvertex, sizeof(VertexL), false, vcount, pindex, icount);
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
