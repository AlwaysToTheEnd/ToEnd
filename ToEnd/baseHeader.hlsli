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
};

cbuffer TextureGuide : register(b2)
{
    uint gNumActiveUVChanels;
    uint gNumUVComponent[MAX_NUMBER_OF_TEXTURECOORDS];
};

StructuredBuffer<float3> gVertexNormals : register(t0, space1);
StructuredBuffer<float3> gVertexTangents : register(t1, space1);
StructuredBuffer<float3> gVertexBitans : register(t2, space1);
StructuredBuffer<float4x4> gBoneData : register(t3, space1);
StructuredBuffer<uint> gBoneIndex : register(t4, space1);
StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t5, space1);
StructuredBuffer<BoneWeight> gBoneWeights : register(t6, space1);
StructuredBuffer<float3> gVertexUVs[MAX_NUMBER_OF_TEXTURECOORDS] : register(t7, space1);

Texture2D gMainTexture[MAXTEXTURE] : register(t0, space2);

struct VSOut
{
    float4 PosH : SV_POSITION;
    float3 Normal : TEXCOORD1;
    nointerpolation int matIndex : MATNDEX;
    float3 uv1[MAX_NUMBER_OF_TEXTURECOORDS] : TEXCOORDS;
};

struct PSOut
{
    float4 color : SV_Target0;
    float3 normal : SV_Target1;
    float specPow : SV_Target2;
    int objectID : SV_Target3;
};
