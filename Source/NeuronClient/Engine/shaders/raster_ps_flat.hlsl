#include "raster.hlsli"

float4 main(float4 position : SV_Position, nointerpolation float4 color : COLOR0, float2 uv : TEXCOORD0) : SV_Target
{
    return Shade(color, uv);
}
