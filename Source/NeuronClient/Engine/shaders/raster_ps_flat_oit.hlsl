#define OIT_PASS
#include "raster.hlsli"

// The translucent pass (raster.hlsli, Oit)

OitOut main(float4 position : SV_Position, nointerpolation float4 color : COLOR0, float2 uv : TEXCOORD0)
{
    float depth = position.z;
    return Oit(Shade(color, uv, position), depth);
}
