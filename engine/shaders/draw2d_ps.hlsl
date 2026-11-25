#include "draw2d.hlsli"

float4 main(VertexOut input) : SV_Target
{
    if (g_mode.x == 0) {
        //
        // Fill: the pixel written as it is, as UnclippedFill did
        //

        return float4(input.color.rgb, 1.0);
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
    // Blit. Key mode 1: texture-cache texture, key stored as alpha 0.
    // Key mode 2: another render target; the key is black, and anything that
    // would have been black once quantised to 565 counts, as it did when the
    // pane was read back into 565 pixels.
    //

    if (g_mode.y == 1 && texel.a < 0.5) {
        discard;
    }

    if (g_mode.y == 2 && texel.r < 8.0 / 255.0 && texel.g < 4.0 / 255.0 && texel.b < 8.0 / 255.0) {
        discard;
    }

    return float4(texel.rgb, 1.0);
}
