#define OIT_PASS
#include "Raster.hlsli"

// The translucent pass (Raster.hlsli, Oit)

OitOut main(float4 position : SV_Position, float4 color : COLOR0, float2 uv : TEXCOORD0)
{
    float depth = position.z;
    return Oit(Shade(color, uv, position), depth);
}
