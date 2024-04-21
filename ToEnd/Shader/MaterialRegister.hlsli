struct TextureInfo
{
    uint pad0;
    uint type;
    uint mapping;
    uint uvChannelIndex;
    uint textureOp;
    uint mapMode;
    float blend;
    uint srcAlphaOP;
};

#define ALPHAOP_ONE 0
#define ALPHAOP_MULTIPLY 1
#define ALPHAOP_INV_MULTIPLY 2

#define TEXTYPE_BASE_COLOR 12
#define TEXTYPE_NORMAL 13
#define TEXTYPE_EMISSION_COLOR 14
#define TEXTYPE_METALNESS 15
#define TEXTYPE_ROUGHNESS 16
#define TEXTYPE_AO 17
#define TEXTYPE_UNKNOWN 18
    //aiTextureType_DIFFUSE = 1,
    //aiTextureType_SPECULAR = 2,
    //aiTextureType_AMBIENT = 3,
    //aiTextureType_EMISSIVE = 4,
    //aiTextureType_HEIGHT = 5,
    //aiTextureType_NORMALS = 6,
    //aiTextureType_SHININESS = 7,
    //aiTextureType_OPACITY = 8,
    //aiTextureType_DISPLACEMENT = 9,
    //aiTextureType_LIGHTMAP = 10,
    //aiTextureType_REFLECTION = 11,

    ///** PBR Materials
    //aiTextureType_BASE_COLOR = 12,
    //aiTextureType_NORMAL_CAMERA = 13,
    //aiTextureType_EMISSION_COLOR = 14,
    //aiTextureType_METALNESS = 15,
    //aiTextureType_DIFFUSE_ROUGHNESS = 16,
    //aiTextureType_AMBIENT_OCCLUSION = 17,
    //aiTextureType_UNKNOWN = 18,

#define TEXOP_MULTIPLY 0
#define TEXOP_ADD 1
#define TEXOP_SUBTRACT 2
#define TEXOP_DIVIDE 3
#define TEXOP_SMOOTHADD 4
#define TEXOP_SIGNEDADD 5
    ///** T = T1 * T2 */
    //aiTextureOp_Multiply = 0x0,
    ///** T = T1 + T2 */
    //aiTextureOp_Add = 0x1,
    ///** T = T1 - T2 */
    //aiTextureOp_Subtract = 0x2,
    ///** T = T1 / T2 */
    //aiTextureOp_Divide = 0x3,
    ///** T = (T1 + T2) - (T1 * T2) */
    //aiTextureOp_SmoothAdd = 0x4,
    ///** T = T1 + (T2-0.5) */
    //aiTextureOp_SignedAdd = 0x5,

#define TEXMAPMO_WRAP 0
#define TEXMAPMO_CLAMP 1
#define TEXMAPMO_MIRROR 2
#define TEXMAPMO_DECAL 3
    //aiTextureMapMode_Wrap = 0x0,
    //aiTextureMapMode_Clamp = 0x1,
    //aiTextureMapMode_Decal = 0x3,
    //aiTextureMapMode_Mirror = 0x2,

    //aiTextureMapping_UV = 0x0,
    //aiTextureMapping_SPHERE = 0x1,
    //aiTextureMapping_CYLINDER = 0x2,
    //aiTextureMapping_BOX = 0x3,
    //aiTextureMapping_PLANE = 0x4,
    //aiTextureMapping_OTHER = 0x5,

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
    float gDiffuseAlpha;
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