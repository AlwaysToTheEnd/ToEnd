SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

static const float epsilon = 0.000001f;

struct CharInfo
{
    float4 color;
    float2 ltP;
    float2 rbP;
    float depth;
    uint glyphID;
    float2 pad0;
};

struct Glyph
{
    float2 uvLT;
    float2 uvRB;
};

StructuredBuffer<CharInfo> gCharInfos : register(t0, space0);
StructuredBuffer<Glyph> gGlyphs : register(t1, space0);
Texture2D<float4> gSpriteTexture : register(t2, space0);

struct VSOut
{
    uint vIndex : VERTEXID;
};

struct PSOut
{
    float4 color : SV_Target0;
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
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

[maxvertexcount(4)]
void GS(point VSOut input[1] : SV_Position, inout TriangleStream<GSOut> output)
{
    CharInfo cInfo = gCharInfos[input[0].vIndex];
    Glyph currGlyph = gGlyphs[cInfo.glyphID];
  
    GSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(cInfo.ltP.x, cInfo.rbP.y, cInfo.depth, 1.0f);
    vertices[1].position = float4(cInfo.ltP.x, cInfo.ltP.y, cInfo.depth, 1.0f);
    vertices[2].position = float4(cInfo.rbP.x, cInfo.rbP.y, cInfo.depth, 1.0f);
    vertices[3].position = float4(cInfo.rbP.x, cInfo.ltP.y, cInfo.depth, 1.0f);
    
    vertices[0].uv = float2(currGlyph.uvLT.x, currGlyph.uvRB.y);
    vertices[1].uv = float2(currGlyph.uvLT.x, currGlyph.uvLT.y);
    vertices[2].uv = float2(currGlyph.uvRB.x, currGlyph.uvRB.y);
    vertices[3].uv = float2(currGlyph.uvRB.x, currGlyph.uvLT.y);
    
    vertices[0].color = cInfo.color;
    vertices[1].color = cInfo.color;
    vertices[2].color = cInfo.color;
    vertices[3].color = cInfo.color;

    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

PSOut PS(GSOut pin)
{
    PSOut result;
    result.color = gSpriteTexture.Sample(gsamAnisotropicClamp, pin.uv) * pin.color;
    
    return result;
}