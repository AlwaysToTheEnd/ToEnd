#include "RenderBaseRegister.hlsli"
#include "MaterialRegister.hlsli"

struct BoneWeightInfo
{
    uint numWeight;
    uint offsetIndex;
};

struct BoneWeight
{
    uint boneIndex;
    float weight;
};

uint gObejctID : register(b0, space1);

StructuredBuffer<float3> gVertexNormals : register(t0, space1);
StructuredBuffer<float3> gVertexTangents : register(t1, space1);
StructuredBuffer<float3> gVertexBitans : register(t2, space1);
StructuredBuffer<float3> gVertexUV : register(t3, space1);

StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t0, space2);
StructuredBuffer<BoneWeight> gBoneWeights : register(t1, space2);
StructuredBuffer<float4x4> gBoneData : register(t2, space2);

struct VSOut
{
    float4 PosH : SV_POSITION;
    float3 Normal : NORMAL0;
    float3 tangent : NORMAL1;
    float3 bitangent : NORMAL2;
    float3 uv0 : TEXCOORD0;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
   
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.Normal = gVertexNormals[vin.id];
    vout.tangent = gVertexTangents[vin.id];
    vout.bitangent = gVertexBitans[vin.id];
    
    BoneWeightInfo weightInfo = gBoneWeightInfos[vin.id];
   
    float3 sumPosL = float3(0.0f, 0.0f, 0.0f);
    float3 sumNormalL = float3(0.0f, 0.0f, 0.0f);
    float3 sumTangent = float3(0.0f, 0.0f, 0.0f);
    float3 sumBitan = float3(0.0f, 0.0f, 0.0f);
    
    BoneWeight boenWeight;
    [loop]
    for (uint i = 0; i < weightInfo.numWeight; i++)
    {
        boenWeight = gBoneWeights[weightInfo.offsetIndex + i];
        
        sumPosL += boenWeight.weight * mul(vout.PosH, gBoneData[boenWeight.boneIndex]).xyz;
        sumNormalL += boenWeight.weight * mul(vout.Normal, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumTangent += boenWeight.weight * mul(vout.tangent, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumBitan += boenWeight.weight * mul(vout.bitangent, (float3x3) gBoneData[boenWeight.boneIndex]);
    }
    
    vout.PosH = mul(float4(sumPosL, 1.0f), gViewProj);
    vout.uv0 = gVertexUV[vin.id];
    
    return vout;
}


struct PSOut
{
    float4 color : SV_Target0;
};

PSOut PS(VSOut pin)
{
    PSOut pout;
    
    pout.color = gTextures[0].Sample(gsamPointWrap, pin.uv0.rg);
    
    return pout;
}