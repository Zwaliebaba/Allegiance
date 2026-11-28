#include "Scene.hlsli"

//////////////////////////////////////////////////////////////////////////////
//
// Scene composite, one sample per pixel (see Scene.hlsli)
//
//////////////////////////////////////////////////////////////////////////////

Texture2D<float4> g_scene : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
    return Composite(g_scene.Load(int3(position.xy, 0)), Bloom(position.xy));
}
