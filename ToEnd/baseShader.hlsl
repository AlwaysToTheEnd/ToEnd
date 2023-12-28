struct TextureView
{
    uint type;
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

struct UVInfo
{
    int channelIndex[8];
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
    uint gObejctID;
};

StructuredBuffer<float4x4> gBoneData : register(t0, space0);
StructuredBuffer<float3> gVertexNormals : register(t1, space0);
StructuredBuffer<float3> gVertexTangents : register(t2, space0);
StructuredBuffer<float3> gVertexBitans : register(t3, space0);
StructuredBuffer<uint> gBoneIndex : register(t4, space0);
StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t5, space0);
StructuredBuffer<BoneWeight> gBoneWeights : register(t6, space0);
StructuredBuffer<UVInfo> gUVInfos : register(t7, space0);
StructuredBuffer<float3> gVertexUV0 : register(t8, space0);
StructuredBuffer<float3> gVertexUV1 : register(t9, space0);
StructuredBuffer<float3> gVertexUV2 : register(t10, space0);

//StructuredBuffer<TextureView> gTextureViews : register(t0, space1);
//Texture2D gMainTexture[10] : register(t1, space1);

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
    nointerpolation int activeUV0 : ACTIVEUV0;
    float3 uv1 : TEXCOORD1;
    nointerpolation int activeUV1 : ACTIVEUV1;
    float3 uv2 : TEXCOORD2;
    nointerpolation int activeUV2 : ACTIVEUV2;
};

VSOut VS(VertexIn vin)
{
    VSOut vout;
    
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.Normal = gVertexNormals[vin.id];
    vout.activeUV0 = 0;
    vout.activeUV1 = 0;
    vout.activeUV2 = 0;
    vout.uv0 = float3(10.0f, -10.0f, 0);
    vout.uv1 = float3(10.0f, -10.0f, 0);
    vout.uv2 = float3(10.0f, -10.0f, 0);
    
    BoneWeightInfo weightInfo = gBoneWeightInfos[vin.id];
   
    
    float3 sumPosL = float3(0.0f, 0.0f, 0.0f);
    float3 sumNormalL = float3(0.0f, 0.0f, 0.0f);
    
    BoneWeight boenWeight;
    [loop]
    for (uint i = 0; i < weightInfo.numWeight; i++)
    {
        boenWeight = gBoneWeights[weightInfo.offsetIndex + i];
        
        sumPosL += boenWeight.weight * mul(vout.PosH, gBoneData[gBoneIndex[boenWeight.boneIndex]]).xyz;
        sumNormalL += boenWeight.weight * mul(vout.Normal, (float3x3) gBoneData[gBoneIndex[boenWeight.boneIndex]]);
    }
    
    vout.PosH = mul(float4(sumPosL, 1.0f), gWorldMat);
    vout.PosH = mul(vout.PosH, gViewProj);
    vout.Normal = mul(sumNormalL, (float3x3) gWorldMat);
    
    //UVInfo uvInfo = gUVInfos[vin.id];
    
    //if (uvInfo.channelIndex[0] > -1)
    //{
    //    vout.uv0 = gVertexUV0[uvInfo.channelIndex[0]];
    //    vout.activeUV0 = 1;
    //}
    
    //if (uvInfo.channelIndex[1] > -1)
    //{
    //    vout.uv1 = gVertexUV1[uvInfo.channelIndex[1]];
    //    vout.activeUV1 = 1;
    //}
    
    //if (uvInfo.channelIndex[2] > -1)
    //{
    //    vout.uv2 = gVertexUV2[uvInfo.channelIndex[2]];
    //    vout.activeUV2 = 1;
    //}
    
    return vout;
}


struct PSOut
{
    float4 color : SV_Target0;
};

PSOut PS(VSOut pin)
{
    PSOut pout;
    pout.color = float4(pin.Normal, 1.0f);
	
    return pout;
}