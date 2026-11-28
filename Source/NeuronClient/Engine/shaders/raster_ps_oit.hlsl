#include "raster.hlsli"

// The translucent pass (raster.hlsli, Oit)

OitOut main(float4 position : SV_Position, float4 color : COLOR0, float2 uv : TEXCOORD0)
{
    return Oit(Shade(color, uv, position), position);
}
