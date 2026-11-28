#include "pch.h"
#include "D3D11Engine.h"

#include "Draw2DVs.h"
#include "Draw2DPs.h"

//////////////////////////////////////////////////////////////////////////////
//
// GPU copies of surfaces and 2D drawing into them
//
// The window frame and the 3D panes keep a BGRA8 render target next to their
// CPU pixels (see Surface.cpp for when each copy is current). This file
// creates those targets, copies pixels between the two copies, and draws the
// 2D primitives the engine used to do with CPU loops: blits, fills and text.
//
// Targets hold premultiplied alpha, which is coverage: colour keyed images
// are blended over what is there, fills and opaque images replace it, and a
// fill with the colour key (black) leaves alpha 0, so a keyed pane starts out
// transparent and keeps the soft edges drawn into it. See MODERNIZATION.md,
// "Soft alpha".
//
//////////////////////////////////////////////////////////////////////////////

D3D11Stats g_d3d11Stats = {};

namespace {

struct Vertex2D {
    float x, y;
    float u, v;
    DWORD color;
};

const int c_maxQuads = 1024;

struct Pipeline2D {
    D3D11Device*                   m_pdevice;
    int                            m_generation;

    TRef<ID3D11VertexShader>       m_pvs;
    TRef<ID3D11PixelShader>        m_pps;
    TRef<ID3D11InputLayout>        m_playout;
    TRef<ID3D11Buffer>             m_pcb;
    TRef<ID3D11Buffer>             m_pvb;
    TRef<ID3D11Buffer>             m_pib;
    TRef<ID3D11BlendState>         m_pblendReplace;
    TRef<ID3D11BlendState>         m_pblendOver;
    TRef<ID3D11DepthStencilState>  m_pdepth;
    TRef<ID3D11RasterizerState>    m_praster;
    TRef<ID3D11SamplerState>       m_psampler;

    Pipeline2D() :
        m_pdevice(NULL),
        m_generation(-1)
    {
    }
};

Pipeline2D        g_p2d;
std::vector<DWORD> g_vconvert;

bool CreatePipeline2D(D3D11Device* pdevice)
{
    Pipeline2D& p = g_p2d;

    if (p.m_pdevice == pdevice && p.m_generation == pdevice->GetGeneration() && p.m_pvs != NULL) {
        return true;
    }

    p = Pipeline2D();

    ID3D11Device* pd3d = pdevice->GetDevice();

    if (pd3d == NULL) {
        return false;
    }

    if (FAILED(pd3d->CreateVertexShader(g_Draw2DVs, sizeof(g_Draw2DVs), NULL, &p.m_pvs))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_Draw2DPs, sizeof(g_Draw2DPs), NULL, &p.m_pps))) return false;

    static const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(pd3d->CreateInputLayout(layout, ArrayCount(layout), g_Draw2DVs, sizeof(g_Draw2DVs), &p.m_playout))) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = 8 * sizeof(DWORD);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pd3d->CreateBuffer(&bd, NULL, &p.m_pcb))) return false;

    bd.ByteWidth = c_maxQuads * 4 * sizeof(Vertex2D);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    if (FAILED(pd3d->CreateBuffer(&bd, NULL, &p.m_pvb))) return false;

    //
    // Quads share one index buffer: 0 1 2, 2 1 3 per quad
    //

    std::vector<WORD> indices(c_maxQuads * 6);

    for (int quad = 0; quad < c_maxQuads; quad++) {
        WORD base = (WORD)(quad * 4);
        indices[quad * 6 + 0] = base + 0;
        indices[quad * 6 + 1] = base + 1;
        indices[quad * 6 + 2] = base + 2;
        indices[quad * 6 + 3] = base + 2;
        indices[quad * 6 + 4] = base + 1;
        indices[quad * 6 + 5] = base + 3;
    }

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(WORD));
    ibd.Usage     = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = indices.data();

    if (FAILED(pd3d->CreateBuffer(&ibd, &data, &p.m_pib))) return false;

    //
    // Fills, text and opaque blits replace pixels; keyed blits are blended
    // over them, premultiplied (one, inverse source alpha), colour and alpha
    // alike. No depth; scissored.
    //

    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(pd3d->CreateBlendState(&bld, &p.m_pblendReplace))) return false;

    D3D11_RENDER_TARGET_BLEND_DESC& rt = bld.RenderTarget[0];
    rt.BlendEnable    = TRUE;
    rt.SrcBlend       = D3D11_BLEND_ONE;
    rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp        = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    if (FAILED(pd3d->CreateBlendState(&bld, &p.m_pblendOver))) return false;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    if (FAILED(pd3d->CreateDepthStencilState(&dsd, &p.m_pdepth))) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.ScissorEnable   = TRUE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(pd3d->CreateRasterizerState(&rd, &p.m_praster))) return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    if (FAILED(pd3d->CreateSamplerState(&sd, &p.m_psampler))) return false;

    p.m_pdevice    = pdevice;
    p.m_generation = pdevice->GetGeneration();
    return true;
}

