struct TextureInfo
{
    uint type;
    uint mapping;
    uint unChannelIndex;
    float blend;
    uint textureOp;
    uint3 mapMode;
};

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

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0, space0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gRightViewProj;
    float4x4 gOrthoMatrix;
    uint2 gRenderTargetSize;
    uint2 gInvRenderTargetSize;
    float4 gAmbientLight;
    float3 gEyePosW;
    float2 gMousePos;
    float3 gPad0;
};

cbuffer ObjectInfo : register(b0, space1)
{
    float4x4 gWorldMat;
    uint gObejctID;
};

cbuffer MaterialData : register(b1, space1)
{
    int shadingModel;
    int twosided;
    int wireframe;
    int blend;
    float3 diffuse;
    float opacity;
    float3 ambient;
    float bumpscaling;
    float3 specular;
    float shininess;
    float3 emissive;
    float refracti;
    float3 transparent;
    float shinpercent;
    float3 reflective;
    float reflectivity;
    uint gNumTexture;
    float3 pad0;
};

StructuredBuffer<TextureInfo> gTextureInfos : register(t0, space1);
Texture2D gTextures[10] : register(t1, space1);

StructuredBuffer<float3> gVertexNormals : register(t0, space2);
StructuredBuffer<float3> gVertexTangents : register(t1, space2);
StructuredBuffer<float3> gVertexBitans : register(t2, space2);
StructuredBuffer<float3> gVertexUV : register(t3, space2);

StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t4, space2);
StructuredBuffer<BoneWeight> gBoneWeights : register(t5, space2);
StructuredBuffer<float4x4> gBoneData : register(t6, space2);

struct VertexIn
{
    float3 PosL : POSITION;
    uint id : SV_VertexID;
};

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
    
    vout.PosH = mul(float4(sumPosL, 1.0f), gWorldMat);
    vout.PosH = mul(vout.PosH, gViewProj);
    vout.Normal = mul(sumNormalL, (float3x3) gWorldMat);
    vout.tangent = mul(sumTangent, (float3x3) gWorldMat);
    vout.bitangent = mul(sumBitan, (float3x3) gWorldMat);
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