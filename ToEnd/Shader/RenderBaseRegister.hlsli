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

struct VertexIn
{
    float3 PosL : POSITION;
    uint id : SV_VertexID;
};
