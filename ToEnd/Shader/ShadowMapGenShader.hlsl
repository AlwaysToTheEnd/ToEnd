
cbuffer ObejctCBPass : register(b0)
{
    float4x4 gObjectMat;
};

cbuffer ShadowLightPass : register(b1)
{
    float4x4 gLightViewOrtho;
};

StructuredBuffer<float3> gPos : register(t0);

struct VSOut
{
    float4 posH : SV_Position;
};

VSOut VS(uint id : SV_VertexID)
{
    VSOut vout;
    
    vout.posH = float4(gPos[id], 1.0f);
    
    vout.posH = mul(vout.posH, gObjectMat);
    vout.posH = mul(vout.posH, gLightViewOrtho);
    return vout;
}
