//////////////////////////////////////////////////////////////////////////////
//
// D3D11 replacement for the DirectX 6 fixed-function rasterizer state the
// engine used: pre-transformed screen vertices (D3DTLVERTEX layout), one
// texture, DECAL or MODULATE, colour-key cut-outs, Gouraud or flat shading.
// Transform and lighting stay on the CPU (device3d.cpp).
//
//////////////////////////////////////////////////////////////////////////////

cbuffer RasterConstants : register(b0)
{
    float4 g_screen;    // x: 2 / width, y: 2 / height
    uint4  g_flags;     // x: bit 0 textured, bit 1 decal, bit 2 colour key
};

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VertexIn
{
    float4 position : POSITION;     // x, y in pixels; z in [0, 1]; w = 1 / w
    float4 color    : COLOR0;       // D3DCOLOR, read as B8G8R8A8_UNORM
    float2 uv       : TEXCOORD0;
};

float4 ScreenToClip(float4 p)
{
    //
    // Direct3D 6 put pixel centres on integer coordinates, Direct3D 11 puts
    // them on half-integers: shift by half a pixel so vertices land on the
    // same pixels they always did. Multiplying through by w (= 1 / rhw)
    // gives back a homogeneous position, so the rasterizer interpolates
    // perspective-correctly, as it did for TL vertices.
    //

    float w = p.w > 0 ? 1.0 / p.w : 1.0;
    float x = (p.x + 0.5) * g_screen.x - 1.0;
    float y = 1.0 - (p.y + 0.5) * g_screen.y;

    return float4(x * w, y * w, p.z * w, w);
}

float4 Shade(float4 color, float2 uv)
{
    if (g_flags.x & 1) {
        float4 texel = g_texture.Sample(g_sampler, uv);

        //
        // The texture cache stores the colour key as alpha 0 (the textures
        // themselves have no alpha). Discarding reproduces
        // D3DRENDERSTATE_COLORKEYENABLE.
        //

        if ((g_flags.x & 4) && texel.a < 0.5) {
            discard;
        }

        if (g_flags.x & 2) {
            // D3DTBLEND_DECAL: texture colour, texture alpha (always opaque)
            return float4(texel.rgb, 1.0);
        }

        // D3DTBLEND_MODULATE without texture alpha: alpha from the vertex
        return float4(texel.rgb * color.rgb, color.a);
    }

    return color;
}
