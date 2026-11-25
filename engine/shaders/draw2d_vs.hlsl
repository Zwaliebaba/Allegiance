#include "draw2d.hlsli"

VertexOut main(VertexIn input)
{
    VertexOut output;
    output.position = float4(input.position.x * g_target.x - 1.0, 1.0 - input.position.y * g_target.y, 0.0, 1.0);
    output.uv       = input.uv;
    output.color    = input.color;
    return output;
}
