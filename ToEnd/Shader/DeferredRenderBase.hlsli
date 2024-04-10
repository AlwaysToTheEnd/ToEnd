#include "RenderBaseRegister.hlsli"

struct SurfaceData
{
    float4 color;
    float3 normal;
    float3 metal_rough_ao;
    float3 emissionColor;
    float linearDepth;
};

Texture2D<float4> gDiffuseTexture : register(t0);
Texture2D<float3> gNormalTexture : register(t1);
Texture2D<float3> gMRATexture : register(t2);
Texture2D<float3> gEmissionTexture : register(t3);
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
    result.metal_rough_ao = gMRATexture.Load(location3);
    result.emissionColor = gEmissionTexture.Load(location3);
    
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

float NdfGGX(float cosHalfWay, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    
    float denom = (cosHalfWay * cosHalfWay) * (alphaSq - 1.0f) + 1.0f;
    return alphaSq / (PI * denom * denom);
}

float GaSchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0f - k) + k);
}

float GaSchlickGGX(float cosLight, float cosEye, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return GaSchlickG1(cosLight, k) * GaSchlickG1(cosEye, k);
}

float3 FresnelSchlick(float3 f0, float cosLight)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosLight, 5.0f);
}
