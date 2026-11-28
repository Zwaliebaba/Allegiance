//////////////////////////////////////////////////////////////////////////////
//
// Scene composite: a 3D layer's HDR scene (linear light, premultiplied
// alpha, resolved from MSAA) is tonemapped, gamma-encoded and blended over
// the frame (one, inverse source alpha).
//
// The tonemap leaves values up to the knee (0.9) unchanged, so the art keeps its
// authored colours, and rolls everything brighter (stacked additive effects:
// engines, explosions, alephs) off smoothly towards white instead of
// clipping each channel.
//
// Alpha is coverage. Additive draws leave it alone (they add light, they
// don't hide what is behind), so where the scene has light but no coverage
// it is added to the frame. Bloom (Bloom.hlsli) is light too: it is added
// to the colour, not the coverage.
//
// A multisampled scene is composited sample by sample (ScenePsMs.hlsl):
// each sample is tonemapped and encoded before they are averaged, so where
// an engine's glow at ten times white meets the dark hull behind it the
// edge is the average of what both look like, not a white smear from
// averaging the light first.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer SceneConstants : register(b0)
{
    float4 g_tonemap;   // x: knee, y: exposure, z: bloom strength
    float4 g_size;      // xy: 1 / scene size
};

Texture2D<float4> g_bloom   : register(t1);
SamplerState      g_sampler : register(s0);

float3 Tonemap(float3 x)
{
    float3 c    = x * g_tonemap.y;
    float  knee = g_tonemap.x;
    float3 over = max(c - knee, 0.0);

    return min(c, knee) + (1.0 - knee) * over / (over + (1.0 - knee));
}

float3 ToGamma(float3 c)
{
    return pow(abs(c), 1.0 / 2.2);
}

//
// One sample (or pixel) of the scene, with the bloom at its pixel: tonemap
// and encode the colour without its coverage, then apply the coverage
// again; light without coverage is encoded as it is
//

float4 Composite(float4 s, float3 bloom)
{
    s.rgb += bloom;

    float3 rgb;

    if (s.a > 1.0 / 255.0) {
        rgb = ToGamma(Tonemap(s.rgb / s.a)) * s.a;
    } else {
        rgb = ToGamma(Tonemap(s.rgb));
    }

    return float4(rgb, saturate(s.a));
}

float3 Bloom(float2 position)
{
    return g_bloom.SampleLevel(g_sampler, position * g_size.xy, 0).rgb * g_tonemap.z;
}
