//////////////////////////////////////////////////////////////////////////////
//
// Scene composite: a 3D layer's HDR scene (linear light, premultiplied
// alpha, resolved from MSAA) is tonemapped, gamma-encoded and blended over
// the frame (one, inverse source alpha).
//
// The tonemap leaves values up to the knee unchanged, so the art keeps its
// authored colours, and rolls everything brighter (stacked additive effects:
// engines, explosions, alephs) off smoothly towards white instead of
// clipping each channel.
//
// Alpha is coverage. Additive draws leave it alone (they add light, they
// don't hide what is behind), so where the scene has light but no coverage
// it is added to the frame. Bloom (bloom.hlsli) is light too: it is added
// to the colour, not the coverage.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer SceneConstants : register(b0)
{
    float4 g_tonemap;   // x: knee, y: exposure, z: bloom strength
    float4 g_size;      // xy: 1 / scene size
};

Texture2D<float4> g_scene   : register(t0);
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

float4 main(float4 position : SV_Position) : SV_Target
{
    float4 s = g_scene.Load(int3(position.xy, 0));

    s.rgb += g_bloom.SampleLevel(g_sampler, position.xy * g_size.xy, 0).rgb * g_tonemap.z;

    //
    // Tonemap and encode the colour without its coverage, then apply the
    // coverage again; light without coverage is encoded as it is
    //

    float3 rgb;

    if (s.a > 1.0 / 255.0) {
        rgb = ToGamma(Tonemap(s.rgb / s.a)) * s.a;
    } else {
        rgb = ToGamma(Tonemap(s.rgb));
    }

    return float4(rgb, saturate(s.a));
}
