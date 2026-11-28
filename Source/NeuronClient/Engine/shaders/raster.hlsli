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
    float4 g_screen;    // x: 2 / width, y: 2 / height; zw: the scene's depth to view distance (SoftFade)
    uint4  g_flags;     // y: soft fade distance (a float), z, w: 0
                        // x: bit 0 textured, bit 1 decal, bit 2 colour key, bit 3 key by black,
                        //    bit 4 blended (BlendModeAdd or BlendModeSourceAlpha),
                        //    bit 5 linear light (drawing into a 3D layer's HDR scene),
                        //    bit 6 reversed depth (the scene's float depth buffer),
                        //    bit 7 alpha to coverage (an opaque cut-out in a multisampled scene),
                        //    bit 8 soft fade against the scene's depth, bit 9 that depth is multisampled
};

//
// The art and the vertex colours are gamma-encoded (authored for a gamma
// display). A 3D layer is drawn in linear light into its HDR scene target,
// so they are decoded first; the scene composite encodes the result again.
//

float3 ToLinear(float3 c)
{
    return pow(abs(c), 2.2);
}

float4 VertexColor(float4 color)
{
    float4 result = color;

    if (g_flags.x & 32) {
        result.rgb = ToLinear(color.rgb);
    }

    return result;
}

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

//
// Soft particles: a blended draw replayed after the scene's opaque geometry
// (d3d11raster.cpp) reads the scene's depth, read-only, and fades out as it
// nears what is behind it, instead of cutting a hard line where a glow or a
// trail meets a hull. Reversed depth is (1 + a) - b / d for view distance d,
// for screen vertices and model-space ones alike, so d = b / ((1 + a) - z).
//

Texture2DMS<float, 4> g_depthSamples : register(t1);
Texture2D<float>      g_depth        : register(t2);

float ViewDistance(float depth)
{
    float den = g_screen.z - depth;
    float d   = den != 0.0 ? g_screen.w / den : 0.0;

    return d > 0.0 ? d : 1.0e30;
}

float SoftFade(float4 position)
{
    float fade = 1.0;

    if (g_flags.x & 256) {
        int2  pixel = int2(position.xy);
        float depth;

        //
        // The nearest of the samples (reversed: the largest), so the fade
        // follows the front of an edge
        //

        if (g_flags.x & 512) {
            depth = max(
                max(g_depthSamples.Load(pixel, 0), g_depthSamples.Load(pixel, 1)),
                max(g_depthSamples.Load(pixel, 2), g_depthSamples.Load(pixel, 3))
            );
        } else {
            depth = g_depth.Load(int3(pixel, 0));
        }

        float gap = ViewDistance(depth) - ViewDistance(position.z);
        fade      = saturate(gap / asfloat(g_flags.y));
    }

    return fade;
}

//
// RASTER_PIXEL: included by another stage's pixel shader (tl_ps.hlsl) for
// Shade only
//

#ifndef RASTER_PIXEL

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

    //
    // Screen vertices come with conventional depth from the CPU; the
    // scene's depth buffer is reversed (1 near, 0 far). The CPU's z already
    // carries its own rounding, so these keep their precision, not more.
    //

    float z = (g_flags.x & 64) ? 1.0 - p.z : p.z;

    return float4(x * w, y * w, z * w, w);
}

#endif

float4 Shade(float4 color, float2 uv, float4 position)
{
    float4 result   = color;
    bool   bBlend   = (g_flags.x & 16) != 0;
    float  coverage = 1.0;

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
        // An opaque draw shows a cut-out's colour, not its premultiplied
        // colour: a filtered edge texel at alpha 0.6 holds 60% of its
        // colour, which drawn opaque was a dark fringe around every cut-out
        //

        if ((g_flags.x & 4) && !bBlend && texel.a > 0.0) {
            texel.rgb = saturate(texel.rgb / texel.a);
        }

        if (g_flags.x & 32) {
            texel.rgb = ToLinear(texel.rgb);
        }

        //
        // Alpha to coverage: the filtered alpha, sharpened to a ramp about
        // a pixel wide around 0.5, is the share of the pixel's samples the
        // cut-out covers. The edge stays where the 50% cut put it, at any
        // distance, but antialiased. (fwidth here is in uniform control
        // flow: the branches above and around depend on constants only.)
        //

        if (g_flags.x & 128) {
            float ramp = max(fwidth(texel.a), 1.0 / 255.0);
            coverage   = saturate((texel.a - 0.5) / ramp + 0.5);
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
            if (g_flags.x & 128) {
                if (coverage <= 0.0) {
                    discard;
                }
            } else if (texel.a <= 0.0 || (!bBlend && texel.a < 0.5)) {
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
    // covers is opaque. With alpha to coverage the alpha here is how many
    // samples are covered; the blend state stores 1 in each of them.
    //

    if (!bBlend) {
        result.a = coverage;
    } else {
        // Premultiplied fading: the draw adds and hides less
        result *= SoftFade(position);
    }

    return result;
}
