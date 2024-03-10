struct Character
{
    uint stringInfoIndex;
    uint index;
};

struct StringInfo
{
    float4x4 transform;
    float4 color;
    uint fontSize;
    float pad0;
    float pad1;
    float pad2;
};

struct FontMeshInfo
{
    uint advance_width;
    uint left_side_bearing;
    float2 glyph_center;
    uint4 bounding_box;
    uint numVertex;
    uint vertexOffset;
    uint numIndex;
    uint indexOffset;
};

struct VSIn
{
    float2 lPos : POSITION;
    uint instanceID : SV_InstanceID;
};

struct VSOut
{
    float4 postion : SV_Position;
    float4 color : COLOR0;
};

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

float4x4 gOrthographicMatrix : register(b0, space0);
uint gOffsetCharStream : register(b1, space0);
uint gFontMeshInfoIndex : register(b2, space0);

StructuredBuffer<StringInfo> gStringInfos : register(t0, space0);
StructuredBuffer<Character> gCharacterStream : register(t1, space0);
StructuredBuffer<FontMeshInfo> gFontMeshInfos : register(t2, space0);

VSOut VS(VSIn vin)
{
    VSOut result;
    
    FontMeshInfo meshInfo = gFontMeshInfos[gFontMeshInfoIndex];
    Character currChar = gCharacterStream[gOffsetCharStream + vin.instanceID];
    StringInfo stringInfo = gStringInfos[currChar.stringInfoIndex];
    
    float fontSize = float(stringInfo.fontSize) / meshInfo.advance_width;
    float leftSideBearing = (float(meshInfo.left_side_bearing) / meshInfo.advance_width) * stringInfo.fontSize;
    result.postion.xy = float4((vin.lPos.xy * fontSize), 0, 1);
    result.postion.x += fontSize * currChar.index + leftSideBearing;
    result.postion = mul(result.postion, stringInfo.transform);
    result.color = stringInfo.color;
    
    return result;
}

float4 PS(VSOut pin)
{
    return pin.color;
}