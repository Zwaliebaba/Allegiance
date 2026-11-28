//////////////////////////////////////////////////////////////////////////////
//
// Scene composite: one triangle covering the target; the scissor limits it
// to the 3D layer's clip rect.
//
//////////////////////////////////////////////////////////////////////////////

float4 main(uint id : SV_VertexID) : SV_Position
{
    float2 t = float2((id << 1) & 2, id & 2);     // (0,0) (2,0) (0,2)

    return float4(t.x * 2.0 - 1.0, 1.0 - t.y * 2.0, 0.0, 1.0);
}
