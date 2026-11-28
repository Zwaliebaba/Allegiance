#include "Bloom.hlsli"

//
// Dual-filter upsample of the smaller level (a tent of eight taps), added to
// the larger one by the blend state (one, one)
//

float4 main(float4 position : SV_Position) : SV_Target
{
    float2 uv = position.xy * g_texel.zw;
    float2 d  = g_texel.xy;

    float3 sum =
          Fetch(uv + float2(-2.0 * d.x, 0.0))
        + Fetch(uv + float2( 2.0 * d.x, 0.0))
        + Fetch(uv + float2(0.0, -2.0 * d.y))
        + Fetch(uv + float2(0.0,  2.0 * d.y))
        + Fetch(uv + float2(-d.x, -d.y)) * 2.0
        + Fetch(uv + float2( d.x, -d.y)) * 2.0
        + Fetch(uv + float2(-d.x,  d.y)) * 2.0
        + Fetch(uv + float2( d.x,  d.y)) * 2.0;

    return float4(sum / 12.0, 1.0);
}
