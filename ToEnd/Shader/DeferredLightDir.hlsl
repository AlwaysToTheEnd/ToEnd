#include "DeferredRenderBase.hlsli"

float4 VS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

struct DLightGSOut
{
    float4 position : SV_Position;
};

[maxvertexcount(4)]
void GS(point float4 input[1], inout TriangleStream<DLightGSOut> output)
{
    DLightGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 1.0f, 1.0f);
    vertices[1].position = float4(-1.0f, 1.0f, 1.0f, 1.0f);
    vertices[2].position = float4(1.0f, -1.0f, 1.0f, 1.0f);
    vertices[3].position = float4(1.0f, 1.0f, 1.0f, 1.0f);

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
    
    finalColor *= gbData.color.rgb;
    
    return float4(finalColor, gPower);
}