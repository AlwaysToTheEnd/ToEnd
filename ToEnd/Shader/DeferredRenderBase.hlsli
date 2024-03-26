struct SurfaceData
{
    float4 Color;
    float3 Normal;
    float LinearDepth;
    float SpecPower;
    int ObjectID;
};

SurfaceData UnpackGBufferL(int2 location)
{
    SurfaceData result;
    
    int3 location3 = int3(location, 0);
    float depth = gDepthTexture.Load(location3).x;
    result.Normal = gNormalTexture.Load(location3).xyz;
    
    result.LinearDepth = gProj._43 / (depth - gProj._33);
    result.Color = gColorTexture.Load(location3);
    result.Normal = normalize(result.Normal * 2.0f - 1.0);
    result.SpecPower = gSpecPowerTexture.Load(location3);
    result.ObjectID = gObjectIDTexture.Load(location3);
    
    return result;
}