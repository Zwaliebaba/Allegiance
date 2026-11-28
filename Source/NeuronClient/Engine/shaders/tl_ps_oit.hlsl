#define OIT_PASS
#include "tl_pixel.hlsli"

// The translucent pass (raster.hlsli, Oit)

OitOut main(VertexOut input)
{
    float depth = input.position.z;
    return Oit(TLShade(input), depth);
}