void SetQuad(Vertex2D* pv, const WinRect& rect, float u0, float v0, float u1, float v1, DWORD color)
{
    float x0 = (float)rect.XMin();
    float y0 = (float)rect.YMin();
    float x1 = (float)rect.XMax();
    float y1 = (float)rect.YMax();

    pv[0].x = x0; pv[0].y = y0; pv[0].u = u0; pv[0].v = v0; pv[0].color = color;
    pv[1].x = x1; pv[1].y = y0; pv[1].u = u1; pv[1].v = v0; pv[1].color = color;
    pv[2].x = x0; pv[2].y = y1; pv[2].u = u0; pv[2].v = v1; pv[2].color = color;
    pv[3].x = x1; pv[3].y = y1; pv[3].u = u1; pv[3].v = v1; pv[3].color = color;
}

void DrawQuads(
    D3D11Device*              pdevice,
    D3D11Target*              ptarget,
    const Vertex2D*           pvertex,
    int                       quads,
    UINT                      mode,
    UINT                      keyMode,
    ID3D11ShaderResourceView* psrv,
    const WinRect&            rectScissor
) {
    if (quads <= 0 || !CreatePipeline2D(pdevice) || ptarget->m_prtv == NULL) {
        return;
    }

    D3D11_RECT scissor;
    scissor.left   = MaxOf<LONG>(0,                      rectScissor.XMin());
    scissor.top    = MaxOf<LONG>(0,                      rectScissor.YMin());
    scissor.right  = MinOf<LONG>(ptarget->m_size.X(),    rectScissor.XMax());
    scissor.bottom = MinOf<LONG>(ptarget->m_size.Y(),    rectScissor.YMax());

    if (scissor.left >= scissor.right || scissor.top >= scissor.bottom) {
        return;
    }

    Pipeline2D&          p        = g_p2d;
    ID3D11DeviceContext* pcontext = pdevice->GetContext();

    //
    // Rasterizers and the presenter bind their own state lazily; tell them
    // the context changed, and bind everything here.
    //

    D3D11BoundRasterizerChanged();

    D3D11_MAPPED_SUBRESOURCE mapped;

    if (FAILED(pcontext->Map(p.m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        return;
    }

    float* pf = (float*)mapped.pData;
    pf[0] = 2.0f / ptarget->m_size.X();
    pf[1] = 2.0f / ptarget->m_size.Y();
    pf[2] = 0;
    pf[3] = 0;

    UINT* pu = (UINT*)(pf + 4);
    pu[0] = mode;
    pu[1] = keyMode;
    pu[2] = 0;
    pu[3] = 0;

    pcontext->Unmap(p.m_pcb, 0);

    ID3D11RenderTargetView* prtv = ptarget->m_prtv;
    pcontext->OMSetRenderTargets(1, &prtv, NULL);
    pcontext->OMSetBlendState(mode == 1 && keyMode != D3D11KeyNone ? p.m_pblendOver : p.m_pblendReplace, NULL, 0xffffffff);
    pcontext->OMSetDepthStencilState(p.m_pdepth, 0);

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width    = (float)ptarget->m_size.X();
    vp.Height   = (float)ptarget->m_size.Y();
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    pcontext->RSSetViewports(1, &vp);
    pcontext->RSSetState(p.m_praster);
    pcontext->RSSetScissorRects(1, &scissor);

    UINT stride = sizeof(Vertex2D);
    UINT offset = 0;
    ID3D11Buffer* pvb = p.m_pvb;
    pcontext->IASetInputLayout(p.m_playout);
    pcontext->IASetVertexBuffers(0, 1, &pvb, &stride, &offset);
    pcontext->IASetIndexBuffer(p.m_pib, DXGI_FORMAT_R16_UINT, 0);
    pcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11Buffer* pcb = p.m_pcb;
    pcontext->VSSetShader(p.m_pvs, NULL, 0);
    pcontext->VSSetConstantBuffers(0, 1, &pcb);
    pcontext->PSSetShader(p.m_pps, NULL, 0);
    pcontext->PSSetConstantBuffers(0, 1, &pcb);
    pcontext->PSSetShaderResources(0, 1, &psrv);

    ID3D11SamplerState* psampler = p.m_psampler;
    pcontext->PSSetSamplers(0, 1, &psampler);

    while (quads > 0) {
        int count = MinOf(quads, c_maxQuads);

        if (FAILED(pcontext->Map(p.m_pvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            break;
        }

        memcpy(mapped.pData, pvertex, count * 4 * sizeof(Vertex2D));
        pcontext->Unmap(p.m_pvb, 0);

        pcontext->DrawIndexed(count * 6, 0, 0);

        pvertex += count * 4;
        quads   -= count;
        g_d3d11Stats.m_draws2D++;
    }

    //
    // Don't leave a source bound: it may be the next draw's render target
    //

    ID3D11ShaderResourceView* pnull = NULL;
    pcontext->PSSetShaderResources(0, 1, &pnull);
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
//
// Targets
//
//////////////////////////////////////////////////////////////////////////////

bool D3D11Target::IsValid(D3D11Device* pdevice, const WinPoint& size)
{
    return
           m_ptexture   != NULL
        && m_generation == pdevice->GetGeneration()
        && m_size       == size;
}

bool D3D11EnsureTarget(D3D11Device* pdevice, D3D11Target* ptarget, const WinPoint& size, bool& bRecreated)
{
    bRecreated = false;

    if (ptarget->IsValid(pdevice, size)) {
        return true;
    }

    bRecreated = true;

    ptarget->m_ptexture   = NULL;
    ptarget->m_prtv       = NULL;
    ptarget->m_psrv       = NULL;
    ptarget->m_pdepth     = NULL;
    ptarget->m_pdsv       = NULL;
    ptarget->m_pstaging   = NULL;
    ptarget->m_generation = -1;

    ID3D11Device* pd3d = pdevice->GetDevice();

    if (pd3d == NULL || size.X() <= 0 || size.Y() <= 0) {
        return false;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = size.X();
    td.Height           = size.Y();
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = pd3d->CreateTexture2D(&td, NULL, &ptarget->m_ptexture);
    if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(ptarget->m_ptexture, NULL, &ptarget->m_prtv);
    if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(ptarget->m_ptexture, NULL, &ptarget->m_psrv);

    if (FAILED(hr)) {
        ptarget->m_ptexture = NULL;
        ptarget->m_prtv     = NULL;
        ptarget->m_psrv     = NULL;
        pdevice->CheckDeviceLost(hr);
        return false;
    }

    ptarget->m_size       = size;
    ptarget->m_generation = pdevice->GetGeneration();
    return true;
}

bool D3D11EnsureDepth(D3D11Device* pdevice, D3D11Target* ptarget)
{
    if (ptarget->m_pdsv != NULL) {
        return true;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = ptarget->m_size.X();
    td.Height           = ptarget->m_size.Y();
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Device* pd3d = pdevice->GetDevice();
    HRESULT hr = pd3d->CreateTexture2D(&td, NULL, &ptarget->m_pdepth);
    if (SUCCEEDED(hr)) hr = pd3d->CreateDepthStencilView(ptarget->m_pdepth, NULL, &ptarget->m_pdsv);

    if (FAILED(hr)) {
        ptarget->m_pdepth = NULL;
        ptarget->m_pdsv   = NULL;
        pdevice->CheckDeviceLost(hr);
        return false;
    }

    //
    // Start cleared, as a freshly attached DirectDraw z-buffer was
    //

    pdevice->GetContext()->ClearDepthStencilView(ptarget->m_pdsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// CPU pixels <-> GPU copy
//
//////////////////////////////////////////////////////////////////////////////

void D3D11Upload(D3D11Device* pdevice, D3D11Target* ptarget, const BYTE* psource, int pitch, PixelFormat* ppf)
{
    const WinPoint& size = ptarget->m_size;

    //
    // Alpha is kept: the CPU pixels' alpha 0 (the colour key) stays
    // transparent on the GPU
    //

    g_vconvert.resize(size.X() * size.Y());
    ConvertSurfaceToBGRA(psource, pitch, ppf, size, true, g_vconvert.data(), size.X() * 4);

    pdevice->GetContext()->UpdateSubresource(ptarget->m_ptexture, 0, NULL, g_vconvert.data(), size.X() * 4, 0);
    g_d3d11Stats.m_uploads++;
}

bool D3D11Readback(D3D11Device* pdevice, D3D11Target* ptarget, BYTE* pdest, int pitch, PixelFormat* ppf)
{
    ID3D11DeviceContext* pcontext = pdevice->GetContext();

    if (ptarget->m_pstaging == NULL) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = ptarget->m_size.X();
        td.Height           = ptarget->m_size.Y();
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_STAGING;
        td.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;

        HRESULT hr = pdevice->GetDevice()->CreateTexture2D(&td, NULL, &ptarget->m_pstaging);

        if (FAILED(hr)) {
            pdevice->CheckDeviceLost(hr);
            return false;
        }
    }

    pcontext->CopyResource(ptarget->m_pstaging, ptarget->m_ptexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = pcontext->Map(ptarget->m_pstaging, 0, D3D11_MAP_READ, 0, &mapped);

    if (FAILED(hr)) {
        pdevice->CheckDeviceLost(hr);
        return false;
    }

    ConvertBGRAToSurface((const DWORD*)mapped.pData, mapped.RowPitch, ppf, ptarget->m_size, pdest, pitch);

    pcontext->Unmap(ptarget->m_pstaging, 0);
    g_d3d11Stats.m_readbacks++;
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// 2D drawing
//
//////////////////////////////////////////////////////////////////////////////

DWORD D3D11MakeBGRA(const Color& color)
{
    float r = MaxOf(0.0f, MinOf(1.0f, color.R()));
    float g = MaxOf(0.0f, MinOf(1.0f, color.G()));
    float b = MaxOf(0.0f, MinOf(1.0f, color.B()));
    float a = MaxOf(0.0f, MinOf(1.0f, color.A()));

    return
          (DWORD(a * 255.0f + 0.5f) << 24)
        | (DWORD(r * 255.0f + 0.5f) << 16)
        | (DWORD(g * 255.0f + 0.5f) <<  8)
        |  DWORD(b * 255.0f + 0.5f);
}

void D3D11Blit(
    D3D11Device*              pdevice,
    D3D11Target*              ptarget,
    const WinRect&            rectDest,
    ID3D11ShaderResourceView* psrv,
    const WinPoint&           sizeSource,
    const WinRect&            rectSource,
    D3D11KeyMode              keyMode,
    const WinRect&            rectScissor
) {
    if (psrv == NULL || rectDest.IsEmpty() || rectSource.IsEmpty()) {
        return;
    }

    float sx = 1.0f / sizeSource.X();
    float sy = 1.0f / sizeSource.Y();

    Vertex2D quad[4];
    SetQuad(
        quad,
        rectDest,
        rectSource.XMin() * sx,
        rectSource.YMin() * sy,
        rectSource.XMax() * sx,
        rectSource.YMax() * sy,
        0xffffffff
    );

    DrawQuads(pdevice, ptarget, quad, 1, 1, keyMode, psrv, rectScissor);
}

void D3D11Fill(D3D11Device* pdevice, D3D11Target* ptarget, const WinRect& rect, const Color& color)
{
    if (rect.IsEmpty()) {
        return;
    }

    //
    // Opaque, except the colour key: black (in 565 terms, as the CPU fills
    // do) is transparent, alpha 0
    //

    DWORD bgra = D3D11MakeBGRA(color) | 0xff000000;

    if (D3D11IsKeyBlack(bgra)) {
        bgra = 0;
    }

    Vertex2D quad[4];
    SetQuad(quad, rect, 0, 0, 0, 0, bgra);

    DrawQuads(pdevice, ptarget, quad, 1, 0, 0, NULL, rect);
}

TRef<ID3D11ShaderResourceView> D3D11CreateAtlas(D3D11Device* pdevice, const BYTE* pdata, const WinPoint& size)
{
    ID3D11Device* pd3d = pdevice->GetDevice();

    if (pd3d == NULL || size.X() <= 0 || size.Y() <= 0) {
        return NULL;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = size.X();
    td.Height           = size.Y();
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem     = pdata;
    data.SysMemPitch = size.X();

    TRef<ID3D11Texture2D>          ptexture;
    TRef<ID3D11ShaderResourceView> psrv;

    HRESULT hr = pd3d->CreateTexture2D(&td, &data, &ptexture);
    if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(ptexture, NULL, &psrv);

    if (FAILED(hr)) {
        pdevice->CheckDeviceLost(hr);
        return NULL;
    }

    return psrv;
}

void D3D11DrawGlyphs(
    D3D11Device*              pdevice,
    D3D11Target*              ptarget,
    ID3D11ShaderResourceView* patlas,
    const WinPoint&           sizeAtlas,
    const D3D11Glyph*         pglyphs,
    int                       count,
    const WinRect&            rectScissor
) {
    if (patlas == NULL || count <= 0) {
        return;
    }

    float sx = 1.0f / sizeAtlas.X();
    float sy = 1.0f / sizeAtlas.Y();

    std::vector<Vertex2D> vertices(count * 4);

    for (int index = 0; index < count; index++) {
        const D3D11Glyph& glyph = pglyphs[index];

        SetQuad(
            &vertices[index * 4],
            glyph.m_rectDest,
            glyph.m_rectAtlas.XMin() * sx,
            glyph.m_rectAtlas.YMin() * sy,
            glyph.m_rectAtlas.XMax() * sx,
            glyph.m_rectAtlas.YMax() * sy,
            glyph.m_color
        );
    }

    DrawQuads(pdevice, ptarget, vertices.data(), count, 2, 0, patlas, rectScissor);
}
