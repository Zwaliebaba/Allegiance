//////////////////////////////////////////////////////////////////////////////
//
// 2D drawing into GPU-resident surfaces: blits, fills and text, the CPU
// loops of surface.cpp and font.cpp done as quads. Positions are pixel
// edges (y down), so a quad covers exactly the pixels whose centres it
// contains, and point sampling at those centres hits exactly one texel.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer Draw2DConstants : register(b0)
{
    float4 g_target;    // x: 2 / width, y: 2 / height
    uint4  g_mode;      // x: 0 fill, 1 blit, 2 glyphs; y: colour key mode
};

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VertexIn
{
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VertexOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};
