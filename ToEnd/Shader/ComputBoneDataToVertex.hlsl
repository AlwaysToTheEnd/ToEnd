struct BoneWeightInfo
{
    uint numWeight;
    uint offsetIndex;
};

struct BoneWeight
{
    uint boneIndex;
    float weight;
};

cbuffer BoneComputePass : register(b0, space0)
{
    float4x4 gWorldMat;
};

cbuffer CurrBonePass : register(b1, space0)
{
    float4x4 gCurrBone;
    uint gNumVertices;
    float3 gPad0;
};

RWStructuredBuffer<float3> gResultVertices : register(u0, space0);
StructuredBuffer<float3> gVertices : register(t0, space0);

StructuredBuffer<BoneWeightInfo> gBoneWeightInfos : register(t1, space0);
StructuredBuffer<BoneWeight> gBoneWeights : register(t2, space0);
StructuredBuffer<float4x4> gBoneData : register(t3, space0);

[numthreads(32, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < gNumVertices)
    {
        BoneWeightInfo weightInfo = gBoneWeightInfos[id.x];
        float4 oriPos = float4(gVertices[id.x], 1.0f);
        
        float3 sumPosL = float3(0.0f, 0.0f, 0.0f);
        BoneWeight boenWeight;
        [loop]
        for (uint i = 0; i < weightInfo.numWeight; i++)
        {
            boenWeight = gBoneWeights[weightInfo.offsetIndex + i];
        
            sumPosL += boenWeight.weight * mul(oriPos, gBoneData[boenWeight.boneIndex]).xyz;
        }
        
        gResultVertices[id.x] = sumPosL;
    }
   
}