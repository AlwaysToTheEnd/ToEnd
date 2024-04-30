cbuffer gSMAAPassCB : register(b0, space0)
{
    float4 gWindowReciprocalnSize;
    float4 gSubsampleIndices;
};

#define SMAA_RT_METRICS gWindowReciprocalnSize
#define SMAA_HLSL_4
#define SMAA_PRESET_MEDIUM
#include "SMAA.hlsl"

Texture2D gColorTex : register(t0);
Texture2D gColorTexGamma : register(t1); 
Texture2D gSearchTex : register(t2);
Texture2D gAreaTex : register(t3);
Texture2D gEdgesTex : register(t4);
Texture2D gBlendTex : register(t5);

struct EdgeGSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 offset[3] : TEXCOORD1;
};

struct BlendGSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 pixuv : TEXCOORD1;
    float4 offset[3] : TEXCOORD2;
};

struct NeiBlendGSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 offset : TEXCOORD2;
};

float4 VS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

[maxvertexcount(4)]
void EdgeGS(point float4 input[1] : SV_Position, inout TriangleStream<EdgeGSOut> output)
{
    EdgeGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.1f, 1.0f);
    vertices[1].position = float4(-1.0f, 0.0f, 0.1f, 1.0f);
    vertices[2].position = float4(0.0f, -1.0f, 0.1f, 1.0f);
    vertices[3].position = float4(0.0f, 0.0f, 0.1f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, 0.0f);
    vertices[2].uv = float2(1.0f, 1.0f);
    vertices[3].uv = float2(1.0f, 0.0f);
    
    SMAAEdgeDetectionVS(vertices[0].uv, vertices[0].offset);
    SMAAEdgeDetectionVS(vertices[1].uv, vertices[1].offset);
    SMAAEdgeDetectionVS(vertices[2].uv, vertices[2].offset);
    SMAAEdgeDetectionVS(vertices[3].uv, vertices[3].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

float2 EdgePS(EdgeGSOut input) : SV_Target0  // edgeTex
{
    return SMAAColorEdgeDetectionPS(input.uv, input.offset, gColorTexGamma);
}

[maxvertexcount(4)]
void BlendGS(point float4 input[1] : SV_Position, inout TriangleStream<BlendGSOut> output)
{
    BlendGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.1f, 1.0f);
    vertices[1].position = float4(-1.0f, 0.0f, 0.1f, 1.0f);
    vertices[2].position = float4(0.0f, -1.0f, 0.1f, 1.0f);
    vertices[3].position = float4(0.0f, 0.0f, 0.1f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, 0.0f);
    vertices[2].uv = float2(1.0f, 1.0f);
    vertices[3].uv = float2(1.0f, 0.0f);
    
    SMAABlendingWeightCalculationVS(vertices[0].uv, vertices[0].pixuv, vertices[0].offset);
    SMAABlendingWeightCalculationVS(vertices[1].uv, vertices[1].pixuv, vertices[1].offset);
    SMAABlendingWeightCalculationVS(vertices[2].uv, vertices[2].pixuv, vertices[2].offset);
    SMAABlendingWeightCalculationVS(vertices[3].uv, vertices[3].pixuv, vertices[3].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

float4 BlendPS(BlendGSOut input) : SV_Target0 //blendTex
{
    return SMAABlendingWeightCalculationPS(input.uv, input.pixuv, input.offset,
    gEdgesTex, gAreaTex, gSearchTex, gSubsampleIndices);
}


[maxvertexcount(4)]
void NeiBlendGS(point float4 input[1] : SV_Position, inout TriangleStream<NeiBlendGSOut> output)
{
    NeiBlendGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.1f, 1.0f);
    vertices[1].position = float4(-1.0f, 0.0f, 0.1f, 1.0f);
    vertices[2].position = float4(0.0f, -1.0f, 0.1f, 1.0f);
    vertices[3].position = float4(0.0f, 0.0f, 0.1f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, 0.0f);
    vertices[2].uv = float2(1.0f, 1.0f);
    vertices[3].uv = float2(1.0f, 0.0f);
    
    SMAANeighborhoodBlendingVS(vertices[0].uv, vertices[0].offset);
    SMAANeighborhoodBlendingVS(vertices[1].uv, vertices[1].offset);
    SMAANeighborhoodBlendingVS(vertices[2].uv, vertices[2].offset);
    SMAANeighborhoodBlendingVS(vertices[3].uv, vertices[3].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

float4 NeiBlendPS(NeiBlendGSOut input) : SV_Target0 
{
    return SMAANeighborhoodBlendingPS(input.uv,input.offset, gColorTex, gBlendTex);
}