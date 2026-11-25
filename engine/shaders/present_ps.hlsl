cbuffer PresentConstants : register(b0)
{
    float4 g_source;
    float4 g_gamma;     // x: 1 / gamma
};

Texture2D    g_frame   : register(t0);
SamplerState g_sampler : register(s0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float3 c = g_frame.Sample(g_sampler, uv).rgb;

    //
    // Same curve as the DirectDraw gamma ramp it replaces: value^(1/gamma).
    //

    return float4(pow(abs(c), g_gamma.x), 1.0);
}
