struct UIInfo
{
    uint uiGraphicType;
    float3 pos;
    float4 color;
    float2 uvLT;
    float2 uvRB;
    float2 size;
    uint renderID;
    float pad0;
};

cbuffer window : register(b0, space0)
{
    float2 gWinSizeReciprocal;
};

StructuredBuffer<UIInfo> gUIInfos : register(t0, space0);

struct VSOut
{
    uint vIndex : VERTEXID;
};

struct GSOut
{
    float4 position : SV_Position;
    nointerpolation uint renderID : RENDERID;
};

struct PSOut
{
    uint renderID : SV_Target0;
};

VSOut VS(uint vIndex : SV_VertexID)
{
    VSOut result;
    result.vIndex = vIndex;
    
    return result;
}

[maxvertexcount(4)]
void GS(point VSOut input[1] : SV_Position, inout TriangleStream<GSOut> output)
{
    GSOut vertices[4];
    UIInfo info = gUIInfos[input[0].vIndex];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(info.pos.x, info.pos.y + info.size.y, info.pos.z, 1.0f);
    vertices[1].position = float4(info.pos.x, info.pos.y, info.pos.z, 1.0f);
    vertices[2].position = float4(info.pos.x + info.size.x, info.pos.y + info.size.y, info.pos.z, 1.0f);
    vertices[3].position = float4(info.pos.x + info.size.x, info.pos.y, info.pos.z, 1.0f);

    vertices[0].renderID = info.renderID;
    vertices[1].renderID = info.renderID;
    vertices[2].renderID = info.renderID;
    vertices[3].renderID = info.renderID;
    
    output.Append(vertices[0]);
    output.Append(vertices[2]);
    output.Append(vertices[1]);
    output.Append(vertices[3]);
}

PSOut PS(GSOut pin)
{
    PSOut result;
    result.renderID = pin.renderID;
    return result;
}