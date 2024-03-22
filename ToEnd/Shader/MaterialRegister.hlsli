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
    int shadingModel;
    int twosided;
    int wireframe;
    int blend;
    float3 diffuse;
    float opacity;
    float3 ambient;
    float bumpscaling;
    float3 specular;
    float shininess;
    float3 emissive;
    float refracti;
    float3 transparent;
    float shinpercent;
    float3 reflective;
    float reflectivity;
    uint gNumTexture;
    float3 pad0;
    TextureInfo gTextureInfo[16];
};

Texture2D gTextures[16] : register(t0, space0);