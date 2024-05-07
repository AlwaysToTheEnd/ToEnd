struct WeightInfo
{
    uint numWeight;
    uint offsetIndex;
};

struct BoneWeight
{
    uint boneIndex;
    float weight;
};

cbuffer CurrBonePass : register(b0, space0)
{
    uint gNumVertices;
};

StructuredBuffer<float3> gVertices : register(t0, space0);
StructuredBuffer<float3> gNormals : register(t1, space0);
StructuredBuffer<float3> gTangents : register(t2, space0);
StructuredBuffer<float3> gBitans : register(t3, space0);

StructuredBuffer<WeightInfo> gWeightInfos : register(t4, space0);
StructuredBuffer<BoneWeight> gBoneWeights : register(t5, space0);

RWStructuredBuffer<float3> gResultVertices : register(u0, space0);
RWStructuredBuffer<float3> gResultNormals : register(u1, space0);
RWStructuredBuffer<float3> gResultTangents : register(u2, space0);
RWStructuredBuffer<float3> gResultBitans : register(u3, space0);

StructuredBuffer<float4x4> gBoneData : register(t6, space0);

[numthreads(32, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < gNumVertices)
    {
        float4 oriPos = float4(gVertices[id.x], 1.0f);
        float3 oriNormal = gNormals[id.x];
        float3 oriTangent = gTangents[id.x];
        float3 oriBitan = gBitans[id.x];
    
        WeightInfo weightInfo = gWeightInfos[id.x];
    
        float3 sumPos = float3(0, 0, 0);
        float3 sumNormal = float3(0, 0, 0);
        float3 sumTangent = float3(0, 0, 0);
        float3 sumBitan = float3(0, 0, 0);
    
        BoneWeight boenWeight;
        [loop]
        for (uint i = 0; i < weightInfo.numWeight; i++)
        {
            boenWeight = gBoneWeights[weightInfo.offsetIndex + i];
            float4x4 currBone = gBoneData[boenWeight.boneIndex];
        
            sumPos += boenWeight.weight * mul(oriPos, currBone);
            sumNormal += boenWeight.weight * mul(oriNormal, (float3x3) currBone);
            sumTangent += boenWeight.weight * mul(oriTangent, (float3x3) currBone);
            sumBitan += boenWeight.weight * mul(oriBitan, (float3x3) currBone);
        }
    
        gResultVertices[id.x] = sumPos.xyz;
        gResultNormals[id.x] = normalize(sumNormal);
        gResultTangents[id.x] = normalize(sumTangent);
        gResultBitans[id.x] = normalize(sumBitan);
    }
}