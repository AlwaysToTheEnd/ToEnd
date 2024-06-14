SamplerState PointSampler : register(s4);
SamplerState LinearSampler : register(s4);

cbuffer gSMAAPassCB : register(b0, space0)
{
    float4 gWindowReciprocalnSize;
    float4 gSubsampleIndices;
    float gThreshold;
    int gMaxSearchSteps;
    int gMaxSearchStepsDiag;
    int gCornerRounding;
};

#define SMAA_RT_METRICS gWindowReciprocalnSize
#define SMAA_HLSL_4_1
#define SMAA_THRESHOLD gThreshold
#define SMAA_MAX_SEARCH_STEPS gMaxSearchSteps
#define SMAA_MAX_SEARCH_STEPS_DIAG gMaxSearchStepsDiag
#define SMAA_CORNER_ROUNDING gCornerRounding
#include "SMAA.hlsl"

//#if defined(SMAA_PRESET_LOW)
//#define SMAA_THRESHOLD 0.15
//#define SMAA_MAX_SEARCH_STEPS 4
//#define SMAA_DISABLE_DIAG_DETECTION
//#define SMAA_DISABLE_CORNER_DETECTION
//#elif defined(SMAA_PRESET_MEDIUM)
//#define SMAA_THRESHOLD 0.1
//#define SMAA_MAX_SEARCH_STEPS 8
//#define SMAA_DISABLE_DIAG_DETECTION
//#define SMAA_DISABLE_CORNER_DETECTION
//#elif defined(SMAA_PRESET_HIGH)
//#define SMAA_THRESHOLD 0.1
//#define SMAA_MAX_SEARCH_STEPS 16
//#define SMAA_MAX_SEARCH_STEPS_DIAG 8
//#define SMAA_CORNER_ROUNDING 25
//#elif defined(SMAA_PRESET_ULTRA)
//#define SMAA_THRESHOLD 0.05
//#define SMAA_MAX_SEARCH_STEPS 32
//#define SMAA_MAX_SEARCH_STEPS_DIAG 16
//#define SMAA_CORNER_ROUNDING 25
//#endif

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

[maxvertexcount(3)]
void EdgeGS(point float4 input[1] : SV_Position, inout TriangleStream<EdgeGSOut> output)
{
    EdgeGSOut vertices[3];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 1.0f, 1.0f);
    vertices[1].position = float4(-1.0f, 3.0f, 1.0f, 1.0f);
    vertices[2].position = float4(3.0f, -1.0f, 1.0f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, -1.0f);
    vertices[2].uv = float2(2.0f, 1.0f);
    
    SMAAEdgeDetectionVS(vertices[0].uv, vertices[0].offset);
    SMAAEdgeDetectionVS(vertices[1].uv, vertices[1].offset);
    SMAAEdgeDetectionVS(vertices[2].uv, vertices[2].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
}

float2 EdgePS(EdgeGSOut input) : SV_Target0  // edgeTex
{
    return SMAAColorEdgeDetectionPS(input.uv, input.offset, gColorTexGamma);
}

[maxvertexcount(3)]
void BlendGS(point float4 input[1] : SV_Position, inout TriangleStream<BlendGSOut> output)
{
    BlendGSOut vertices[3];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 1.0f, 1.0f);
    vertices[1].position = float4(-1.0f, 3.0f, 1.0f, 1.0f);
    vertices[2].position = float4(3.0f, -1.0f, 1.0f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, -1.0f);
    vertices[2].uv = float2(2.0f, 1.0f);
    
    SMAABlendingWeightCalculationVS(vertices[0].uv, vertices[0].pixuv, vertices[0].offset);
    SMAABlendingWeightCalculationVS(vertices[1].uv, vertices[1].pixuv, vertices[1].offset);
    SMAABlendingWeightCalculationVS(vertices[2].uv, vertices[2].pixuv, vertices[2].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
}

float4 BlendPS(BlendGSOut input) : SV_Target0 //blendTex
{
    return SMAABlendingWeightCalculationPS(input.uv, input.pixuv, input.offset,
    gEdgesTex, gAreaTex, gSearchTex, gSubsampleIndices);
}


[maxvertexcount(3)]
void NeiBlendGS(point float4 input[1] : SV_Position, inout TriangleStream<NeiBlendGSOut> output)
{
    NeiBlendGSOut vertices[3];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 1.0f, 1.0f);
    vertices[1].position = float4(-1.0f, 3.0f, 1.0f, 1.0f);
    vertices[2].position = float4(3.0f, -1.0f, 1.0f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, -1.0f);
    vertices[2].uv = float2(2.0f, 1.0f);
    
    SMAANeighborhoodBlendingVS(vertices[0].uv, vertices[0].offset);
    SMAANeighborhoodBlendingVS(vertices[1].uv, vertices[1].offset);
    SMAANeighborhoodBlendingVS(vertices[2].uv, vertices[2].offset);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
}

float4 NeiBlendPS(NeiBlendGSOut input) : SV_Target0 
{
    float4 result = SMAANeighborhoodBlendingPS(input.uv, input.offset, gColorTexGamma, gBlendTex);
    //result.rgb = pow(result.rgb, 1.0f / 2.2f);
    
    
    return result;
}