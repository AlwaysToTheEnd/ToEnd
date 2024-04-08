#include "RenderBaseRegister.hlsli"

struct SurfaceData
{
    float4 color;
    float3 normal;
    float3 metal_rough_emis;
    float3 ao_fres_anis;
    float linearDepth;
};

Texture2D<float4> gDiffuseTexture : register(t0);
Texture2D<float3> gNormalTexture : register(t1);
Texture2D<float3> gMRETexture : register(t2);
Texture2D<float3> gAFATexture : register(t3);
Texture2D<float> gDepthTexture : register(t4);

static const float3 Fdielectric = 0.04;
static const float PI = 3.141592;
static const float Epsilon = 0.00001;

SurfaceData UnpackGBufferL(int2 location)
{
    SurfaceData result;
    
    int3 location3 = int3(location, 0);
    float depth = gDepthTexture.Load(location3).x;
    result.normal = gNormalTexture.Load(location3);
    
    result.linearDepth = gProj._43 / (depth - gProj._33);
    result.color = gDiffuseTexture.Load(location3);
    result.normal = normalize(result.normal * 2.0f - 1.0);
    result.metal_rough_emis = gMRETexture.Load(location3);
    result.ao_fres_anis = gAFATexture.Load(location3);
    
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