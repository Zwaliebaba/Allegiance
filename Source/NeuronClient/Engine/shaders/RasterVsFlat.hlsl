#include "Raster.hlsli"

// D3DSHADE_FLAT: the colour of the first vertex, which is also Direct3D 11's
// provoking vertex.

struct VertexOut
{
    float4                 position : SV_Position;
    nointerpolation float4 color    : COLOR0;
    float2                 uv       : TEXCOORD0;
};

VertexOut main(VertexIn input)
{
    VertexOut output;
    output.position = ScreenToClip(input.position);
    output.color    = VertexColor(input.color);
    output.uv       = input.uv;
    return output;
}
