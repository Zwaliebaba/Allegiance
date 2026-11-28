#include "tl_pixel.hlsli"

// The translucent pass (raster.hlsli, Oit)

OitOut main(VertexOut input)
{
    return Oit(TLShade(input), input.position);
}
