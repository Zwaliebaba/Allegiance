#include "Raster.hlsli"

struct VertexOut
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

VertexOut main(VertexIn input)
{
    VertexOut output;
    output.position = ScreenToClip(input.position);
    output.color    = VertexColor(input.color);
    output.uv       = input.uv;
    return output;
}
