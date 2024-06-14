#include "DeferredRenderBase.hlsli"

struct LightData
{
    float3 dir;
    float power;
    float3 color;
    int shadowMapIndex;
    float4x4 shadowMapMat;
    bool enableShadow;
    float shadowDistance;
    float shadowNear;
    float shadowFar;
};

Texture2D<float> gShadowMapTexture[8] : register(t0, space1);

uint gNumLight : register(b1, space0);
StructuredBuffer<LightData> gLightDatas : register(t5);

float4 VS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

struct DLightGSOut
{
    float4 position : SV_Position;
    float2 cpPos : TEXCOORD0;
};

float CalcSahdowFactor(float4 shadowPos, uint shadowMapIndex)
{
    uint width, height, numMips;
    gShadowMapTexture[shadowMapIndex].GetDimensions(0, width, height, numMips);
    
    float dx = 1.0f / (float) width;
    
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMapTexture[shadowMapIndex].SampleCmpLevelZero(gsamShadow, shadowPos.xy + offsets[i], shadowPos.z);
    }
    
    return percentLit / 9.0f;
}


[maxvertexcount(4)]
void GS(point float4 input[1] : SV_Position, inout TriangleStream<DLightGSOut> output)
{
    DLightGSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.0001f, 1.0f);
    vertices[0].cpPos = vertices[0].position.xy;
    vertices[1].position = float4(-1.0f, 1.0f, 0.0001f, 1.0f);
    vertices[1].cpPos = vertices[1].position.xy;
    vertices[2].position = float4(1.0f, -1.0f, 0.0001f, 1.0f);
    vertices[2].cpPos = vertices[2].position.xy;
    vertices[3].position = float4(1.0f, 1.0f, 0.0001f, 1.0f);
    vertices[3].cpPos = vertices[3].position.xy;

    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

float4 PS(DLightGSOut inDLight) : SV_Target
{
    float depth = gDepthTexture.Load(float3(inDLight.position.xy, 0)).x;
    if (depth == 1.0f)
    {
        clip(-1);
    }
    
    SurfaceData gbData = UnpackGBufferL(inDLight.position.xy);
    
    float3 worldPos = CalcWorldPos(inDLight.cpPos, gbData.linearDepth);
    float3 toEye = normalize(gEyePosW - worldPos);
    float cosEye = max(0.0f, dot(gbData.normal, toEye));
    float3 fresRef = lerp(Fdielectric, gbData.color.rgb, gbData.metal_rough_ao.x);
    
    float3 finalColor = float3(0, 0, 0);
    for (uint index = 0; index < gNumLight; index++)
    {
        LightData currLight = gLightDatas[index];
        float3 revLightDir = -currLight.dir;
        float3 halfWay = normalize(revLightDir + toEye);
        
        float cosLight = max(0.0f, dot(gbData.normal, revLightDir));
        float cosHalfWay = max(0.0f, dot(gbData.normal, halfWay));
        
        float3 F = FresnelSchlick(fresRef, max(0.0f, dot(halfWay, toEye)));
        float D = NdfGGX(cosHalfWay, gbData.metal_rough_ao.y);
        float G = GaSchlickGGX(cosLight, cosEye, gbData.metal_rough_ao.y);
        
        float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0, 0, 0), gbData.metal_rough_ao.x);
        
        float3 diffuseBRDF = kd * gbData.color.rgb;
        float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLight * cosEye);
        
        float shadowFactor = 1.0f;
        
        if (currLight.shadowMapIndex > -1)
        {
            float4 posInShadow = mul(float4(worldPos, 1.0f), currLight.shadowMapMat);
            shadowFactor = CalcSahdowFactor(posInShadow, currLight.shadowMapIndex);
        }
        
        finalColor += (diffuseBRDF + specularBRDF) * currLight.color * cosLight * currLight.power * shadowFactor;
    }
    
    float3 specRefVec = 2.0f * cosEye * gbData.normal - toEye;
    
    float3 ambientLight = gAmbientLight.rgb * gbData.color.rgb;
    
    return float4(finalColor + ambientLight, 1.0f);
}