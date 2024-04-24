SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

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
//Texture2D<float4> gBackgourndTexture : register(t1, space0);

struct VSOut
{
    uint vIndex : VERTEXID;
};

struct GSOut
{
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    nointerpolation uint uiGraphicType : UITYPE;
};

struct PSOut
{
    float4 color : SV_Target0;
    float depth : SV_Depth;
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

    vertices[0].uv = float2(info.uvLT.x, info.uvRB.y);
    vertices[1].uv = float2(info.uvLT.x, info.uvLT.y);
    vertices[2].uv = float2(info.uvRB.x, info.uvRB.y);
    vertices[3].uv = float2(info.uvRB.x, info.uvLT.y);
    
    [unroll(4)]
    for (int index = 0; index < 4; index++)
    {
        vertices[index].position.x = vertices[index].position.x * gWinSizeReciprocal.x * 2.0f - 1.0f;
        vertices[index].position.y = 1.0f - (vertices[index].position.y * gWinSizeReciprocal.y * 2.0f);
        vertices[index].color = info.color;
        vertices[index].uiGraphicType = info.uiGraphicType;
    }
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

PSOut PS(GSOut pin)
{
    PSOut result;
    result.color = pin.color;
    
    //switch (pin.uiGraphicType)
    //{
    //    case 1:{
    //            result.color = gBackgourndTexture.Sample(gsamLinearWrap, pin.uv);
    //        }
    //        break;
    //}
    
    if(result.color.a ==0.0f)
    {
        result.depth = 1.0f;
    }
    else
    {
        result.depth = pin.position.z;
    }
    
    return result;
}