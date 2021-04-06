#ifndef LOWDISCREPANCY_H_
#define LOWDISCREPANCY_H_
#include "lowdiscrepancyconsts.hlsl"

StructuredBuffer<uint> RadicalInversePermutations : register(t0, space100);

inline uint InverseRadicalInverse(int base, uint inverse, int nDigits)
{
    uint index = 0;
    for (int i = 0; i < nDigits; ++i)
    {
        uint digit = inverse % base;
        inverse /= base;
        index = index * base + digit;
    }
    return index;
}

static float RadicalInverseSpecialized(int base, uint a)
{
    const float invBase = (float) 1 / (float) base;
    uint reversedDigits = 0;
    float invBaseN = 1;
    while (a)
    {
        uint next = a / base;
        uint digit = a - next * base;
        reversedDigits = reversedDigits * base + digit;
        invBaseN *= invBase;
        a = next;
    }
    return min(reversedDigits * invBaseN, OneMinusEpsilon);
}

inline uint ReverseBits32(uint n)
{
    n = (n << 16) | (n >> 16);
    n = ((n & 0x00ff00ff) << 8) | ((n & 0xff00ff00) >> 8);
    n = ((n & 0x0f0f0f0f) << 4) | ((n & 0xf0f0f0f0) >> 4);
    n = ((n & 0x33333333) << 2) | ((n & 0xcccccccc) >> 2);
    n = ((n & 0x55555555) << 1) | ((n & 0xaaaaaaaa) >> 1);
    return n;
}

float RadicalInverse(int baseIndex, uint a)
{
    switch (baseIndex)
    {
        case 0:
            return ReverseBits32(a);
        default:
            return RadicalInverseSpecialized(Primes[baseIndex], a);
    }
    return RadicalInverseSpecialized(Primes[baseIndex], a);
}

static float
ScrambledRadicalInverseSpecialized(int base, const uint pPerm, uint a)
{
    const float invBase = (float) 1 / (float) base;
    uint reversedDigits = 0;
    float invBaseN = 1;
    while (a)
    {
        uint next = a / base;
        uint digit = a - next * base;
        reversedDigits = reversedDigits * base + RadicalInversePermutations[pPerm+digit];
        invBaseN *= invBase;
        a = next;
    }
    return min(
        invBaseN * (reversedDigits + invBase * RadicalInversePermutations[pPerm] / (1 - invBase)),
        OneMinusEpsilon);
}

float ScrambledRadicalInverse(int baseIndex, uint a, uint pPerm)
{
    return ScrambledRadicalInverseSpecialized(Primes[baseIndex], pPerm, a);

}

#endif