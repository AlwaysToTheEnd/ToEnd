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

cbuffer cbsettings : register(b0, space1)
{
    uint gRenderID;
};

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
    float3x3 tangentBasis : TANBASIS;
    float3 uv0 : TEXCOORD0;
    float3 uv1 : TEXCOORD1;
    float3 uv2 : TEXCOORD2;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
   
    vout.posH = float4(vin.PosL, 1.0f);
    float3 normal = gVertexNormals[vin.id];
    float3 tangent = gVertexTangents[vin.id];
    float3 bitangent = gVertexBitans[vin.id];
    
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
        sumNormalL += boenWeight.weight * mul(normal, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumTangent += boenWeight.weight * mul(tangent, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumBitan += boenWeight.weight * mul(bitangent, (float3x3) gBoneData[boenWeight.boneIndex]);
    }
    
    vout.posH = mul(float4(sumPosL, 1.0f), gViewProj);
    vout.tangentBasis = transpose(float3x3(normalize(sumTangent), normalize(sumBitan), normalize(sumNormalL)));
   
    vout.uv0 = gVertexUV0[vin.id];
    vout.uv1 = gVertexUV1[vin.id];
    vout.uv2 = gVertexUV2[vin.id];
    
    return vout;
}

struct PSOut
{
    float4 color : SV_Target0;
    float3 normal : SV_Target1;
    float3 metal_rough_ao : SV_Target2;
    float3 emissionColor : SV_Target3;
    uint renderID : SV_Target4;
};

float3 ExcuteTextureOP(float3 dest, float3 src, uint op)
{
    float3 result = float3(0, 0, 0);
    
    result = dest + src;
    switch (op)
    {
        case TEXOP_MULTIPLY:
            result = dest * src;
            break;
        case TEXOP_ADD:
            result = dest + src;
            break;
        case TEXOP_SUBTRACT:
            result = dest - src;
            break;
        case TEXOP_DIVIDE:
            result = dest / src;
            break;
        case TEXOP_SMOOTHADD:
            result = (dest + src) - (dest * src);
            break;
        case TEXOP_SIGNEDADD:
            result = dest + (src - 0.5);
            break;
    }
    
    return result;
}

PSOut PS(VSOut pin)
{
    PSOut pout;
    pout.color = float4(gDiffuse, gDiffuseAlpha);
    pout.normal = pin.tangentBasis._13_23_33;
    float normalAlpha = 0.0f;
    pout.metal_rough_ao = float3(gMetalness, gRoughness, 0);
    pout.emissionColor = float3(0, 0, 0);
    pout.renderID = gRenderID;
    
    for (uint index = 0; index < gNumTexture; index++)
    {
        float4 currTextureValue = float4(0, 0, 0, 1);
        TextureInfo currTextureInfo = gTextureInfo[index];
        float3 uv = float3(0, 0, 0);
        
        switch (currTextureInfo.uvChannelIndex)
        {
            case 0:
                uv = pin.uv0;
                break;
            case 1:
                uv = pin.uv1;
                break;
            case 2:
                uv = pin.uv2;
                break;
        }
        
        switch (currTextureInfo.mapMode)
        {
            case TEXMAPMO_WRAP:
                currTextureValue = gTextures[index].Sample(gsamAnisotropicWrap, uv.xy);
                break;
            case TEXMAPMO_CLAMP:
                currTextureValue = gTextures[index].Sample(gsamAnisotropicClamp, uv.xy);
                break;
            case TEXMAPMO_MIRROR:
                {
                    float3 fractionalPart = frac(uv * 0.5f);
                    float3 integerPart = floor(uv * 0.5f);
                    float3 isEven = step(0.5f, frac(integerPart));
                    uv = lerp(fractionalPart, 1.0f - fractionalPart, isEven);
                    currTextureValue = gTextures[index].Sample(gsamAnisotropicWrap, uv.xy);
                }
                break;
            case TEXMAPMO_DECAL:
                {
                    if (uv.x > 1.0f || uv.x < 0.0f ||
                        uv.y > 1.0f || uv.y < 0.0f ||
                        uv.z > 1.0f || uv.z < 0.0f)
                    {
                        currTextureInfo.type = TEXTYPE_UNKNOWN + 1;

                    }
                    else
                    {
                        currTextureValue = gTextures[index].Sample(gsamAnisotropicWrap, uv.xy);
                    }
                }
                break;
        }
        
        float blend = currTextureInfo.blend;
        switch (currTextureInfo.type)
        {
            case TEXTYPE_BASE_COLOR:{
                    float3 srcColor = currTextureValue.rgb * blend;
                    pout.color.rgb = ExcuteTextureOP(pout.color.rgb * pout.color.a, srcColor, currTextureInfo.textureOp);
                    pout.color.a = 1.0f;
                }
                break;
            case TEXTYPE_NORMAL:{
                    float3 normalResult = currTextureValue.xyz * 2 - 1;
                    normalResult.y = -normalResult.y;
                    normalResult.xy *= blend;
                    normalResult = mul(pin.tangentBasis, normalize(normalResult));
                    
                    pout.normal = ExcuteTextureOP(pout.normal * normalAlpha, normalResult, currTextureInfo.textureOp);
                    pout.normal = normalize(pout.normal);
                    normalAlpha = 1.0f;
                }
                break;
            case TEXTYPE_EMISSION_COLOR:
                pout.emissionColor = currTextureValue.rgb;
                break;
            case TEXTYPE_METALNESS:
                pout.metal_rough_ao.x = currTextureValue.x;
                break;
            case TEXTYPE_ROUGHNESS:
                pout.metal_rough_ao.y = currTextureValue.x;
                break;
            case TEXTYPE_AO:
                pout.metal_rough_ao.z = currTextureValue.x;
                break;
            case TEXTYPE_UNKNOWN:{
                }
                break;
        }
    }
    
    pout.normal = (pout.normal * 0.5).xyz + float3(0.5f, 0.5f, 0.5f);
    
    return pout;
}