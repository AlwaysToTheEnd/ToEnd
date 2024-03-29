#include "RenderBaseRegister.hlsli"

struct SurfaceData
{
    float4 color;
    float3 normal;
    float3 metal_rough_emis;
    float3 ao_fres_anis;
    float linearDepth;
};

Texture2D gDiffuseTexture : register(t0, sapce0);
Texture2D gNormalTexture : register(t1, sapce0);
Texture2D gMRETexture : register(t2, sapce0);
Texture2D gAFATexture : register(t3, sapce0);
Texture2D gDepthTexture : register(t4, sapce0);

SurfaceData UnpackGBufferL(int2 location)
{
    SurfaceData result;
    
    int3 location3 = int3(location, 0);
    float depth = gDepthTexture.Load(location3).x;
    result.normal = gNormalTexture.Load(location3).xyz;
    
    result.linearDepth = gProj._43 / (depth - gProj._33);
    result.color = gDiffuseTexture.Load(location3);
    result.normal = normalize(result.normal * 2.0f - 1.0);
    result.metal_rough_emis = gMRETexture.Load(location3).rgb;
    result.ao_fres_anis = gAFATexture.Load(location3).rgb;
    
    return result;
}

float3 CalcWorldPos(float2 csPos, float depth)
{
    float4 position;

    position.xy = csPos.xy * float2(1 / gProj._11, 1 / gProj._22) * depth;
    position.z = depth;
    position.w = 1.0;
	
    return mul(position, gInvView).xyz;
}