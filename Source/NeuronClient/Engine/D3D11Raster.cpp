#include "pch.h"
#include "D3D11Engine.h"

#include "RasterVs.h"
#include "RasterVsFlat.h"
#include "RasterPs.h"
#include "RasterPsFlat.h"
#include "TlVs.h"
#include "TlVsFlat.h"
#include "TlVsL.h"
#include "TlVsLFlat.h"
#include "TlPs.h"
#include "TlPsFlat.h"
#include "SceneVs.h"
#include "ScenePs.h"
#include "ScenePsMs.h"
#include "BloomDownPs.h"
#include "BloomUpPs.h"
#include "RasterPsOit.h"
#include "RasterPsFlatOit.h"
#include "TlPsOit.h"
#include "TlPsFlatOit.h"
#include "OitCompositePs.h"

//////////////////////////////////////////////////////////////////////////////
//
// D3D11 Rasterizer
//
// Replaces the DirectX 6 D3DRasterizer (raster.cpp) and the software
// rasterizer (softraster.cpp). It takes the same pre-transformed screen
// vertices from Device3D.cpp's CPU transform, lighting and clipping, and
// reproduces the render states that path set.
//
// It draws into the surface's GPU copy (PrivateSurface::BeginGpuDraw), which
// the surface keeps between scenes; 2D drawing into the same surface is done
// on the GPU too, so nothing is copied back to system memory unless
// something reads the pixels.
//
// A 3D layer with a z-buffer (Context::Begin3DLayer) is drawn into a scene
// target instead: RGBA16F in linear light, 4x MSAA where the GPU has the
// memory, with its own depth buffer. At the end of the layer it is resolved,
// tonemapped and blended over the surface's GPU copy (ScenePs.hlsl).
//
//////////////////////////////////////////////////////////////////////////////

namespace {

//
// A 3D layer's HDR scene. Shared by size between rasterizers, one user at a
// time (a pane can render its own 3D layer while the frame's is open).
//

class SceneTarget : public IObject {
public:
    WinPoint                        m_size;
    int                             m_samples;
    int                             m_generation;   // the device's, when made
    bool                            m_bInUse;

    TRef<ID3D11Texture2D>           m_pcolor;       // RGBA16F, m_samples
    TRef<ID3D11RenderTargetView>    m_prtv;
    TRef<ID3D11ShaderResourceView>  m_psrvSamples;  // m_pcolor's samples, for the composite (if multisampled)
    TRef<ID3D11Texture2D>           m_pdepth;       // D32F, m_samples
    TRef<ID3D11DepthStencilView>    m_pdsv;
    TRef<ID3D11DepthStencilView>    m_pdsvReadOnly; // for the replayed blended draws (feature level 11),
    TRef<ID3D11ShaderResourceView>  m_psrvDepth;    // which read the depth through this
    //
    // The translucent pass's targets (made the first time a scene has a
    // translucent draw): accumulation (RGBA16F) and revealage (R16F), with
    // the scene's samples, and one-sample copies the composite reads
    //

    TRef<ID3D11Texture2D>           m_paccum;
    TRef<ID3D11RenderTargetView>    m_prtvAccum;
    TRef<ID3D11Texture2D>           m_preveal;
    TRef<ID3D11RenderTargetView>    m_prtvReveal;
    TRef<ID3D11Texture2D>           m_paccumResolve;
    TRef<ID3D11ShaderResourceView>  m_psrvAccum;
    TRef<ID3D11Texture2D>           m_prevealResolve;
    TRef<ID3D11ShaderResourceView>  m_psrvReveal;

    TRef<ID3D11Texture2D>           m_presolve;     // RGBA16F, one sample
    TRef<ID3D11ShaderResourceView>  m_psrv;

    //
    // Bloom levels: half the scene's size, then half again each
    //

    static const int                c_maxBloomLevels = 6;

    int                             m_bloomLevels;
    WinPoint                        m_sizeBloom[c_maxBloomLevels];
    TRef<ID3D11Texture2D>           m_pbloom[c_maxBloomLevels];
    TRef<ID3D11RenderTargetView>    m_prtvBloom[c_maxBloomLevels];
    TRef<ID3D11ShaderResourceView>  m_psrvBloom[c_maxBloomLevels];

    SceneTarget() :
        m_size(0, 0),
        m_samples(1),
        m_generation(-1),
        m_bInUse(false),
        m_bloomLevels(0)
    {
    }
};

const DXGI_FORMAT c_formatScene      = DXGI_FORMAT_R16G16B16A16_FLOAT;
const DXGI_FORMAT c_formatSceneDepth = DXGI_FORMAT_D32_FLOAT;

//
// The tonemap's knee: scene values up to it are shown as they are. Above it
// they roll off towards white; 1.0 (white art, fully lit) comes out at 0.95
// in linear light, 249 of 255 once encoded.
//

const float c_sceneKnee     = 0.9f;
const float c_sceneExposure = 1.0f;

//
// Bloom: what is brighter than the threshold (with a soft knee below it)
// glows, at this strength
//

const float c_bloomThreshold = 0.9f;
const float c_bloomKnee      = 0.5f;
const float c_bloomStrength  = 0.35f;

//
// Soft particles: a replayed blended draw fades out over this view distance
// in front of the geometry behind it (world units; a fighter is about 10)
//

const float c_softDistance = 3.0f;

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
    TRef<ID3D11PixelShader>        m_ppsTL;             // Vertex: lit per pixel
    TRef<ID3D11PixelShader>        m_ppsTLFlat;
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
    TRef<ID3D11DepthStencilState>  m_pdepthScene[2][2]; // the same, reversed (greater-equal), for scene targets
    TRef<ID3D11DepthStencilState>  m_pdepthAlways;
    TRef<ID3D11RasterizerState>    m_praster[3];        // none, cw, ccw
    TRef<ID3D11RasterizerState>    m_prasterTL[3];      // the same, clipping to near and far
    TRef<ID3D11SamplerState>       m_psampler[2];       // point, linear

    //
    // 3D layers: blend states for the scene target (additive draws leave
    // its coverage alone), the composite, and the scene targets
    //

    TRef<ID3D11BlendState>         m_pblendScene[3];    // BlendMode
    TRef<ID3D11BlendState>         m_pblendSceneCoverage; // opaque cut-outs: alpha to coverage
    TRef<ID3D11BlendState>         m_pblendComposite;
    TRef<ID3D11VertexShader>       m_pvsScene;
    TRef<ID3D11PixelShader>        m_ppsScene;
    TRef<ID3D11PixelShader>        m_ppsSceneMS;        // 4 samples, tonemapped one by one

    //
    // The translucent pass (weighted blended OIT, Raster.hlsli)
    //

    TRef<ID3D11PixelShader>        m_ppsOit;
    TRef<ID3D11PixelShader>        m_ppsOitFlat;
    TRef<ID3D11PixelShader>        m_ppsTLOit;
    TRef<ID3D11PixelShader>        m_ppsTLOitFlat;
    TRef<ID3D11PixelShader>        m_ppsOitComposite;
    TRef<ID3D11BlendState>         m_pblendOit;         // accumulate (one, one); reveal (zero, 1 - source)
    TRef<ID3D11Buffer>             m_pcbScene;
    TRef<ID3D11PixelShader>        m_ppsBloomDown;
    TRef<ID3D11PixelShader>        m_ppsBloomUp;
    TRef<ID3D11Buffer>             m_pcbBloom;
    TRef<ID3D11BlendState>         m_pblendBloomAdd;
    TRef<ID3D11SamplerState>       m_psamplerClamp;     // linear, clamped: bloom
    int                            m_samplesScene;
    bool                           m_bSoftParticles;    // the scene depth can be read while bound (feature level 11)
    std::vector<TRef<SceneTarget>> m_vscenes;

