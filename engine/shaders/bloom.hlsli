//////////////////////////////////////////////////////////////////////////////
//
// Bloom on a 3D layer's HDR scene: a chain of half-size levels, each
// filtered down from the one above (the first from the resolved scene,
// keeping only its bright part), then added back up, level by level, with a
// tent filter. The composite (scene_ps.hlsl) adds the top level before the
// tonemap. Stacked additive light (engines, weapons, explosions, alephs) is
// what goes over the threshold; lit hulls stay under it.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer BloomConstants : register(b0)
{
    float4 g_texel;     // xy: 1 / source size, zw: 1 / destination size
    float4 g_bright;    // x: threshold, y: soft knee, z: 1 on the first pass
};

Texture2D    g_source  : register(t0);
SamplerState g_sampler : register(s0);

float3 Fetch(float2 uv)
{
    return g_source.SampleLevel(g_sampler, uv, 0).rgb;
}

//
// The bright part of a colour, with a quadratic knee below the threshold so
// bloom fades in rather than switching on
//

float3 BrightPass(float3 c)
{
    float brightness = max(c.r, max(c.g, c.b));
    float knee       = g_bright.x * g_bright.y + 1e-5;
    float soft       = clamp(brightness - g_bright.x + knee, 0.0, 2.0 * knee);
    soft             = soft * soft / (4.0 * knee);

    float contribution = max(soft, brightness - g_bright.x) / max(brightness, 1e-5);

    return c * contribution;
}
