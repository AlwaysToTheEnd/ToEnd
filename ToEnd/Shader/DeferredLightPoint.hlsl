#include "DeferredRenderBase.hlsli"

cbuffer LightData : register(b1, space0)
{
    float3 gPosW;
    float gPower;
    float3 gDir;
    float gLength;
    float3 gColor;
    float gPad1_;
};

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
    float3 dir : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PointLightConstantHS")]
HSOut HS()
{
    HSOut result;
    result.dir = float3(0, 0, 0);
    return result;
}

struct DSOut
{
    float4 position : SV_position;
    float2 cpPos : TEXCOORD0;
};


[domain("quad")]
DSOut DS(HSConstDataOut input, float2 uv : SV_DomainLocation, const OutputPatch<HSOut, 4> quad)
{
    DSOut result;
    
    float2 posClipSpace = uv.xy * 2.0f - 1.0f;
    float2 posClipSpaceAbs = abs(posClipSpace.xy);
    float maxLen = max(posClipSpaceAbs.x, posClipSpaceAbs.y);
    
    float3 centerPos = mul(float4(gPosW.xyz, 1.0f), gView).xyz;
    float3 dir = normalize(float3(posClipSpace.xy, -(maxLen - 1.0f))) * gLength;
    float4 posLS = float4(centerPos + dir.xyz, 1.0f);
    
    result.position = mul(posLS, gProj);
    result.cpPos = result.position.xy / result.position.w;
    
    return result;
}

float4 PS(DSOut inDLight) : SV_Target0
{
    float4 finalColor = float4(0, 0, 0, 1);
    SurfaceData gbData = UnpackGBufferL(inDLight.position.xy);
    
    if (gDepthTexture.Load(float3(inDLight.position.xy, 0)).x == 1.0f)
    {
        clip(-1);
    }
    
    float4 viewPos = float4(inDLight.cpPos * float2(1 / gProj._11, 1 / gProj._22) * gbData.linearDepth, gbData.linearDepth, 1.0f);
    float3 positionW = mul(viewPos, gInvView).xyz;
    float3 dir = positionW - gPosW;
    float dirSqr = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    
    if (dirSqr > gLength * gLength)
    {
        clip(-1);
    }
    
    float3 fresRef = lerp(Fdielectric, gbData.color.rgb, gbData.metal_rough_ao.x);
    float dstToLightNormal = 1.0f - saturate(sqrt(dirSqr) / gLength);
    float3 toEye = normalize(gEyePosW - positionW);
    
    float3 revLightDir = -dir;
    float3 halfWay = normalize(toEye + revLightDir);
    
    float cosEye = max(0.0f, dot(gbData.normal, toEye));
    float cosLight = max(0.0f, dot(gbData.normal, revLightDir));
    float cosHalfWay = max(0.0f, dot(gbData.normal, halfWay));
        
    float3 F = FresnelSchlick(fresRef, max(0.0f, dot(halfWay, toEye)));
    float D = NdfGGX(cosHalfWay, gbData.metal_rough_ao.y);
    float G = GaSchlickGGX(cosLight, cosEye, gbData.metal_rough_ao.y);
        
    float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0, 0, 0), gbData.metal_rough_ao.x);
        
    float3 diffuseBRDF = kd * gbData.color.rgb;
    float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLight * cosEye);
        
    finalColor.rgb = (diffuseBRDF + specularBRDF) * gColor * cosLight * dstToLightNormal;
    finalColor.a = gPower;
    
    return finalColor;
}