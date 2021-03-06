#ifndef SAMPLING_H_
#define SAMPLING_H_

#define M_E        2.71828182845904523536   // e
#define M_LOG2E    1.44269504088896340736   // log2(e)
#define M_LOG10E   0.434294481903251827651  // log10(e)
#define M_LN2      0.693147180559945309417  // ln(2)
#define M_LN10     2.30258509299404568402   // ln(10)
#define M_PI       3.14159265358979323846   // pi
#define M_PI_2     1.57079632679489661923   // pi/2
#define M_PI_4     0.785398163397448309616  // pi/4
#define M_1_PI     0.318309886183790671538  // 1/pi
#define M_2_PI     0.636619772367581343076  // 2/pi
#define M_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#define M_SQRT2    1.41421356237309504880   // sqrt(2)
#define M_SQRT1_2  0.707106781186547524401  // 1/sqrt(2)

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