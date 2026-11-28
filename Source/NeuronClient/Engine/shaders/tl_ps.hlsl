#include "tl_pixel.hlsli"

float4 main(VertexOut input) : SV_Target
{
    return TLShade(input);
}
