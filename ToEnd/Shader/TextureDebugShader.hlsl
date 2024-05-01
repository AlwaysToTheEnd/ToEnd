SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer window : register(b0, space0)
{
    uint2 windowSize;
};

Texture2D gColorTex : register(t0);
Texture2D gColorTexGamma : register(t1);
Texture2D gSearchTex : register(t2);
Texture2D gAreaTex : register(t3);
Texture2D gEdgesTex : register(t4);
Texture2D gBlendTex : register(t5);

struct VSOut
{
    uint vIndex : VERTEXID;
};

struct GSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 VS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

[maxvertexcount(4)]
void GS(point float4 input[1] : SV_Position, inout TriangleStream<GSOut> output)
{
    GSOut vertices[4];
    // 1  3
	// |\ |
	// 0 \2
    vertices[0].position = float4(-1.0f, -1.0f, 0.1f, 1.0f);
    vertices[1].position = float4(-1.0f, 0.0f, 0.1f, 1.0f);
    vertices[2].position = float4(0.0f, -1.0f, 0.1f, 1.0f);
    vertices[3].position = float4(0.0f, 0.0f, 0.1f, 1.0f);

    vertices[0].uv = float2(0.0f, 1.0f);
    vertices[1].uv = float2(0.0f, 0.0f);
    vertices[2].uv = float2(1.0f, 1.0f);
    vertices[3].uv = float2(1.0f, 0.0f);
    
    output.Append(vertices[0]);
    output.Append(vertices[1]);
    output.Append(vertices[2]);
    output.Append(vertices[3]);
}

float4 PS(GSOut pin) : SV_Target0
{
    float4 color = float4(1, 1, 1, 1);
    
    color.rgb = gBlendTex.SampleLevel(gsamLinearClamp, pin.uv, 0).rgb;

    return color;
}