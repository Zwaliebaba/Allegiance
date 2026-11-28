#ifndef TL_PIXEL_HLSLI
#define TL_PIXEL_HLSLI

#define TL_NORMAL
#define TL_PIXEL
#include "Tl.hlsli"
#define RASTER_PIXEL
#include "Raster.hlsli"

//////////////////////////////////////////////////////////////////////////////
//
// Per-pixel lighting for model-space Vertex geometry (TlPs.hlsl, and the
// translucent pass's TlPsOit.hlsl): the lighting formulas
// of Tl.hlsli for the interpolated normal, then the texture and render
// state of Raster.hlsli. In a 3D layer's linear-light scene, lit opaque
// surfaces also get a Blinn-Phong highlight in the light's colour, faded
// out towards the terminator.
//
//////////////////////////////////////////////////////////////////////////////

static const float c_specularPower     = 24.0;
static const float c_specularIntensity = 0.35;

float4 TLShade(VertexOut input)
{
    float3 n = normalize(input.normal);

    float4 result = Shade(Light(n), input.uv, input.position);

    bool bLit =
           g_tlmode.x == TLModeDirectional
        || g_tlmode.x == TLModeBidirectional
        || g_tlmode.x == TLModeMaterial;

    if (bLit && g_tlmode.z != 0 && (g_flags.x & 16) == 0) {
        float3 l      = normalize(g_light.xyz);
        float3 v      = normalize(input.eye);
        float3 h      = normalize(l + v);
        float  bright = dot(n, l);

        float3 colorLight =
            g_tlmode.x == TLModeBidirectional
                ? AuthoredColor(g_color.rgb)
                : float3(1, 1, 1);

        float spec =
              pow(saturate(dot(n, h)), c_specularPower)
            * c_specularIntensity
            * saturate(bright * 4.0);

        result.rgb += spec * colorLight;   // opaque: alpha is coverage here, not opacity
    }

    return result;
}

#endif
