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

cbuffer cbPass : register(b0)
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

cbuffer MaterialData : register(b1)
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

cbuffer ObejctInfo : register(b2)
{
    float4x4 gWorldMat;
    uint4x2 gNumUVComponent;
    uint gNumUVChannel;
    uint gHasTanBitans;
    uint gObejctID;
};

StructuredBuffer<float4x4> gBoneData : register(t0, space0);
StructuredBuffer<float3> gVertexNormals : register(t1, space0);
StructuredBuffer<float3> gVertexTangents : register(t2, space0);
StructuredBuffer<float3> gVertexBitans : register(t3, space0);
StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t4, space0);
StructuredBuffer<BoneWeight> gBoneWeights : register(t5, space0);
StructuredBuffer<float3> gVertexUV[3] : register(t6, space0);

StructuredBuffer<TextureInfo> gTextureInfos : register(t0, space1);
Texture2D gTextures[10] : register(t1, space1);

struct VertexIn
{
    float3 PosL : POSITION;
    uint id : SV_VertexID;
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float3 Normal : NORMAL0;
    float3 uv0 : TEXCOORD0;
    float3 uv1 : TEXCOORD1;
    float3 uv2 : TEXCOORD2;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
   
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.Normal = gVertexNormals[vin.id];
    
    BoneWeightInfo weightInfo = gBoneWeightInfos[vin.id];
   
    float3 sumPosL = float3(0.0f, 0.0f, 0.0f);
    float3 sumNormalL = float3(0.0f, 0.0f, 0.0f);
    
    BoneWeight boenWeight;
    [loop]
    for (uint i = 0; i < weightInfo.numWeight; i++)
    {
        boenWeight = gBoneWeights[weightInfo.offsetIndex + i];
        
        sumPosL += boenWeight.weight * mul(vout.PosH, gBoneData[boenWeight.boneIndex]).xyz;
        sumNormalL += boenWeight.weight * mul(vout.Normal, (float3x3) gBoneData[boenWeight.boneIndex]);
    }
    
    vout.PosH = float4(sumPosL, 1.0f);
    vout.PosH = mul(vout.PosH, gWorldMat);
    vout.PosH = mul(vout.PosH, gViewProj);
    vout.Normal = mul(vout.Normal, (float3x3) gWorldMat);
    
    float3 uvResults[3] = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };
    [loop]
    for (uint uvIndex = 0; uvIndex < gNumUVChannel; uvIndex++)
    {
        uvResults[uvIndex] = gVertexUV[uvIndex][vin.id];
    }
    
    vout.uv0 = uvResults[0];
    vout.uv1 = uvResults[1];
    vout.uv2 = uvResults[2];
    
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
    
    if (pin.uv1.r < 0.9f && pin.uv1.r > 0.1f && pin.uv1.g < 0.9f && pin.uv1.g > 0.1f)
    {
        float4 baseColor = gTextures[1].Sample(gsamPointWrap, pin.uv1.rg);
    }
    
	
    return pout;
}