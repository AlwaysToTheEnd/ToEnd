#include "DeferredRenderBase.hlsli"

float4 VS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

struct HSConstDataOut
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

HSConstDataOut PointLightConstantHS(uint patchID : SV_PrimitiveID)
{
    HSConstDataOut output;
    
    float tessFactor = 18.0;
    output.edges[0] = output.edges[1] = output.edges[2] = output.edges[3] = tessFactor;
    output.inside[0] = output.inside[1] = tessFactor;
    
    return output;
}

struct HSOut
{
    float3 hemiDir : POSITION;
};

static const float3 s_hemiDir[2] =
{
    float3(1.0f, 1.0f, 1.0f),
    float3(-1.0f, 1.0f, -1.0f)
};

[domain("quad")]
[partitioning("interger")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PointLightConstantHS")]
HSOut HS(uint patchID : SV_PrimitiveID)
{
    HSOut result;
    result.hemiDir = s_hemiDir[patchID % 2];
    
    return result;
}

struct DSOut
{
    float4 position : SV_position;
    float2 cpPos : TEXCOORD0;
};


[domain("quad")]
DSOut DS(HSConstDataOut input, float2 uv : SV_DomainLocation, const OutputPatch<HSOut,4> quad)
{
    DSOut result;
    
    return result;
}