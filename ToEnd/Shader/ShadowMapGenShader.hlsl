
cbuffer ShadowLightPass : register(b0, sapce0)
{
    float4x4 gLightViewOrtho;
};

struct VertexIn
{
    float3 PosW : POSITION;
};

struct VSOut
{
    float4 posH : SV_Position;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
    vout.posH = mul(float4(vin.PosW, 1.0f), gLightViewOrtho);
   
    return vout;
}
