SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct CharInfo
{
    float xOffsetInString;
    uint stringIndex;
    uint glyphID;
    uint stringInfoID;
};

struct StringInfo
{
    float4 color;
    float3 pos;
    float size;
    uint renderID;
    float3 pad0;
};

struct Glyph
{
    int pad0;
    int subrect_left;
    int subrect_top;
    int subrect_right;
    int subrect_bottom;
    float xOffset;
    float yOffset;
    float xAdvance;
};

cbuffer FontRenderConst : register(b0)
{
    float gLineSpacing;
    float gWindowWidthReciprocal;
    float gWindowHeightReciprocal;
    float gTextureWidthReciprocal;
    float gTextureHeightReciprocal;
};

Texture2D<float4> gSpriteTexture : register(t0);
StructuredBuffer<CharInfo> gCharInfos : register(t1);
StructuredBuffer<StringInfo> gStringInfos : register(t2);
StructuredBuffer<Glyph> gGlyphs : register(t3);

struct VSOut
{
    uint vIndex : VERTEXID;
};

struct PSOut
{
    uint renderID : SV_Target0;
    float4 color : SV_Target1;
};

VSOut VS(uint vIndex : SV_VertexID)
{
    VSOut result;
    result.vIndex = vIndex;
    return result;
}

struct GSOut
{
    float4 position : SV_position;
    float2 uv : TEXCOORD0;
    nointerpolation uint stringInfoID : TEXCOORD1;
};

[maxvertexcount(4)]
void GS(point VSOut input[1] : SV_Position, inout TriangleStream<GSOut> output)
{
    CharInfo cInfo = gCharInfos[input[0].vIndex];
    Glyph currGlyph = gGlyphs[cInfo.glyphID];
    StringInfo sInfo = gStringInfos[cInfo.stringInfoID];
    
    float2 uvLT = float2(float(currGlyph.subrect_left) * gTextureWidthReciprocal, float(currGlyph.subrect_right) * gTextureHeightReciprocal);
    float2 uvRB = float2(float(currGlyph.subrect_left) * gTextureWidthReciprocal, float(currGlyph.subrect_right) * gTextureHeightReciprocal);
    
    float2 size = float2(gWindowWidthReciprocal, gWindowHeightReciprocal) * 2.0f * sInfo.size;
    float2 fontSize = float2(currGlyph.subrect_right - currGlyph.subrect_left, currGlyph.subrect_bottom - currGlyph.subrect_top) * size;
    float2 posLT = float2(sInfo.pos.x * gWindowWidthReciprocal, sInfo.pos.y * gWindowHeightReciprocal) * 2.0f - 1.0f;
    float2 posRB = float2(posLT.x + fontSize.x + (size.x * cInfo.xOffsetInString), posLT.y - fontSize.y);
    
    GSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(posLT.x, posRB.y, sInfo.pos.z, 1.0f);
    vertices[1].position = float4(posLT.x, posLT.y, sInfo.pos.z, 1.0f);
    vertices[2].position = float4(posRB.x, posRB.y, sInfo.pos.z, 1.0f);
    vertices[3].position = float4(posRB.x, posLT.y, sInfo.pos.z, 1.0f);
    
    vertices[0].uv = float2(uvLT.x, uvRB.y);
    vertices[1].uv = float2(uvLT.x, uvLT.y);
    vertices[2].uv = float2(uvRB.x, uvRB.y);
    vertices[3].uv = float2(uvRB.x, uvLT.y);
    
    vertices[0].stringInfoID = cInfo.stringInfoID;
    vertices[1].stringInfoID = cInfo.stringInfoID;
    vertices[2].stringInfoID = cInfo.stringInfoID;
    vertices[3].stringInfoID = cInfo.stringInfoID;

    output.Append(vertices[0]);
    output.Append(vertices[2]);
    output.Append(vertices[1]);
    output.Append(vertices[3]);
}

PSOut PS(GSOut pin)
{
    PSOut result;
    StringInfo sInfo = gStringInfos[pin.stringInfoID];
    result.color = gSpriteTexture.Sample(gsamLinearWrap, pin.uv);
    result.renderID = sInfo.renderID;
    result.color = sInfo.color;
    
    if (result.color.a != 0.0f)
    {
        result.color = sInfo.color;
    }
    
    return result;
}