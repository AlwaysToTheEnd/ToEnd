struct TextureInfo
{
    float pad0;
    uint type;
    uint mapping;
    uint unChannelIndex;
    float blend;
    uint textureOp;
    uint3 mapMode;
    float3 pad1;
};

cbuffer MaterialData : register(b1, space0)
{
    int gShadingModel;
    int gTwosided;
    int gWireframe;
    int gBlend;
    float3 gDiffuse;
    float gOpacity;
    float3 gAmbient;
    float gBumpscaling;
    float3 gSpecular;
    float gShininess;
    float3 gEmissive;
    float gRefracti;
    float3 gTransparent;
    float gShinpercent;
    float3 gReflective;
    float gReflectivity;
    
    float gMetalness;
    float gRoughness;
    
    uint gNumTexture;
    uint renderQueue;
    TextureInfo gTextureInfo[16];
};

Texture2D gTextures[16] : register(t0, space0);