    Pipeline() :
        m_pdevice(nullptr),
        m_generation(-1),
        m_vbCapacity(0),
        m_ibCapacity(0),
        m_samplesScene(1),
        m_bSoftParticles(false)
    {
    }
};

Pipeline   g_pipeline;
Rasterizer* g_pboundRasterizer = nullptr;

bool CreatePipeline(D3D11Device* pdevice)
{
    Pipeline& p = g_pipeline;

    if (p.m_pdevice == pdevice && p.m_generation == pdevice->GetGeneration() && p.m_pvs != nullptr) {
        return true;
    }

    p = Pipeline();
    g_pboundRasterizer = nullptr;

    ID3D11Device* pd3d = pdevice->GetDevice();

    if (pd3d == nullptr) {
        return false;
    }

    if (FAILED(pd3d->CreateVertexShader(g_RasterVs,      sizeof(g_RasterVs),      nullptr, &p.m_pvs    ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_RasterVsFlat, sizeof(g_RasterVsFlat), nullptr, &p.m_pvsFlat))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_TlVs,          sizeof(g_TlVs),          nullptr, &p.m_pvsTL     ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_TlVsFlat,     sizeof(g_TlVsFlat),     nullptr, &p.m_pvsTLFlat ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_TlVsL,        sizeof(g_TlVsL),        nullptr, &p.m_pvsTLL    ))) return false;
    if (FAILED(pd3d->CreateVertexShader(g_TlVsLFlat,   sizeof(g_TlVsLFlat),   nullptr, &p.m_pvsTLLFlat))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_RasterPs,      sizeof(g_RasterPs),      nullptr, &p.m_pps    ))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_RasterPsFlat, sizeof(g_RasterPsFlat), nullptr, &p.m_ppsFlat))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_TlPs,          sizeof(g_TlPs),          nullptr, &p.m_ppsTL    ))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_TlPsFlat,     sizeof(g_TlPsFlat),     nullptr, &p.m_ppsTLFlat))) return false;

    //
    // VertexScreen: x y z rhw, color, specular (unused), u v
    //

    static const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(pd3d->CreateInputLayout(layout, ArrayCount(layout), g_RasterVs, sizeof(g_RasterVs), &p.m_playout))) return false;

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

    if (FAILED(pd3d->CreateInputLayout(layoutTL,  ArrayCount(layoutTL),  g_TlVs,   sizeof(g_TlVs),   &p.m_playoutTL ))) return false;
    if (FAILED(pd3d->CreateInputLayout(layoutTLL, ArrayCount(layoutTLL), g_TlVsL, sizeof(g_TlVsL), &p.m_playoutTLL))) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = 8 * sizeof(DWORD);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pd3d->CreateBuffer(&bd, nullptr, &p.m_pcb))) return false;

    bd.ByteWidth = 18 * 4 * sizeof(float);   // TLConstants
    if (FAILED(pd3d->CreateBuffer(&bd, nullptr, &p.m_pcbTL))) return false;

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
    // Scene target blending: the same colour blends; alpha is coverage, so
    // BlendModeAdd keeps it (light added in front of nothing stays see-through,
    // and the composite adds it to what is behind)
    //

    for (int mode = 0; mode < 3; mode++) {
        D3D11_BLEND_DESC desc = {};
        D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];

        rt.BlendEnable           = mode != BlendModeSource;
        rt.SrcBlend              = D3D11_BLEND_ONE;
        rt.DestBlend             = mode == BlendModeAdd ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp               = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha         = mode == BlendModeAdd ? D3D11_BLEND_ZERO : D3D11_BLEND_ONE;
        rt.DestBlendAlpha        = mode == BlendModeAdd ? D3D11_BLEND_ONE  : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendScene[mode]))) return false;
    }

    //
    // Opaque cut-outs (colour-keyed model textures) in a multisampled scene:
    // the shader's alpha says how many of a pixel's samples the surface
    // covers, so a cut-out's edge is antialiased like a triangle's. The
    // samples it covers are opaque: colour as it is, alpha from the blend
    // factor (the default, 1), not from the shader.
    //

    {
        D3D11_BLEND_DESC desc = {};
        desc.AlphaToCoverageEnable = TRUE;

        D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];
        rt.BlendEnable           = TRUE;
        rt.SrcBlend              = D3D11_BLEND_ONE;
        rt.DestBlend             = D3D11_BLEND_ZERO;
        rt.BlendOp               = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha         = D3D11_BLEND_BLEND_FACTOR;
        rt.DestBlendAlpha        = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendSceneCoverage))) return false;
    }

    {
        D3D11_BLEND_DESC desc = {};
        D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];

        rt.BlendEnable           = TRUE;
        rt.SrcBlend              = D3D11_BLEND_ONE;
        rt.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp               = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
        rt.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendComposite))) return false;
    }

    if (FAILED(pd3d->CreateVertexShader(g_SceneVs, sizeof(g_SceneVs), nullptr, &p.m_pvsScene))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_ScenePs, sizeof(g_ScenePs), nullptr, &p.m_ppsScene))) return false;
    if (FAILED(pd3d->CreatePixelShader (g_ScenePsMs, sizeof(g_ScenePsMs), nullptr, &p.m_ppsSceneMS))) return false;

    if (FAILED(pd3d->CreatePixelShader(g_RasterPsOit,      sizeof(g_RasterPsOit),      nullptr, &p.m_ppsOit         ))) return false;
    if (FAILED(pd3d->CreatePixelShader(g_RasterPsFlatOit, sizeof(g_RasterPsFlatOit), nullptr, &p.m_ppsOitFlat     ))) return false;
    if (FAILED(pd3d->CreatePixelShader(g_TlPsOit,          sizeof(g_TlPsOit),          nullptr, &p.m_ppsTLOit       ))) return false;
    if (FAILED(pd3d->CreatePixelShader(g_TlPsFlatOit,     sizeof(g_TlPsFlatOit),     nullptr, &p.m_ppsTLOitFlat   ))) return false;
    if (FAILED(pd3d->CreatePixelShader(g_OitCompositePs,   sizeof(g_OitCompositePs),   nullptr, &p.m_ppsOitComposite))) return false;

    {
        D3D11_BLEND_DESC desc = {};
        desc.IndependentBlendEnable = TRUE;

        D3D11_RENDER_TARGET_BLEND_DESC& accum = desc.RenderTarget[0];
        accum.BlendEnable           = TRUE;
        accum.SrcBlend              = D3D11_BLEND_ONE;
        accum.DestBlend             = D3D11_BLEND_ONE;
        accum.BlendOp               = D3D11_BLEND_OP_ADD;
        accum.SrcBlendAlpha         = D3D11_BLEND_ONE;
        accum.DestBlendAlpha        = D3D11_BLEND_ONE;
        accum.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        accum.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        D3D11_RENDER_TARGET_BLEND_DESC& reveal = desc.RenderTarget[1];
        reveal.BlendEnable           = TRUE;
        reveal.SrcBlend              = D3D11_BLEND_ZERO;
        reveal.DestBlend             = D3D11_BLEND_INV_SRC_COLOR;
        reveal.BlendOp               = D3D11_BLEND_OP_ADD;
        reveal.SrcBlendAlpha         = D3D11_BLEND_ZERO;
        reveal.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        reveal.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        reveal.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendOit))) return false;
    }

    if (FAILED(pd3d->CreatePixelShader(g_BloomDownPs, sizeof(g_BloomDownPs), nullptr, &p.m_ppsBloomDown))) return false;
    if (FAILED(pd3d->CreatePixelShader(g_BloomUpPs,   sizeof(g_BloomUpPs),   nullptr, &p.m_ppsBloomUp  ))) return false;

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth      = 8 * sizeof(float);
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pd3d->CreateBuffer(&desc, nullptr, &p.m_pcbScene))) return false;
        if (FAILED(pd3d->CreateBuffer(&desc, nullptr, &p.m_pcbBloom))) return false;
    }

    {
        D3D11_BLEND_DESC desc = {};
        D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];

        rt.BlendEnable           = TRUE;
        rt.SrcBlend              = D3D11_BLEND_ONE;
        rt.DestBlend             = D3D11_BLEND_ONE;
        rt.BlendOp               = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
        rt.DestBlendAlpha        = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pd3d->CreateBlendState(&desc, &p.m_pblendBloomAdd))) return false;
    }

    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD         = D3D11_FLOAT32_MAX;

        if (FAILED(pd3d->CreateSamplerState(&desc, &p.m_psamplerClamp))) return false;
    }

    //
    // 4x MSAA for the scene where both formats support it and the GPU is a
    // real one with the memory for it (at 2560x1440 the scene's colour and
    // depth take about 180 MB)
    //

    {
        UINT qualityColor = 0;
        UINT qualityDepth = 0;

        if (
               pdevice->IsHardware()
            && pdevice->GetTotalVideoMemory() >= 1024 * 1024 * 1024
            && SUCCEEDED(pd3d->CheckMultisampleQualityLevels(c_formatScene,      4, &qualityColor))
            && SUCCEEDED(pd3d->CheckMultisampleQualityLevels(c_formatSceneDepth, 4, &qualityDepth))
            && qualityColor > 0
            && qualityDepth > 0
        ) {
            p.m_samplesScene = 4;
        }
    }

    //
    // Read-only depth views, which a draw can test against while it reads
    // the depth as a texture, need feature level 11
    //

    p.m_bSoftParticles = pd3d->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;

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

            desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
            if (FAILED(pd3d->CreateDepthStencilState(&desc, &p.m_pdepthScene[test][write]))) return false;
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
        // Device3D.cpp's clipper did
        //

        desc.DepthClipEnable = TRUE;

        if (FAILED(pd3d->CreateRasterizerState(&desc, &p.m_prasterTL[index]))) return false;
    }

    //
    // Samplers: wrap addressing. The filtered one is anisotropic (16x), so
    // model textures stay sharp at grazing angles; their baked mip chains
    // keep distant ones from shimmering. Screen-aligned images look the
    // same as with plain bilinear.
    //

    for (int linear = 0; linear < 2; linear++) {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter         = linear ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.MaxAnisotropy  = linear ? 16 : 1;
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
    if (pbuffer != nullptr && capacity >= bytes) {
        return true;
    }

    int size = MaxOf(bytes, 64 * 1024);

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = size;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    pbuffer  = nullptr;
    capacity = 0;

    if (FAILED(g_pipeline.m_pdevice->GetDevice()->CreateBuffer(&desc, nullptr, &pbuffer))) {
        return false;
    }

    capacity = size;
    return true;
}

