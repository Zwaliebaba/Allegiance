#include "bloom.hlsli"

//
// Dual-filter downsample: the centre and four diagonal taps half a source
// pixel out, each a bilinear average of four
//

float4 main(float4 position : SV_Position) : SV_Target
{
    float2 uv = position.xy * g_texel.zw;
    float2 d  = g_texel.xy;

    float3 sum =
          Fetch(uv) * 4.0
        + Fetch(uv + float2(-d.x, -d.y))
        + Fetch(uv + float2( d.x, -d.y))
        + Fetch(uv + float2(-d.x,  d.y))
        + Fetch(uv + float2( d.x,  d.y));

    float3 c = sum / 8.0;

    if (g_bright.z > 0.5) {
        c = BrightPass(c);
    }

    return float4(c, 1.0);
}
