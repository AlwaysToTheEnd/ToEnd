Texture2D<float4> gHDRTexture : register(t0);
RWStructuredBuffer<float> gAverageLum : register(u0);

cbuffer DownScaleConstants : register(b0)
{
    uint2 gWindowSize : packoffset(c0);
    uint gThreadOffsetW : packoffset(c0.z);
    uint gNumDonwScale : packoffset(c0.w);
    uint gLumBufferSize;
};

static const float4 LUM_FACTOR = float4(0.299f, 0.587f, 0.114f, 0.0f);

void DownScale4x4(uint2 currPixel, uint storeIndex)
{
    float avgLum = 0.0f;
    float4 downScaled = float4(0, 0, 0, 0);
    int count = 0;
        
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (currPixel.x + i < gWindowSize.x)
        {
            break;
        }
        
        [unroll]
        for (int j = 0; j < 4; j++)
        {
            if (currPixel.y + j >= gWindowSize.y)
            {
                break;
            }
                
            downScaled += gHDRTexture.Load(int3(currPixel, 0), int2(j, i));
            count++;
        }
    }
    downScaled /= float(count);
        
    avgLum = dot(downScaled, LUM_FACTOR);
    
    if (count)
    {
        gAverageLum[storeIndex] = avgLum;
    }
}

[numthreads(32, 1, 1)]
void DownScaleFirstPass(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 currPixel = uint2(dispatchThreadId.x * 4, dispatchThreadId.y * 4);
    uint storeIndex = dispatchThreadId.x + dispatchThreadId.y * gThreadOffsetW;
    DownScale4x4(currPixel, storeIndex);
}

[numthreads(32, 1, 1)]
void DownScaleFinalPass(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint currOffset = 1;
    
    for (int i = 0; i < gNumDonwScale; i++)
    {
        if (currOffset % dispatchThreadId.x == 0)
        {
            float avgLum = 0.0f;
            uint count = 0;
            for (int j = 0; j < 4; j++)
            {
                uint offsetIndex = dispatchThreadId.x + j * currOffset;
                
                if (offsetIndex < gLumBufferSize)
                {
                    avgLum += gAverageLum[offsetIndex];
                    count++;
                }
            }
            
            if(count)
            {
                gAverageLum[dispatchThreadId.x] = avgLum / count;
            }
        }
        else
        {
            break;
        }
        
        AllMemoryBarrierWithGroupSync();
        currOffset *= 4;
    }
}