#ifndef BSDF_COMMON_H_
#define BSDF_COMMON_H_

typedef uint BxDFType;

static const uint BSDF_REFLECTION = 1 << 0;
static const uint BSDF_TRANSMISSION = 1 << 1;
static const uint BSDF_DIFFUSE = 1 << 2;
static const uint BSDF_GLOSSY = 1 << 3;
static const uint BSDF_SPECULAR = 1 << 4;
static const uint BSDF_ALL = BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR | BSDF_REFLECTION |
               BSDF_TRANSMISSION;

// BSDF Inline Functions
inline float CosTheta(float3 w)
{
    return w.z;
}
inline float Cos2Theta(float3 w)
{
    return w.z * w.z;
}
inline float AbsCosTheta(float3 w)
{
    return abs(w.z);
}
inline float Sin2Theta(float3 w)
{
    return max((float) 0, (float) 1 - Cos2Theta(w));
}

inline float SinTheta(float3 w)
{
    return sqrt(Sin2Theta(w));
}

inline float TanTheta(float3 w)
{
    return SinTheta(w) / CosTheta(w);
}

inline float Tan2Theta(float3 w)
{
    return Sin2Theta(w) / Cos2Theta(w);
}

inline float CosPhi(float3 w)
{
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 1 : clamp(w.x / sinTheta, -1, 1);
}

inline float SinPhi(float3 w)
{
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 0 : clamp(w.y / sinTheta, -1, 1);
}

inline float Cos2Phi(float3 w)
{
    return CosPhi(w) * CosPhi(w);
}

inline float Sin2Phi(float3 w)
{
    return SinPhi(w) * SinPhi(w);
}

float FrDielectric(float cosThetaI, float etaI, float etaT)
{
    cosThetaI = clamp(cosThetaI, -1, 1);
    // Potentially swap indices of refraction
    bool entering = cosThetaI > 0.f;
    if (!entering)
    {
        float tmp = etaI;
        etaI = etaT;
        etaT = tmp;
        cosThetaI = abs(cosThetaI);
    }

    // Compute _cosThetaT_ using Snell's law
    float sinThetaI = sqrt(max((float) 0, 1 - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;

    // Handle total internal reflection
    if (sinThetaT >= 1)
        return 1;
    float cosThetaT = sqrt(max((float) 0, 1 - sinThetaT * sinThetaT));
    float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
                  ((etaT * cosThetaI) + (etaI * cosThetaT));
    float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
                  ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2;
}

#endif