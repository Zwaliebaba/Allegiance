//////////////////////////////////////////////////////////////////////////////
//
// D3D11 replacement for the DirectX 6 fixed-function rasterizer state the
// engine used: pre-transformed screen vertices (D3DTLVERTEX layout), one
// texture, DECAL or MODULATE, colour-key cut-outs, Gouraud or flat shading.
// Transform and lighting stay on the CPU (device3d.cpp) for these vertices.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer RasterConstants : register(b0)
{
    float4 g_screen;    // x: 2 / width, y: 2 / height
    uint4  g_flags;     // x: bit 0 textured, bit 1 decal, bit 2 colour key, bit 3 key by black,
                        //    bit 4 blended (BlendModeAdd or BlendModeSourceAlpha)
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
    float4 result  = color;
    bool   bBlend  = (g_flags.x & 16) != 0;

    if (g_flags.x & 1) {
        float4 texel = g_texture.Sample(g_sampler, uv);

        //
        // Texture-cache textures hold the image's premultiplied alpha (alpha
        // 0 is the colour key). A texture that is another GPU surface (a pane
        // image) holds coverage, and there an opaque pixel that is black
        // (anything that would have been black in 565, as when the pane was
        // read back into 565 pixels) is the pane's colour key.
        //

        if ((g_flags.x & 12) == 12 && texel.a >= 1.0 && texel.r < 8.0 / 255.0 && texel.g < 4.0 / 255.0 && texel.b < 8.0 / 255.0) {
            texel = float4(0, 0, 0, 0);
        }

        //
        // With the colour key on: blended draws (HUD images, radar) blend
        // the texture's alpha in, so soft edges stay soft; opaque draws
        // (model textures) cut out at half alpha, which reproduces
        // D3DRENDERSTATE_COLORKEYENABLE for the 0 / 255 art. Fully
        // transparent pixels are discarded either way, so they write no
        // depth. With it off the texture is opaque.
        //

        float alpha = 1.0;

        if (g_flags.x & 4) {
            if (texel.a <= 0.0 || (!bBlend && texel.a < 0.5)) {
                discard;
            }

            if (bBlend) {
                alpha = texel.a;
            }
        }

        //
        // One exit point: FXC warns (X4000) about early returns from branches
        // on constant-buffer values
        //

        if (g_flags.x & 2) {
            // D3DTBLEND_DECAL: texture colour and alpha
            result = float4(texel.rgb, alpha);
        } else {
            // D3DTBLEND_MODULATE: alpha from the vertex, times the texture's
            result = float4(texel.rgb * color.rgb, color.a * alpha);
        }
    }

    //
    // The target's alpha is coverage (premultiplied): what an opaque draw
    // covers is opaque
    //

    if (!bBlend) {
        result.a = 1.0;
    }

    return result;
}