//
// A free scene target of the size, made if there is none
//

TRef<SceneTarget> AcquireSceneTarget(D3D11Device* pdevice, const WinPoint& size)
{
    Pipeline& p = g_pipeline;

    for (int index = 0; index < (int)p.m_vscenes.size(); index++) {
        SceneTarget* pscene = p.m_vscenes[index];

        if (!pscene->m_bInUse && pscene->m_size == size) {
            pscene->m_bInUse = true;
            return pscene;
        }
    }

    ID3D11Device*     pd3d   = pdevice->GetDevice();
    TRef<SceneTarget> pscene = new SceneTarget();

    pscene->m_size       = size;
    pscene->m_samples    = p.m_samplesScene;
    pscene->m_generation = pdevice->GetGeneration();

    D3D11_TEXTURE2D_DESC td = {};
    td.Width              = size.X();
    td.Height             = size.Y();
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = c_formatScene;
    td.SampleDesc.Count   = pscene->m_samples;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | (pscene->m_samples > 1 ? D3D11_BIND_SHADER_RESOURCE : 0);

    HRESULT hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_pcolor);
    if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(pscene->m_pcolor, nullptr, &pscene->m_prtv);
    if (SUCCEEDED(hr) && pscene->m_samples > 1) hr = pd3d->CreateShaderResourceView(pscene->m_pcolor, nullptr, &pscene->m_psrvSamples);

    //
    // The depth, readable by the replayed blended draws where the device
    // allows it: typeless, with a depth view, a read-only one and a texture
    // view
    //

    bool bReadable = p.m_bSoftParticles;
    bool bSamples  = pscene->m_samples > 1;

    if (SUCCEEDED(hr)) {
        td.Format    = bReadable ? DXGI_FORMAT_R32_TYPELESS : c_formatSceneDepth;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | (bReadable ? D3D11_BIND_SHADER_RESOURCE : 0);
        hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_pdepth);
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
    dsvd.Format        = c_formatSceneDepth;
    dsvd.ViewDimension = bSamples ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;

    if (SUCCEEDED(hr)) hr = pd3d->CreateDepthStencilView(pscene->m_pdepth, &dsvd, &pscene->m_pdsv);

    if (SUCCEEDED(hr) && bReadable) {
        dsvd.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        hr = pd3d->CreateDepthStencilView(pscene->m_pdepth, &dsvd, &pscene->m_pdsvReadOnly);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format              = DXGI_FORMAT_R32_FLOAT;
        srvd.ViewDimension       = bSamples ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;

        if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(pscene->m_pdepth, &srvd, &pscene->m_psrvDepth);
    }

    if (SUCCEEDED(hr)) {
        td.Format           = c_formatScene;
        td.SampleDesc.Count = 1;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_presolve);
    }
    if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(pscene->m_presolve, nullptr, &pscene->m_psrv);

    //
    // Bloom levels, down to about 16 pixels
    //

    WinPoint sizeLevel = size;

    while (SUCCEEDED(hr) && pscene->m_bloomLevels < SceneTarget::c_maxBloomLevels) {
        sizeLevel = WinPoint((sizeLevel.X() + 1) / 2, (sizeLevel.Y() + 1) / 2);

        if (pscene->m_bloomLevels > 0 && (sizeLevel.X() < 16 || sizeLevel.Y() < 16)) {
            break;
        }

        int level = pscene->m_bloomLevels;

        td.Width            = sizeLevel.X();
        td.Height           = sizeLevel.Y();
        td.Format           = c_formatScene;
        td.SampleDesc.Count = 1;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_pbloom[level]);
        if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(pscene->m_pbloom[level], nullptr, &pscene->m_prtvBloom[level]);
        if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(pscene->m_pbloom[level], nullptr, &pscene->m_psrvBloom[level]);

        if (SUCCEEDED(hr)) {
            pscene->m_sizeBloom[level] = sizeLevel;
            pscene->m_bloomLevels++;
        }
    }

    if (FAILED(hr)) {
        pdevice->CheckDeviceLost(hr);
        return nullptr;
    }

    pscene->m_bInUse = true;
    p.m_vscenes.push_back(pscene);
    return pscene;
}

//
// The translucent pass's targets of a scene, made on first use (at 2560x1440
// with 4x MSAA they take about 150 MB)
//

