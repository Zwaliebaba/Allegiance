//////////////////////////////////////////////////////////////////////////////
//
// The translucent pass's composite (raster.hlsli, Oit): the weighted
// average of the translucent fragments, over the scene by how much they
// hide (1 - revealage). Premultiplied, blended (one, inverse source alpha),
// so the scene's coverage takes it in too. Drawn with scene_vs.hlsl over
// the scene, before the additive effects and the tonemap.
//
//////////////////////////////////////////////////////////////////////////////

Texture2D<float4> g_accum  : register(t0);
Texture2D<float>  g_reveal : register(t1);

float4 main(float4 position : SV_Position) : SV_Target
{
    int3  pixel  = int3(position.xy, 0);
    float reveal = g_reveal.Load(pixel);

    if (reveal >= 1.0) {
        discard;    // nothing translucent here
    }

    float4 accum = g_accum.Load(pixel);
    float3 color = accum.rgb / max(accum.a, 1e-5);

    return float4(color * (1.0 - reveal), 1.0 - reveal);
}
