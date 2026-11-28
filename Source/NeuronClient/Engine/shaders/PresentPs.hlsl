cbuffer PresentConstants : register(b0)
{
    float4 g_source;
    float4 g_gamma;     // x: 1 / gamma
    float4 g_scale;     // xy: frame size in pixels, z: whole scale factor (at least 1)
};

Texture2D    g_frame   : register(t0);
SamplerState g_sampler : register(s0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    //
    // Sharp bilinear: each source pixel becomes a solid block of g_scale.z
    // screen pixels, and the bilinear filter only blends the one-pixel seams
    // that the fractional rest of the scale leaves. At 1:1 this samples
    // texel centres, an exact copy.
    //

    float2 texel  = uv * g_scale.xy;
    float2 base   = floor(texel);
    float2 offset = frac(texel) - 0.5;
    float  range  = 0.5 - 0.5 / g_scale.z;
    float2 f      = (offset - clamp(offset, -range, range)) * g_scale.z + 0.5;

    float3 c = g_frame.Sample(g_sampler, (base + f) / g_scale.xy).rgb;

    //
    // Same curve as the DirectDraw gamma ramp it replaces: value^(1/gamma).
    //

    return float4(pow(abs(c), g_gamma.x), 1.0);
}
