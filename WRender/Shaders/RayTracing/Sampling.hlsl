#ifndef SAMPLING_H_
#define SAMPLING_H_

#include "Common.hlsl"

inline void UniformSampleHemisphere(const float x, const float y, out float3 p)
{
    float Phi = 2 * M_PI * x;
    float CosTheta = y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    p.x = SinTheta * cos(Phi);
    p.y = SinTheta * sin(Phi);
    p.z = CosTheta;
}

inline float2 ConcentricSampleDisk(const float2 u)
{
    // Map uniform random numbers to $[-1,1]^2$
    float2 uOffset = 2.f * u - float2(1, 1);

    // Handle degeneracy at the origin
    if (uOffset.x == 0 && uOffset.y == 0)
        return float2(0, 0);

    // Apply concentric mapping to point
    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y))
    {
        r = uOffset.x;
        theta = M_PI_4 * (uOffset.y / uOffset.x);
    }
    else
    {
        r = uOffset.y;
        theta = M_PI_2 - M_PI_4 * (uOffset.x / uOffset.y);
    }
    return r * float2(cos(theta), sin(theta));
}

inline void CosineSampleHemisphere(const float2 u, out float3 p)
{
    float2 d = ConcentricSampleDisk(u);
    float z = sqrt(max(0, 1 - d.x * d.x - d.y * d.y));
    p = float3(d.x, d.y, z);
}

#endif