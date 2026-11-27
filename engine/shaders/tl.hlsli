//////////////////////////////////////////////////////////////////////////////
//
// Transform and lighting for model-space geometry: what device3d.cpp did on
// the CPU before handing screen vertices to the rasterizer.
//
// Positions go through the same full matrix Device3D clips with (model,
// perspective, and the clip rect mapped to [-1, 1]), then through g_post to
// Direct3D clip space for the whole surface, including the half-pixel shift
// to DirectX 6 pixel centres. The rasterizer clips to the frustum; the
// scissor keeps drawing inside the clip rect. Extra clip planes are applied
// in the full matrix's space, where Device3D keeps them.
//
// Define TL_NORMAL for Vertex input (position, uv, normal: lit here), leave
// it undefined for VertexL (position, uv, colour). Define TL_FLAT for flat
// shading (the first vertex's colour, as D3DSHADE_FLAT).
//
//////////////////////////////////////////////////////////////////////////////

cbuffer TLConstants : register(b1)
{
    float4 g_full[4];
    float4 g_post[4];
    float4 g_light;     // xyz: model-space light direction
    float4 g_color;     // bidirectional: light colour
    float4 g_colorAlt;  // bidirectional: colour of the other side
    float4 g_global;    // global colour, or material diffuse
    float4 g_params;    // x: ambient level
    uint4  g_tlmode;    // x: TLMode, y: number of extra clip planes, z: linear light (see raster.hlsli)
    float4 g_planes[2];
};

#define TLModeWhite         0
#define TLModeGlobalColor   1
#define TLModeDirectional   2
#define TLModeBidirectional 3
#define TLModeMaterial      4
#define TLModeVertexColor   5

struct VertexIn
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
#ifdef TL_NORMAL
    float3 normal   : NORMAL;
#else
    float4 color    : COLOR0;
#endif
};

struct VertexOut
{
    float4 position : SV_Position;
#ifdef TL_FLAT
    nointerpolation
#endif
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
    float2 clip     : SV_ClipDistance0;
};

//
// Authored colours (the global colour, material diffuse, light colours and
// vertex colours) are gamma-encoded; in linear light they are decoded. Light
// intensities (the directional term and ambient) are linear already.
//

float3 AuthoredColor(float3 c)
{
    return g_tlmode.z != 0 ? pow(abs(c), 2.2) : c;
}

//
// One exit point: FXC warns (X4000) about early returns from branches on
// constant-buffer values
//

float4 Light(VertexIn input)
{
#ifdef TL_NORMAL
    float  bright  = dot(input.normal, g_light.xyz);
    float  ambient = g_params.x;
    float  b       = saturate(bright + ambient);
    float4 result  = float4(1, 1, 1, 1);

    if (g_tlmode.x == TLModeGlobalColor) {
        result = float4(AuthoredColor(g_global.rgb), g_global.a);
    } else if (g_tlmode.x == TLModeDirectional) {
        result = float4(b, b, b, 1);
    } else if (g_tlmode.x == TLModeBidirectional) {
        float3 rgb =
            bright >= 0
                ? saturate( bright * AuthoredColor(g_color.rgb)    + ambient)
                : saturate(-bright * AuthoredColor(g_colorAlt.rgb) + ambient);
        result = float4(rgb, 1);
    } else if (g_tlmode.x == TLModeMaterial) {
        result = float4(AuthoredColor(g_global.rgb) * b, g_global.a);
    }

    return result;
#else
    return float4(AuthoredColor(input.color.rgb), input.color.a);
#endif
}

VertexOut main(VertexIn input)
{
    float4 p  = float4(input.position, 1);
    float4 pf = float4(dot(g_full[0], p), dot(g_full[1], p), dot(g_full[2], p), dot(g_full[3], p));

    VertexOut output;
    output.position = float4(dot(g_post[0], pf), dot(g_post[1], pf), dot(g_post[2], pf), dot(g_post[3], pf));
    output.color    = Light(input);
    output.uv       = input.uv;
    output.clip.x   = g_tlmode.y > 0 ? dot(g_planes[0], pf) : 1;
    output.clip.y   = g_tlmode.y > 1 ? dot(g_planes[1], pf) : 1;
    return output;
}