bool EnsureOitTargets(D3D11Device* pdevice, SceneTarget* pscene)
{
    if (pscene->m_prtvAccum != nullptr) {
        return true;
    }

    ID3D11Device* pd3d = pdevice->GetDevice();

    D3D11_TEXTURE2D_DESC td = {};
    td.Width              = pscene->m_size.X();
    td.Height             = pscene->m_size.Y();
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.SampleDesc.Count   = pscene->m_samples;
    td.Usage              = D3D11_USAGE_DEFAULT;

    //
    // One sample: the targets are read directly; several: resolved first
    //

    bool bSamples = pscene->m_samples > 1;

    td.Format    = c_formatScene;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | (bSamples ? 0 : D3D11_BIND_SHADER_RESOURCE);
    HRESULT hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_paccum);
    if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(pscene->m_paccum, nullptr, &pscene->m_prtvAccum);

    if (SUCCEEDED(hr)) {
        td.Format = DXGI_FORMAT_R16_FLOAT;
        hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_preveal);
    }
    if (SUCCEEDED(hr)) hr = pd3d->CreateRenderTargetView(pscene->m_preveal, nullptr, &pscene->m_prtvReveal);

    if (SUCCEEDED(hr)) {
        if (bSamples) {
            td.SampleDesc.Count = 1;
            td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
            td.Format           = c_formatScene;
            hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_paccumResolve);
            if (SUCCEEDED(hr)) {
                td.Format = DXGI_FORMAT_R16_FLOAT;
                hr = pd3d->CreateTexture2D(&td, nullptr, &pscene->m_prevealResolve);
            }
        } else {
            pscene->m_paccumResolve  = pscene->m_paccum;
            pscene->m_prevealResolve = pscene->m_preveal;
        }
    }
    if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(pscene->m_paccumResolve,  nullptr, &pscene->m_psrvAccum);
    if (SUCCEEDED(hr)) hr = pd3d->CreateShaderResourceView(pscene->m_prevealResolve, nullptr, &pscene->m_psrvReveal);

    if (FAILED(hr)) {
        pdevice->CheckDeviceLost(hr);
        pscene->m_prtvAccum = nullptr;
        return false;
    }

    return true;
}

} // namespace

