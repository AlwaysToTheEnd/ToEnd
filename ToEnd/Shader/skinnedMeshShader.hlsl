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

uint gRenderID : register(b0, space1);

StructuredBuffer<float3> gVertexNormals : register(t0, space1);
StructuredBuffer<float3> gVertexTangents : register(t1, space1);
StructuredBuffer<float3> gVertexBitans : register(t2, space1);
StructuredBuffer<float3> gVertexUV0 : register(t3, space1);
StructuredBuffer<float3> gVertexUV1 : register(t4, space1);
StructuredBuffer<float3> gVertexUV2 : register(t5, space1);

StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t0, space2);
StructuredBuffer<BoneWeight> gBoneWeights : register(t1, space2);
StructuredBuffer<float4x4> gBoneData : register(t2, space2);

struct VSOut
{
    float4 posH : SV_POSITION;
    float3 normal : NORMAL0;
    float3 tangent : NORMAL1;
    float3 bitangent : NORMAL2;
    float3 uv0 : TEXCOORD0;
    float3 uv1 : TEXCOORD1;
    float3 uv2 : TEXCOORD2;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
   
    vout.posH = float4(vin.PosL, 1.0f);
    vout.normal = gVertexNormals[vin.id];
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
        
        sumPosL += boenWeight.weight * mul(vout.posH, gBoneData[boenWeight.boneIndex]).xyz;
        sumNormalL += boenWeight.weight * mul(vout.normal, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumTangent += boenWeight.weight * mul(vout.tangent, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumBitan += boenWeight.weight * mul(vout.bitangent, (float3x3) gBoneData[boenWeight.boneIndex]);
    }
    
    vout.posH = mul(float4(sumPosL, 1.0f), gViewProj);
    vout.normal = sumNormalL;
    vout.tangent = sumTangent;
    vout.bitangent = sumBitan;
    vout.uv0 = gVertexUV0[vin.id];
    vout.uv1 = gVertexUV1[vin.id];
    vout.uv2 = gVertexUV2[vin.id];
    
    return vout;
}


struct PSOut
{
    float4 color : SV_Target0;
    float3 normal : SV_Target1;
    float3 metal_rough_emis : SV_Target2;
    float3 ao_fres_anis : SV_Target3;
    uint renderID : SV_Target4;
};

PSOut PS(VSOut pin)
{
    PSOut pout;
   
    pout.color = gTextures[0].Sample(gsamPointWrap, pin.uv0.rg);
    
    if (gNumTexture > 1)
    {
        float3 tangentNormal = gTextures[1].Sample(gsamPointWrap, pin.uv0.rg).rgb;
        tangentNormal = normalize(tangentNormal * 2 - 1);
        float3x3 TBN = float3x3(normalize(pin.tangent), normalize(pin.bitangent), normalize(pin.normal));
        TBN = transpose(TBN);
        float3 worldnormal = mul(TBN, tangentNormal);
        
        pout.normal = (worldnormal * 0.5).xyz + float3(0.5f, 0.5f, 0.5f);
    }
    else
    {
        pout.normal = (pin.normal * 0.5).xyz + float3(0.5f, 0.5f, 0.5f);
    }
    
    pout.renderID = gRenderID;
    pout.metal_rough_emis = float3(0, 0, 0);
    pout.ao_fres_anis = float3(0, 0, 0);
    
    return pout;
}