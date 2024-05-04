#include "RenderBaseRegister.hlsli"
#include "MaterialRegister.hlsli"

struct VertexIn
{
    float3 posL : POSITION;
    float3 normal : NORMAL;
    float3 tan : TANGENT;
    float3 bitan : BITAN;
    float3 uv0 : UV0;
    float3 uv1 : UV1;
    float3 uv2 : UV2;
    uint2 weightInfo : BONEWEIGHTINFO; //x =numWeight , y = offsetIndex
};

struct BoneWeight
{
    uint boneIndex;
    float weight;
};

cbuffer cbsettings : register(b0, space1)
{
    uint gRenderID;
    bool gIsShadowGen;
    float gPhongTessAlpha;
};

StructuredBuffer<BoneWeight> gBoneWeights : register(t0, space1);
StructuredBuffer<float4x4> gBoneData : register(t1, space1);

RWStructuredBuffer<float3> gResultVertices : register(u0, space1);

struct VSOut
{
    float3 pos : POSITION0;
    float3 normal : NORMAL0;
    float3 tangent : TANGENT0;
    float3 biTan : BITANGENT0;
    float3 uv0 : TEXCOORD0;
    float3 uv1 : TEXCOORD1;
    float3 uv2 : TEXCOORD2;
};

VSOut VS(VertexIn vin, uint id : SV_VertexID)
{
    VSOut vout;
   
    float4 sumPos = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float3 sumNormal = float3(0.0f, 0.0f, 0.0f);
    float3 sumTangent = float3(0.0f, 0.0f, 0.0f);
    float3 sumBitan = float3(0.0f, 0.0f, 0.0f);
    
    BoneWeight boenWeight;
    [loop]
    for (uint i = 0; i < vin.weightInfo.x; i++)
    {
        boenWeight = gBoneWeights[vin.weightInfo.y + i];
        
        sumPos += boenWeight.weight * mul(float4(vin.posL, 1.0f), gBoneData[boenWeight.boneIndex]);
        sumNormal += boenWeight.weight * mul(vin.normal, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumTangent += boenWeight.weight * mul(vin.tan, (float3x3) gBoneData[boenWeight.boneIndex]);
        sumBitan += boenWeight.weight * mul(vin.bitan, (float3x3) gBoneData[boenWeight.boneIndex]);
    }
    
    vout.pos = sumPos.xyz;
    vout.tangent = normalize(sumTangent);
    vout.normal = normalize(sumNormal);
    vout.biTan = normalize(sumBitan);
   
    vout.uv0 = vin.uv0;
    vout.uv1 = vin.uv1;
    vout.uv2 = vin.uv2;
    
    if (gIsShadowGen)
    {
        gResultVertices[id] = sumPos.xyz;
    }
    
    return vout;
}

struct HSConstDataOut
{
    float edges[3] : SV_TessFactor;
    float inside[1] : SV_InsideTessFactor;
};

HSConstDataOut PointLightConstantHS(InputPatch<VSOut, 3> patch)
{
    HSConstDataOut output;
    //float3 pToEyeVec[3];
    //pToEyeVec[0] = normalize((patch[0].pos + patch[1].pos) *0.5f - gEyePosW);
    //pToEyeVec[1] = normalize((patch[1].pos + patch[2].pos) * 0.5f - gEyePosW);
    //pToEyeVec[2] = normalize((patch[2].pos + patch[0].pos) * 0.5f - gEyePosW);
    
    //float3 normals[3];
    //normals[0] = normalize(patch[0].normal + patch[1].normal);
    //normals[1] = normalize(patch[1].normal + patch[2].normal);
    //normals[2] = normalize(patch[2].normal + patch[0].normal);
    
    //output.edges[0] = 1.0f + (1.0f - abs(dot(pToEyeVec[0], normals[0]))) * 10.0f;
    //output.edges[1] = 1.0f + (1.0f - abs(dot(pToEyeVec[1], normals[1]))) * 10.0f;
    //output.edges[2] = 1.0f + (1.0f - abs(dot(pToEyeVec[2], normals[2]))) * 10.0f;
    
    //output.inside[0] = 1.0f + (output.edges[0] + output.edges[1] + output.edges[2]) * 0.33333333f;
    float tessFactor = 4.0f;
    output.edges[0] = output.edges[1] = output.edges[2] = tessFactor;
    output.inside[0] = tessFactor;
    
    return output;
}


[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PointLightConstantHS")]
[maxtessfactor(10.0f)]
VSOut HS(InputPatch<VSOut, 3> patch, uint patchID : SV_PrimitiveID, uint cpId : SV_OutputControlPointID)
{
    VSOut result;
    result = patch[cpId];
    
    return result;
}

struct DSOut
{
    float4 posH : SV_position;
    float3x3 tangentBasis : TANBASIS;
    float3 uv0 : TEXCOORD0;
    float3 uv1 : TEXCOORD1;
    float3 uv2 : TEXCOORD2;
};

float3 Project(float3 p, float3 c, float3 n)
{
    return p - dot(p - c, n) * n;
}

[domain("tri")]
DSOut DS(HSConstDataOut input, float3 uvw : SV_DomainLocation, const OutputPatch<VSOut, 3> tri)
{
    DSOut result;
    
    float3 clampPos = uvw.x * tri[0].pos + uvw.y * tri[1].pos + uvw.z * tri[2].pos;
    float3 c0 = Project(clampPos, tri[0].pos, tri[0].normal);
    float3 c1 = Project(clampPos, tri[1].pos, tri[1].normal);
    float3 c2 = Project(clampPos, tri[2].pos, tri[2].normal);
    float3 phong = uvw.x * c0 + uvw.y * c1 + uvw.z * c2;
    float3 r = lerp(clampPos, phong, gPhongTessAlpha);
    
    float3 normal = uvw.x * tri[0].normal + uvw.y * tri[1].normal + uvw.z * tri[2].normal;
    float3 tangent = uvw.x * tri[0].tangent + uvw.y * tri[1].tangent + uvw.z * tri[2].tangent;
    float3 bitan = uvw.x * tri[0].biTan + uvw.y * tri[1].biTan + uvw.z * tri[2].biTan;
    
    result.posH = mul(float4(r, 1.0f), gViewProj);
    result.tangentBasis = transpose(float3x3(tangent, bitan, normal));
    result.uv0 = uvw.x * tri[0].uv0 + uvw.y * tri[1].uv0 + uvw.z * tri[2].uv0;
    result.uv1 = uvw.x * tri[0].uv1 + uvw.y * tri[1].uv1 + uvw.z * tri[2].uv1;
    result.uv2 = uvw.x * tri[0].uv2 + uvw.y * tri[1].uv2 + uvw.z * tri[2].uv2;
    
    return result;
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

float3 ExcuteSrcAlphaOP(float3 color, float alpha, uint op)
{
    float3 result = color.rgb;
   
    switch (op)
    {
        case ALPHAOP_MULTIPLY:
            result *= alpha;
            break;
        case ALPHAOP_INV_MULTIPLY:
            result *= (1.0f - alpha);
            break;
    }
    
    return result;
}

PSOut PS(DSOut pin)
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
                    pout.color.rgb = ExcuteTextureOP(pout.color.rgb * pout.color.a, ExcuteSrcAlphaOP(srcColor, currTextureValue.a, currTextureInfo.srcAlphaOP), currTextureInfo.textureOp);
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