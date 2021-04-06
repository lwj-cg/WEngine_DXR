#ifndef HALTON_H_
#define HALTON_H_
#include "lowdiscrepancy.hlsl"

static const int permArraySize = 3682913;
static const int kMaxResolution = 128;
static int baseScales0 = 128;
static int baseScales1 = 243;
static int sampleStride = 31104;
static int baseExponents0 = 7;
static int baseExponents1 = 5;
static int multInverse0 = 59;
static int multInverse1 = 131; 

struct HaltonSampler
{
    int2 pixelForOffset;
    int offsetForCurrentPixel;
    int2 currentPixel;
    int currentPixelSampleIndex;
    int intervalSampleIndex;
    int dimension;
    
    void initializeOffsetForCurrentPixel()
    {
        offsetForCurrentPixel = 0;
        int2 pm = int2(currentPixel[0] % kMaxResolution, currentPixel[1] % kMaxResolution);
        int dimOffset = InverseRadicalInverse(2, pm[0], baseExponents0);
        offsetForCurrentPixel += dimOffset * baseScales1 * multInverse0;
        dimOffset = InverseRadicalInverse(3, pm[0], baseExponents1);
        offsetForCurrentPixel += dimOffset * baseScales0 * multInverse1;
        offsetForCurrentPixel %= sampleStride;
    }
    
    int GetIndexForSample(int sampleNum)
    {
        return offsetForCurrentPixel + sampleNum * sampleStride;
    }
    
    uint pPermForDimension(int dim)
    {
        return PrimeSums[dim];
    }
    
    float SampleDimension0(int index)
    {
        return RadicalInverse(0, index >> baseExponents0);
    }
    
    float SampleDimension1(int index)
    {
        return RadicalInverse(1, index / baseScales1);
    }
    
    float SampleDimension(int index, int dim)
    {
        return ScrambledRadicalInverse(dim, index, pPermForDimension(dim));
    }
    
    void StartNextSample()
    {
        dimension = 0;
        intervalSampleIndex = GetIndexForSample(currentPixelSampleIndex + 1);
    }
    
    void SetSampleNumber(int sampleNum)
    {
        dimension = 0;
        intervalSampleIndex = GetIndexForSample(sampleNum);
    }
    
    float Get1D()
    {
        return SampleDimension(intervalSampleIndex, dimension++);
    }
    
    float Get1D(inout int dimension)
    {
        float p = SampleDimension(intervalSampleIndex, dimension);
        dimension++;
        return p;
    }
    
    float2 Get2D()
    {
        float2 p = float2(SampleDimension(intervalSampleIndex, dimension),
                          SampleDimension(intervalSampleIndex, dimension + 1));
        dimension += 2;
        return p;
    }
    
    float2 Get2D(inout int dimension)
    {
        float2 p = float2(SampleDimension(intervalSampleIndex, dimension),
                          SampleDimension(intervalSampleIndex, dimension + 1));
        dimension += 2;
        return p;
    }
};

// create halton sampler with current pixel id
HaltonSampler createHaltonSampler(int2 currentPixel)
{
    HaltonSampler haltonSampler;
    haltonSampler.currentPixel = currentPixel;
    haltonSampler.dimension = 0;
    haltonSampler.intervalSampleIndex = 0;
    haltonSampler.currentPixelSampleIndex = 0;
    return haltonSampler;
}

// create halton sampler directly with sampleIndex
HaltonSampler createHaltonSampler(int intervalSampleIndex)
{
    HaltonSampler haltonSampler;
    haltonSampler.dimension = 0;
    haltonSampler.offsetForCurrentPixel = 0;
    haltonSampler.currentPixelSampleIndex = 0;
    haltonSampler.intervalSampleIndex = intervalSampleIndex;
    return haltonSampler;
}

#endif