void D3D11BoundRasterizerChanged()
{
    g_pboundRasterizer = nullptr;
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
    // While a 3D layer with a z-buffer is open: its scene target, and the
    // clip rects drawn through (what the composite covers)
    //

    TRef<SceneTarget>              m_pscene;
    Rect                           m_rectSceneDrawn;
    bool                           m_bSceneDrawn;

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

    //
    // Blended draws in a scene, deferred until its opaque geometry is drawn
    // (see Defer): each with the state it was drawn in and a copy of its
    // vertices
    //

    struct DeferredDraw {
        ShadeMode                  m_shadeMode;
        BlendMode                  m_blendMode;
        CullMode                   m_cullMode;
        bool                       m_bLinearFilter;
        bool                       m_bColorKey;
        TRef<Surface>              m_psurfaceTexture;
        Rect                       m_rectClip;

        D3D11_PRIMITIVE_TOPOLOGY   m_topology;
        bool                       m_bTranslucent;  // for the translucent pass: blended over, or a translucent texture
        bool                       m_bModel;        // DrawModel's, with m_tl; else screen vertices
        bool                       m_bNormal;
        int                        m_stride;
        TransformAndLight          m_tl;
        std::vector<BYTE>          m_vvertex;
        std::vector<MeshIndex>     m_vindex;
    };

    std::vector<DeferredDraw>      m_vdeferred;
    bool                           m_bReplaying;
    bool                           m_bOit;            // replaying into the translucent pass's targets
    float                          m_sceneDepth[2];   // the scene's depth to view distance, from its model draws

public:
    D3D11RasterizerImpl(PrivateSurface* psurface) :
        m_psurface(psurface),
        m_prtvBound(nullptr),
        m_sizeTarget(0, 0),
        m_bInScene(false),
        m_bSceneDrawn(false),
        m_shadeMode(ShadeModeGouraud),
        m_blendMode(BlendModeSource),
        m_cullMode(CullModeNone),
        m_bZTest(false),
        m_bZWrite(false),
        m_bLinearFilter(true),
        m_bColorKey(true),
        m_bStateChanged(true),
        m_flagsBound(0xffffffff),
        m_bReplaying(false),
        m_bOit(false)
    {
        m_sceneDepth[0] = 0;
        m_sceneDepth[1] = 0;

        CastTo(m_pengine, psurface->GetEngine());

        WinPoint size = psurface->GetSize();
        m_rectClip = Rect(0, 0, (float)size.X(), (float)size.Y());
    }

    ~D3D11RasterizerImpl()
    {
        Terminate();
    }

    bool IsValid() override
    {
        D3D11Device* pdevice = m_pengine->GetD3D11Device();
        return pdevice != nullptr && CreatePipeline(pdevice);
    }

    void Terminate() override
    {
        if (g_pboundRasterizer == this) {
            g_pboundRasterizer = nullptr;
        }

        if (m_pscene != nullptr) {
            m_pscene->m_bInUse = false;
            m_pscene = nullptr;
        }

        m_vdeferred.clear();
        m_ptarget   = nullptr;
        m_prtvBound = nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Render target management
    //
    //////////////////////////////////////////////////////////////////////////////

    D3D11Device* GetDevice()
    {
        D3D11Device* pdevice = m_pengine->GetD3D11Device();

        if (pdevice == nullptr || !CreatePipeline(pdevice)) {
            return nullptr;
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

        if (pdevice == nullptr) {
            return false;
        }

        D3D11Target* ptarget = m_psurface->BeginGpuDraw();

        if (ptarget == nullptr) {
            return false;
        }

        //
        // A 3D layer draws into its scene target, which has its own depth
        //

        //
        // (A device reset or a resize during the layer drops the scene; the
        // rest of the layer draws directly)
        //

        if (
               m_pscene != nullptr
            && (   m_pscene->m_size       != ptarget->m_size
                || m_pscene->m_generation != pdevice->GetGeneration())
        ) {
            m_pscene->m_bInUse = false;
            m_pscene = nullptr;
            m_vdeferred.clear();
        }

        if (m_pscene == nullptr && !D3D11EnsureDepth(pdevice, ptarget)) {
            return false;
        }

        ID3D11RenderTargetView* prtv = m_pscene != nullptr ? m_pscene->m_prtv : ptarget->m_prtv;
        ID3D11DepthStencilView* pdsv = m_pscene != nullptr ? m_pscene->m_pdsv : ptarget->m_pdsv;

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        if (g_pboundRasterizer != this || m_prtvBound != prtv) {
            g_pboundRasterizer = this;
            m_ptarget          = ptarget;
            m_prtvBound        = prtv;
            m_sizeTarget       = ptarget->m_size;
            m_bStateChanged    = true;
            m_flagsBound       = 0xffffffff;

            pcontext->OMSetRenderTargets(1, &prtv, pdsv);

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

        ID3D11ShaderResourceView* psrv = nullptr;

        bool bKeyBlack = false;

        if (!bPoints && m_psurfaceTexture != nullptr) {
            PrivateSurface* ptexture; CastTo(ptexture, (Surface*)m_psurfaceTexture);

            //
            // A texture that is itself a GPU surface (a pane image) is read
            // directly; its alpha is coverage, and opaque black is its colour
            // key too
            //

            psrv = ptexture->GetGpuSource();

            if (psrv != nullptr) {
                bKeyBlack = true;
            } else {
                psrv = pdevice->GetTexture(ptexture);
            }
        }

        bool  bFlat   = !bPoints && m_shadeMode == ShadeModeFlat;
        bool  bDecal  = m_shadeMode == ShadeModeNone || m_shadeMode == ShadeModeCopy;
        bool  bBlend  = !bPoints && m_blendMode != BlendModeSource;

        //
        // An opaque cut-out in a multisampled scene: alpha to coverage
        //

        bool  bCoverage =
               !bPoints
            && !bBlend
            && m_bColorKey
            && psrv != nullptr
            && m_pscene != nullptr
            && m_pscene->m_samples > 1;

        DWORD flags   =
              (psrv != nullptr       ? 1 : 0)
            | (bDecal             ? 2 : 0)
            | (m_bColorKey        ? 4 : 0)
            | (bKeyBlack          ? 8 : 0)
            | (bBlend             ? 16 : 0)
            | (m_pscene != nullptr   ? 32 | 64 : 0)
            | (bCoverage          ? 128 : 0);

        //
        // A replayed blended draw fades against the scene's depth, which it
        // reads through the read-only view the replay bound
        //

        bool bSoft =
               m_bReplaying
            && bBlend
            && m_pscene != nullptr
            && m_pscene->m_psrvDepth != nullptr
            && m_sceneDepth[1] != 0;

        if (bSoft) {
            flags |= 256 | (m_pscene->m_samples > 1 ? 512 : 0);
        }

        if (flags != m_flagsBound) {
            D3D11_MAPPED_SUBRESOURCE mapped;

            if (SUCCEEDED(pcontext->Map(p.m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                float* pf = (float*)mapped.pData;
                pf[0] = 2.0f / m_sizeTarget.X();
                pf[1] = 2.0f / m_sizeTarget.Y();
                pf[2] = m_sceneDepth[0];
                pf[3] = m_sceneDepth[1];

                float softDistance = c_softDistance;

                DWORD* pdw = (DWORD*)(pf + 4);
                pdw[0] = flags;
                memcpy(&pdw[1], &softDistance, sizeof(float));
                pdw[2] = 0;
                pdw[3] = 0;

                pcontext->Unmap(p.m_pcb, 0);
            }

            m_flagsBound = flags;
        }

        pcontext->VSSetShader(bFlat ? p.m_pvsFlat : p.m_pvs, nullptr, 0);
        pcontext->PSSetShader(
            m_bOit
                ? (bFlat ? p.m_ppsOitFlat : p.m_ppsOit)
                : (bFlat ? p.m_ppsFlat    : p.m_pps),
            nullptr,
            0
        );
        pcontext->PSSetShaderResources(0, 1, &psrv);

        ID3D11SamplerState* psampler = p.m_psampler[m_bLinearFilter ? 1 : 0];
        pcontext->PSSetSamplers(0, 1, &psampler);

        TRef<ID3D11BlendState>* pblend = m_pscene != nullptr ? p.m_pblendScene : p.m_pblend;

        if (bPoints) {
            pcontext->OMSetBlendState(pblend[BlendModeSource], nullptr, 0xffffffff);
            pcontext->OMSetDepthStencilState(p.m_pdepth[0][0], 0);
            pcontext->RSSetState(p.m_praster[0]);
        } else {
            pcontext->OMSetBlendState(
                m_bOit
                    ? (ID3D11BlendState*)p.m_pblendOit
                    : (bCoverage ? (ID3D11BlendState*)p.m_pblendSceneCoverage : (ID3D11BlendState*)pblend[m_blendMode]),
                nullptr,
                0xffffffff
            );
            //
            // Replayed blended draws write no depth: they are see-through,
            // and the depth may be bound read-only
            //

            pcontext->OMSetDepthStencilState(
                (m_pscene != nullptr ? p.m_pdepthScene : p.m_pdepth)[m_bZTest ? 1 : 0][m_bZWrite && !m_bReplaying ? 1 : 0],
                0
            );

            int cull = m_cullMode == CullModeCW ? 1 : (m_cullMode == CullModeCCW ? 2 : 0);
            pcontext->RSSetState(p.m_praster[cull]);
        }

        D3D11_RECT scissor;
        scissor.left   = MaxOf<LONG>(0,             (LONG)floorf(m_rectClip.XMin()));
        scissor.top    = MaxOf<LONG>(0,             (LONG)floorf(m_rectClip.YMin()));
        scissor.right  = MinOf((LONG)m_sizeTarget.X(), (LONG)ceilf(m_rectClip.XMax()));
        scissor.bottom = MinOf((LONG)m_sizeTarget.Y(), (LONG)ceilf(m_rectClip.YMax()));
        pcontext->RSSetScissorRects(1, &scissor);

        if (m_pscene != nullptr) {
            if (m_bSceneDrawn) {
                m_rectSceneDrawn.Accumulate(m_rectClip);
            } else {
                m_rectSceneDrawn = m_rectClip;
                m_bSceneDrawn    = true;
            }
        }

        m_bStateChanged = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // State
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetShadeMode(ShadeMode shadeMode) override                    { m_shadeMode     = shadeMode;      m_bStateChanged = true; }
    void SetBlendMode(BlendMode blendMode) override                    { m_blendMode     = blendMode;      m_bStateChanged = true; }
    void SetCullMode(CullMode cullMode) override                       { m_cullMode      = cullMode;       m_bStateChanged = true; }
    void SetZTest(bool bZTest) override                                { m_bZTest        = bZTest;         m_bStateChanged = true; }
    void SetZWrite(bool bZWrite) override                              { m_bZWrite       = bZWrite;        m_bStateChanged = true; }
    void SetLinearFilter(bool bLinearFilter) override                  { m_bLinearFilter = bLinearFilter;  m_bStateChanged = true; }
    void SetColorKey(bool bColorKey) override                          { m_bColorKey     = bColorKey;      m_bStateChanged = true; }
    void SetTexture(Surface* psurfaceTexture) override                 { m_psurfaceTexture = psurfaceTexture; m_bStateChanged = true; }

    //
    // Always on with hardware, ignored, and cylindrical wrapping (unused by
    // the game), as in the DirectX 6 rasterizer
    //

    void SetPerspectiveCorrection(bool bPerspectiveCorrection) override {}
    void SetDither(bool bDither) override                               {}
    void SetWrapMode(WrapMode wrapMode) override                        {}

    void SetClipRect(const Rect& rectClip) override
    {
        m_rectClip      = rectClip;
        m_bStateChanged = true;
    }

    Point GetSurfaceSize() override
    {
        return Point::Cast(m_psurface->GetSize());
    }

    bool Has3DAcceleration() override
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

    void BeginScene() override
    {
        m_bInScene = true;
    }

    void EndScene() override
    {
        m_bInScene = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // 3D layers: a layer with a z-buffer draws into a cleared scene target,
    // which is composited over the surface when the layer ends. Layers
    // without one (overlays drawn over what is there) draw directly.
    //
    //////////////////////////////////////////////////////////////////////////////

    void Begin3DLayer(bool bZBuffer) override
    {
        if (!bZBuffer || m_pscene != nullptr) {
            return;
        }

        D3D11Device* pdevice = GetDevice();

        if (pdevice == nullptr) {
            return;
        }

        m_pscene = AcquireSceneTarget(pdevice, m_psurface->GetSize());

        if (m_pscene == nullptr) {
            return;
        }

        static const float transparent[4] = { 0, 0, 0, 0 };

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        pcontext->ClearRenderTargetView(m_pscene->m_prtv, transparent);
        pcontext->ClearDepthStencilView(m_pscene->m_pdsv, D3D11_CLEAR_DEPTH, 0.0f, 0);   // reversed: far is 0

        m_bSceneDrawn   = false;
        m_bStateChanged = true;
        m_prtvBound     = nullptr;   // rebind to the scene

        m_vdeferred.clear();
        m_sceneDepth[0] = 0;
        m_sceneDepth[1] = 0;
    }

    void End3DLayer() override
    {
        if (m_pscene == nullptr) {
            return;
        }

        FlushDeferred();

        TRef<SceneTarget> pscene = m_pscene;

        m_pscene->m_bInUse = false;
        m_pscene           = nullptr;
        m_bStateChanged    = true;
        m_prtvBound        = nullptr;   // rebind to the surface
        g_pboundRasterizer = nullptr;

        D3D11Device* pdevice = GetDevice();

        if (pdevice == nullptr || !m_bSceneDrawn) {
            return;
        }

        D3D11Target* ptarget = m_psurface->BeginGpuDraw();

        if (
               ptarget == nullptr
            || ptarget->m_size       != pscene->m_size
            || pscene->m_generation  != pdevice->GetGeneration()
        ) {
            return;
        }

        ID3D11DeviceContext* pcontext = pdevice->GetContext();
        Pipeline&            p        = g_pipeline;

        //
        // Resolve the samples
        //

        pcontext->OMSetRenderTargets(0, nullptr, nullptr);

        if (pscene->m_samples > 1) {
            pcontext->ResolveSubresource(pscene->m_presolve, 0, pscene->m_pcolor, 0, c_formatScene);
        } else {
            pcontext->CopyResource(pscene->m_presolve, pscene->m_pcolor);
        }

        DrawBloom(pcontext, pscene);

        //
        // Composite over the surface, through the clip rects the layer drew in
        //

        D3D11_MAPPED_SUBRESOURCE mapped;

        if (SUCCEEDED(pcontext->Map(p.m_pcbScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* pf = (float*)mapped.pData;
            pf[0] = c_sceneKnee;
            pf[1] = c_sceneExposure;
            pf[2] = pscene->m_bloomLevels > 0 ? c_bloomStrength : 0.0f;
            pf[3] = 0;
            pf[4] = 1.0f / pscene->m_size.X();
            pf[5] = 1.0f / pscene->m_size.Y();
            pf[6] = 0;
            pf[7] = 0;
            pcontext->Unmap(p.m_pcbScene, 0);
        }

        ID3D11RenderTargetView* prtv = ptarget->m_prtv;
        pcontext->OMSetRenderTargets(1, &prtv, nullptr);

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width    = (float)ptarget->m_size.X();
        vp.Height   = (float)ptarget->m_size.Y();
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        pcontext->RSSetViewports(1, &vp);

        D3D11_RECT scissor;
        scissor.left   = MaxOf<LONG>(0, (LONG)floorf(m_rectSceneDrawn.XMin()));
        scissor.top    = MaxOf<LONG>(0, (LONG)floorf(m_rectSceneDrawn.YMin()));
        scissor.right  = MinOf((LONG)ptarget->m_size.X(), (LONG)ceilf(m_rectSceneDrawn.XMax()));
        scissor.bottom = MinOf((LONG)ptarget->m_size.Y(), (LONG)ceilf(m_rectSceneDrawn.YMax()));
        pcontext->RSSetScissorRects(1, &scissor);

        ID3D11Buffer*             pcb      = p.m_pcbScene;
        ID3D11SamplerState*       psampler = p.m_psamplerClamp;
        //
        // A multisampled scene is composited from its samples, each
        // tonemapped on its own (Scene.hlsli); the resolve above is for the
        // bloom
        //

        bool bSamples = pscene->m_samples > 1;

        ID3D11ShaderResourceView* psrvs[2] = {
            bSamples ? (ID3D11ShaderResourceView*)pscene->m_psrvSamples : (ID3D11ShaderResourceView*)pscene->m_psrv,
            pscene->m_bloomLevels > 0 ? (ID3D11ShaderResourceView*)pscene->m_psrvBloom[0] : nullptr
        };

        pcontext->OMSetBlendState(p.m_pblendComposite, nullptr, 0xffffffff);
        pcontext->OMSetDepthStencilState(p.m_pdepth[0][0], 0);
        pcontext->RSSetState(p.m_praster[0]);
        pcontext->IASetInputLayout(nullptr);
        pcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pcontext->VSSetShader(p.m_pvsScene, nullptr, 0);
        pcontext->PSSetShader(bSamples ? p.m_ppsSceneMS : p.m_ppsScene, nullptr, 0);
        pcontext->PSSetConstantBuffers(0, 1, &pcb);
        pcontext->PSSetShaderResources(0, 2, psrvs);
        pcontext->PSSetSamplers(0, 1, &psampler);
        pcontext->Draw(3, 0);

        ID3D11ShaderResourceView* pnull[2] = { nullptr, nullptr };
        pcontext->PSSetShaderResources(0, 2, pnull);

        g_d3d11Stats.m_scenes++;
    }

    //
    // Bloom: filter the resolved scene down the levels (the first pass keeps
    // only the bright part), then add each level into the one above it
    //

    void DrawBloomPass(
        ID3D11DeviceContext*      pcontext,
        ID3D11ShaderResourceView* psource,
        ID3D11RenderTargetView*   pdest,
        const WinPoint&           sizeDest,
        ID3D11PixelShader*        pps,
        ID3D11BlendState*         pblend,
        bool                      bFirst
    ) {
        Pipeline& p = g_pipeline;

        D3D11_MAPPED_SUBRESOURCE mapped;

        if (SUCCEEDED(pcontext->Map(p.m_pcbBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* pf = (float*)mapped.pData;
            pf[0] = 0.5f / sizeDest.X();    // tap spacing: half a destination pixel
            pf[1] = 0.5f / sizeDest.Y();
            pf[2] = 1.0f / sizeDest.X();
            pf[3] = 1.0f / sizeDest.Y();
            pf[4] = c_bloomThreshold;
            pf[5] = c_bloomKnee;
            pf[6] = bFirst ? 1.0f : 0.0f;
            pf[7] = 0;
            pcontext->Unmap(p.m_pcbBloom, 0);
        }

        pcontext->OMSetRenderTargets(1, &pdest, nullptr);

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width    = (float)sizeDest.X();
        vp.Height   = (float)sizeDest.Y();
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        pcontext->RSSetViewports(1, &vp);

        D3D11_RECT scissor = { 0, 0, sizeDest.X(), sizeDest.Y() };
        pcontext->RSSetScissorRects(1, &scissor);

        ID3D11Buffer* pcb = p.m_pcbBloom;

        pcontext->OMSetBlendState(pblend, nullptr, 0xffffffff);
        pcontext->PSSetShader(pps, nullptr, 0);
        pcontext->PSSetConstantBuffers(0, 1, &pcb);
        pcontext->PSSetShaderResources(0, 1, &psource);
        pcontext->Draw(3, 0);

        ID3D11ShaderResourceView* pnull = nullptr;
        pcontext->PSSetShaderResources(0, 1, &pnull);
    }

    void DrawBloom(ID3D11DeviceContext* pcontext, SceneTarget* pscene)
    {
        Pipeline& p = g_pipeline;
        int       levels = pscene->m_bloomLevels;

        if (levels == 0) {
            return;
        }

        ID3D11SamplerState* psampler = p.m_psamplerClamp;

        pcontext->OMSetDepthStencilState(p.m_pdepth[0][0], 0);
        pcontext->RSSetState(p.m_praster[0]);
        pcontext->IASetInputLayout(nullptr);
        pcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pcontext->VSSetShader(p.m_pvsScene, nullptr, 0);
        pcontext->PSSetSamplers(0, 1, &psampler);

        for (int level = 0; level < levels; level++) {
            DrawBloomPass(
                pcontext,
                level == 0 ? (ID3D11ShaderResourceView*)pscene->m_psrv : (ID3D11ShaderResourceView*)pscene->m_psrvBloom[level - 1],
                pscene->m_prtvBloom[level],
                pscene->m_sizeBloom[level],
                p.m_ppsBloomDown,
                p.m_pblend[BlendModeSource],
                level == 0
            );
        }

        for (int level = levels - 2; level >= 0; level--) {
            DrawBloomPass(
                pcontext,
                pscene->m_psrvBloom[level + 1],
                pscene->m_prtvBloom[level],
                pscene->m_sizeBloom[level],
                p.m_ppsBloomUp,
                p.m_pblendBloomAdd,
                false
            );
        }

        pcontext->OMSetRenderTargets(0, nullptr, nullptr);
    }

    void ClearZBuffer() override
    {
        //
        // The blended draws so far were drawn against this depth
        //

        FlushDeferred();

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
            //
            // Far is 0 in a scene's reversed depth; the partial clear below
            // needs nothing special, as its z of 1 is flipped to 0 there
            //

            if (m_pscene != nullptr) {
                pcontext->ClearDepthStencilView(m_pscene->m_pdsv, D3D11_CLEAR_DEPTH, 0.0f, 0);
            } else {
                pcontext->ClearDepthStencilView(m_ptarget->m_pdsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            }
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
        pcontext->OMSetBlendState(g_pipeline.m_pblendNoColor, nullptr, 0xffffffff);
        pcontext->OMSetDepthStencilState(g_pipeline.m_pdepthAlways, 0);

        Draw(pdevice, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, quad, sizeof(VertexScreen), 4, indices, 6);

        //
        // Not a visible change: no readback needed for this alone
        //

        m_bStateChanged = true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Blended draws after the opaque ones
    //
    // In a 3D layer's scene a blended draw that tests depth is recorded, and
    // drawn once the scene's opaque geometry is in: when the layer ends, or
    // before the z-buffer is cleared or something is drawn without a depth
    // test (which keeps their order). Drawn in place, a translucent surface
    // wrote depth that hid the opaque geometry drawn after it behind it, and
    // it could not read the depth behind it to fade where it meets it (soft
    // particles). The replay keeps the draws' order; the effects Context
    // sorts are back to front already.
    //
    //////////////////////////////////////////////////////////////////////////////

    //
    // Whether to record this draw; one that keeps its place among the
    // recorded ones (drawn without a depth test) draws those first
    //

    bool Defer(bool bPoints)
    {
        if (m_pscene == nullptr || m_bReplaying) {
            return false;
        }

        if (!bPoints && m_bZTest && (m_blendMode != BlendModeSource || IsTranslucentTexture())) {
            return true;
        }

        if (bPoints || !m_bZTest) {
            FlushDeferred();
        }

        return false;
    }

    //
    // An opaque draw of a texture whose alpha is translucency, not a
    // cut-out's (AlphaCoverage.h): drawn in the translucent pass, blended
    //

    bool IsTranslucentTexture()
    {
        if (m_blendMode != BlendModeSource || !m_bColorKey || m_psurfaceTexture == nullptr) {
            return false;
        }

        PrivateSurface* ptexture; CastTo(ptexture, (Surface*)m_psurfaceTexture);

        if (ptexture->GetGpuSource() != nullptr) {
            return false;
        }

        D3D11Device* pdevice = GetDevice();
        return pdevice != nullptr && pdevice->IsTranslucent(ptexture);
    }

    DeferredDraw& Record(
        D3D11_PRIMITIVE_TOPOLOGY topology,
        const void*              pvertex,
        int                      stride,
        int                      vcount,
        const MeshIndex*         pindex,
        int                      icount
    ) {
        m_vdeferred.emplace_back();

        DeferredDraw& draw = m_vdeferred.back();
        draw.m_shadeMode       = m_shadeMode;
        draw.m_blendMode       = m_blendMode;
        draw.m_cullMode        = m_cullMode;
        draw.m_bLinearFilter   = m_bLinearFilter;
        draw.m_bColorKey       = m_bColorKey;
        draw.m_psurfaceTexture = m_psurfaceTexture;
        draw.m_rectClip        = m_rectClip;
        draw.m_topology        = topology;
        draw.m_bTranslucent    = m_blendMode == BlendModeSourceAlpha;
        draw.m_bModel          = false;
        draw.m_bNormal         = false;
        draw.m_stride          = stride;
        draw.m_vvertex.assign((const BYTE*)pvertex, (const BYTE*)pvertex + vcount * stride);

        if (pindex != nullptr && icount > 0) {
            draw.m_vindex.assign(pindex, pindex + icount);
        }

        if (IsTranslucentTexture()) {
            draw.m_blendMode    = BlendModeSourceAlpha;
            draw.m_bTranslucent = true;
        }

        return draw;
    }

    //
    // The replay's targets: the scene (or the translucent pass's two), the
    // depth read-only, and the depth as a texture for the soft fade
    //

    void BindReplayTargets(ID3D11DeviceContext* pcontext, bool bOitTargets)
    {
        bool bSamples = m_pscene->m_samples > 1;

        ID3D11RenderTargetView*   prtvs[2] = { m_pscene->m_prtv, nullptr };
        ID3D11ShaderResourceView* psrvs[2] = {
            bSamples ? (ID3D11ShaderResourceView*)m_pscene->m_psrvDepth : nullptr,
            bSamples ? nullptr : (ID3D11ShaderResourceView*)m_pscene->m_psrvDepth
        };

        if (bOitTargets) {
            prtvs[0] = m_pscene->m_prtvAccum;
            prtvs[1] = m_pscene->m_prtvReveal;
        }

        pcontext->OMSetRenderTargets(bOitTargets ? 2 : 1, prtvs, m_pscene->m_pdsvReadOnly);
        pcontext->PSSetShaderResources(1, 2, psrvs);
    }

    void ReplayDraw(DeferredDraw& draw)
    {
        m_shadeMode       = draw.m_shadeMode;
        m_blendMode       = draw.m_blendMode;
        m_cullMode        = draw.m_cullMode;
        m_bZTest          = true;
        m_bZWrite         = false;
        m_bLinearFilter   = draw.m_bLinearFilter;
        m_bColorKey       = draw.m_bColorKey;
        m_psurfaceTexture = draw.m_psurfaceTexture;
        m_rectClip        = draw.m_rectClip;
        m_bStateChanged   = true;

        int              vcount = (int)draw.m_vvertex.size() / draw.m_stride;
        int              icount = (int)draw.m_vindex.size();
        const MeshIndex* pindex = icount > 0 ? draw.m_vindex.data() : nullptr;

        if (draw.m_bModel) {
            DrawModel(draw.m_topology, draw.m_tl, draw.m_vvertex.data(), draw.m_stride, draw.m_bNormal, vcount, pindex, icount);
        } else {
            DrawPrimitive(draw.m_topology, (const VertexScreen*)draw.m_vvertex.data(), vcount, pindex, icount, false);
        }
    }

    //
    // The translucent pass's average, over the scene (OitCompositePs.hlsl)
    //

    void CompositeOit(ID3D11DeviceContext* pcontext)
    {
        Pipeline&    p      = g_pipeline;
        SceneTarget* pscene = m_pscene;

        ID3D11RenderTargetView* prtv = pscene->m_prtv;
        pcontext->OMSetRenderTargets(1, &prtv, pscene->m_pdsvReadOnly);

        if (pscene->m_samples > 1) {
            pcontext->ResolveSubresource(pscene->m_paccumResolve,  0, pscene->m_paccum,  0, c_formatScene);
            pcontext->ResolveSubresource(pscene->m_prevealResolve, 0, pscene->m_preveal, 0, DXGI_FORMAT_R16_FLOAT);
        }

        D3D11_RECT scissor = { 0, 0, (LONG)pscene->m_size.X(), (LONG)pscene->m_size.Y() };

        ID3D11ShaderResourceView* psrvs[3] = { pscene->m_psrvAccum, pscene->m_psrvReveal, nullptr };

        pcontext->PSSetShaderResources(0, 3, psrvs);
        pcontext->OMSetBlendState(p.m_pblendComposite, nullptr, 0xffffffff);
        pcontext->OMSetDepthStencilState(p.m_pdepth[0][0], 0);
        pcontext->RSSetState(p.m_praster[0]);
        pcontext->RSSetScissorRects(1, &scissor);
        pcontext->IASetInputLayout(nullptr);
        pcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pcontext->VSSetShader(p.m_pvsScene, nullptr, 0);
        pcontext->PSSetShader(p.m_ppsOitComposite, nullptr, 0);
        pcontext->Draw(3, 0);

        ID3D11ShaderResourceView* pnull[3] = { nullptr, nullptr, nullptr };
        pcontext->PSSetShaderResources(0, 3, pnull);

        //
        // The rasterizer's own bindings again (Bind only makes them when it
        // binds the target)
        //

        pcontext->IASetInputLayout(p.m_playout);
        m_bStateChanged = true;
        m_flagsBound    = 0xffffffff;
    }

    void FlushDeferred()
    {
        if (m_vdeferred.empty() || m_bReplaying) {
            return;
        }

        std::vector<DeferredDraw> vdraws;
        vdraws.swap(m_vdeferred);

        D3D11Device* pdevice;

        if (m_pscene == nullptr || !Bind(pdevice) || m_pscene == nullptr) {
            return;
        }

        ID3D11DeviceContext* pcontext = pdevice->GetContext();

        //
        // The scene's depth, to test against read-only and to read; without
        // it (below feature level 11) the draws are replayed in order into
        // the scene, translucent ones blended over
        //

        bool bRead = m_pscene->m_pdsvReadOnly != nullptr;
        bool bOit  = false;

        if (bRead) {
            for (const DeferredDraw& draw : vdraws) {
                bOit = bOit || draw.m_bTranslucent;
            }

            bOit = bOit && EnsureOitTargets(pdevice, m_pscene);
        }

        ShadeMode     shadeMode       = m_shadeMode;
        BlendMode     blendMode       = m_blendMode;
        CullMode      cullMode        = m_cullMode;
        bool          bZTest          = m_bZTest;
        bool          bZWrite         = m_bZWrite;
        bool          bLinearFilter   = m_bLinearFilter;
        bool          bColorKey       = m_bColorKey;
        TRef<Surface> psurfaceTexture = m_psurfaceTexture;
        Rect          rectClip        = m_rectClip;

        m_bReplaying = true;
        m_flagsBound = 0xffffffff;

        //
        // The translucent pass first: its draws in any order into its two
        // targets, then their average over the scene. Then the rest (the
        // additive effects), in their order, so light shows over glass.
        //

        if (bOit) {
            static const float zero[4] = { 0, 0, 0, 0 };
            static const float one[4]  = { 1, 1, 1, 1 };

            pcontext->ClearRenderTargetView(m_pscene->m_prtvAccum,  zero);
            pcontext->ClearRenderTargetView(m_pscene->m_prtvReveal, one);

            BindReplayTargets(pcontext, true);
            m_bOit = true;

            for (DeferredDraw& draw : vdraws) {
                if (draw.m_bTranslucent) {
                    ReplayDraw(draw);
                }
            }

            m_bOit = false;

            ID3D11ShaderResourceView* pnull[2] = { nullptr, nullptr };
            pcontext->PSSetShaderResources(1, 2, pnull);
            CompositeOit(pcontext);
        }

        if (bRead) {
            BindReplayTargets(pcontext, false);
        }

        for (DeferredDraw& draw : vdraws) {
            if (!bOit || !draw.m_bTranslucent) {
                ReplayDraw(draw);
            }
        }

        m_bReplaying      = false;
        m_shadeMode       = shadeMode;
        m_blendMode       = blendMode;
        m_cullMode        = cullMode;
        m_bZTest          = bZTest;
        m_bZWrite         = bZWrite;
        m_bLinearFilter   = bLinearFilter;
        m_bColorKey       = bColorKey;
        m_psurfaceTexture = psurfaceTexture;
        m_rectClip        = rectClip;
        m_bStateChanged   = true;
        m_flagsBound      = 0xffffffff;

        if (bRead) {
            ID3D11ShaderResourceView* pnull[2] = { nullptr, nullptr };
            pcontext->PSSetShaderResources(1, 2, pnull);
            m_prtvBound = nullptr;   // the next draw binds the writable depth again
        }
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
        if (vcount <= 0 || (pindex != nullptr && icount <= 0)) {
            return;
        }

        if (Defer(bPoints)) {
            Record(topology, pvertex, sizeof(VertexScreen), vcount, pindex, pindex != nullptr ? icount : 0);
            return;
        }

        D3D11Device* pdevice;

        if (!Bind(pdevice)) {
            return;
        }

        ApplyState(pdevice, bPoints);
        Draw(pdevice, topology, pvertex, sizeof(VertexScreen), vcount, pindex, pindex != nullptr ? icount : 0);
        g_d3d11Stats.m_draws3D++;
    }

    void DrawTriangles(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, pvertex, vcount, pindex, icount, false);
    }

    void DrawLines(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_LINELIST, pvertex, vcount, pindex, icount, false);
    }

    void DrawPoints(const VertexScreen* pvertex, int vcount) override
    {
        DrawPrimitive(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, pvertex, vcount, nullptr, 0, true);
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
        float m_eye[4];
        float m_zrev[4];
    };

    static_assert(sizeof(TLConstants) == 18 * 4 * sizeof(float), "TLConstants must match Tl.hlsli and the buffer size");

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
        if (vcount <= 0 || pindex == nullptr || icount <= 0) {
            return;
        }

        //
        // The scene's projection, for the replayed draws' soft fade
        //

        if (m_pscene != nullptr && tl.m_depth[1] != 0) {
            m_sceneDepth[0] = tl.m_depth[0];
            m_sceneDepth[1] = tl.m_depth[1];
        }

        if (Defer(false)) {
            DeferredDraw& draw = Record(topology, pvertex, stride, vcount, pindex, icount);
            draw.m_bModel  = true;
            draw.m_bNormal = bNormal;
            draw.m_tl      = tl;
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
        memcpy(pc->m_eye,      tl.m_eye,      sizeof(pc->m_eye));
        memcpy(pc->m_zrev,     tl.m_zrev,     sizeof(pc->m_zrev));

        pc->m_params[0] = tl.m_ambient;
        pc->m_params[1] = 0;
        pc->m_params[2] = 0;
        pc->m_params[3] = 0;
        pc->m_mode[0]   = tl.m_mode;
        pc->m_mode[1]   = MinOf(tl.m_countPlanes, c_maxTLClipPlanes);
        pc->m_mode[2]   = m_pscene != nullptr ? 1 : 0;   // linear light
        pc->m_mode[3]   = m_pscene != nullptr ? 1 : 0;   // reversed depth

        pcontext->Unmap(p.m_pcbTL, 0);

        bool bFlat = m_shadeMode == ShadeModeFlat;
        int  cull  = m_cullMode == CullModeCW ? 1 : (m_cullMode == CullModeCCW ? 2 : 0);

        ID3D11Buffer* pcb = p.m_pcbTL;
        pcontext->VSSetConstantBuffers(1, 1, &pcb);
        pcontext->VSSetShader(
            bNormal
                ? (bFlat ? p.m_pvsTLFlat  : p.m_pvsTL )
                : (bFlat ? p.m_pvsTLLFlat : p.m_pvsTLL),
            nullptr,
            0
        );

        //
        // Vertex geometry is lit per pixel (ApplyState sets the pixel
        // shader again for the next draw)
        //

        if (bNormal) {
            pcontext->PSSetConstantBuffers(1, 1, &pcb);
            pcontext->PSSetShader(
                m_bOit
                    ? (bFlat ? p.m_ppsTLOitFlat : p.m_ppsTLOit)
                    : (bFlat ? p.m_ppsTLFlat    : p.m_ppsTL),
                nullptr,
                0
            );
        }
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

    void DrawTriangles(const TransformAndLight& tl, const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, tl, pvertex, sizeof(Vertex), true, vcount, pindex, icount);
    }

    void DrawTriangles(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, tl, pvertex, sizeof(VertexL), false, vcount, pindex, icount);
    }

    void DrawLines(const TransformAndLight& tl, const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        DrawModel(D3D11_PRIMITIVE_TOPOLOGY_LINELIST, tl, pvertex, sizeof(Vertex), true, vcount, pindex, icount);
    }

    void DrawLines(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) override
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

    return nullptr;
}
