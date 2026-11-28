#define OIT_PASS
#include "TlPixel.hlsli"

// The translucent pass (Raster.hlsli, Oit)

OitOut main(VertexOut input)
{
    float depth = input.position.z;
    return Oit(TLShade(input), depth);
}
