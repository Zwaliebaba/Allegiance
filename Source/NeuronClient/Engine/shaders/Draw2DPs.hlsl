#include "Draw2D.hlsli"

//
// Targets and texture-cache textures hold premultiplied alpha (see
// D3D11Draw2D.cpp). Blits of colour keyed images are blended over the target;
// everything else replaces it.
//

bool IsKeyBlack(float3 rgb)
{
    return rgb.r < 8.0 / 255.0 && rgb.g < 4.0 / 255.0 && rgb.b < 8.0 / 255.0;
}

float4 main(VertexOut input) : SV_Target
{
    if (g_mode.x == 0) {
        //
        // Fill: the colour as it is; the colour key comes in as (0, 0, 0, 0)
        //

        return input.color;
    }

    float4 texel = g_texture.Sample(g_sampler, input.uv);

    if (g_mode.x == 2) {
        //
        // Glyphs: set bits take the string colour, others are left alone
        //

        if (texel.r < 0.5) {
            discard;
        }

        return float4(input.color.rgb, 1.0);
    }

    //
    // Blit. Key mode 1: texture-cache texture, whose alpha is the image's.
    // Key mode 2: another render target, whose alpha is coverage; there an
    // opaque pixel that is black (anything that would have been black once
    // quantised to 565) is the pane's colour key, as when the pane was read
    // back into 565 pixels.
    //

    if (g_mode.y == 0) {
        return float4(texel.rgb, 1.0);
    }

    if (g_mode.y == 2 && texel.a >= 1.0 && IsKeyBlack(texel.rgb)) {
        discard;
    }

    if (texel.a <= 0.0) {
        discard;
    }

    return texel;
}
