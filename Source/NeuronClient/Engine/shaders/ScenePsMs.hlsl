#include "Scene.hlsli"

//////////////////////////////////////////////////////////////////////////////
//
// Scene composite of a 4x multisampled scene: each sample tonemapped and
// encoded, then averaged (see Scene.hlsli). The bloom is smooth already, so
// it is sampled once per pixel.
//
//////////////////////////////////////////////////////////////////////////////

Texture2DMS<float4, 4> g_scene : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
    int2   pixel  = int2(position.xy);
    float3 bloom  = Bloom(position.xy);
    float4 result = 0;

    [unroll]
    for (int sample = 0; sample < 4; sample++) {
        result += Composite(g_scene.Load(pixel, sample), bloom);
    }

    return result * 0.25;
}
