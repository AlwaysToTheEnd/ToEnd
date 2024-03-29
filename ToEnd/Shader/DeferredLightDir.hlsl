#include "DeferredRenderBase.hlsli"

struct DLightVSOut
{
    uint index : LIGHTDATAINDEX;
};

struct DLightGSOut
{
    float4 position : SV_Position;
    nointerpolation uint index : LIGHTDATAINDEX;
};

cbuffer LightData : register(b0, space1)
{
    float4 gColor;
    float3 gDir;
    float gPower;
};

DLightVSOut VS(uint id : SV_VertexID)
{
    DLightVSOut result;
    result.index = id;
    return result;
}

[maxvertexcount(4)]
void GS(point DLightVSOut input[1], inout TriangleStream<DLightGSOut> output)
{
    DLightGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.00001f, 1.0f);
    vertices[0].index = input[0].index;
    vertices[1].position = float4(-1.0f, 1.0f, 0.00001f, 1.0f);
    vertices[1].index = input[0].index;
    vertices[2].position = float4(1.0f, -1.0f, 0.00001f, 1.0f);
    vertices[2].index = input[0].index;
    vertices[3].position = float4(1.0f, 1.0f, 0.00001f, 1.0f);
    vertices[3].index = input[0].index;

    output.Append(vertices[0]);
    output.Append(vertices[2]);
    output.Append(vertices[1]);
    output.Append(vertices[3]);
}

float4 PS(DLightGSOut inDLight) : SV_Target
{
    if (gDepthTexture.Load(float3(inDLight.position.xy, 0)).x == 1.0f)
    {
        clip(-1);
    }
    
    SurfaceData gbData = UnpackGBufferL(inDLight.position.xy);
    
    float3 worldPos = CalcWorldPos(inDLight.position.xy, gbData.linearDepth);
    float nDotl = dot(-gDir, gbData.normal);
    float3 finalColor = gColor * saturate(nDotl);
    float3 toEye = normalize(gEyePosW - worldPos);
    float3 halfWay = normalize(toEye + -gDir);
    float nDoth = saturate(dot(halfWay, gbData.normal));
    finalColor += gColor * pow(nDoth, gbData.metal_rough_emis.b);
    
    finalColor *= gbData.color;
    
    return float4(finalColor, gPower);
}