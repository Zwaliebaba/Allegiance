//////////////////////////////////////////////////////////////////////////////
//
// Presentation: one triangle covering the viewport (which the presenter sets
// to the letterboxed rectangle), sampling the source rectangle of the frame.
//
//////////////////////////////////////////////////////////////////////////////

cbuffer PresentConstants : register(b0)
{
    float4 g_source;    // u0, v0, u1, v1
    float4 g_gamma;     // x: 1 / gamma
    float4 g_scale;     // xy: frame size in pixels, z: whole scale factor (at least 1)
};

struct VertexOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VertexOut main(uint id : SV_VertexID)
{
    float2 t = float2((id << 1) & 2, id & 2);     // (0,0) (2,0) (0,2)

    VertexOut output;
    output.position = float4(t.x * 2.0 - 1.0, 1.0 - t.y * 2.0, 0.0, 1.0);
    output.uv       = lerp(g_source.xy, g_source.zw, t);
    return output;
}
