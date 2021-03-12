#ifndef BSDF_COMMON_H_
#define BSDF_COMMON_H_

typedef uint BxDFType;

static const BxDFType BSDF_REFLECTION = 1 << 0;
static const BxDFType BSDF_TRANSMISSION = 1 << 1;
static const BxDFType BSDF_DIFFUSE = 1 << 2;
static const BxDFType BSDF_GLOSSY = 1 << 3;
static const BxDFType BSDF_SPECULAR = 1 << 4;
static const BxDFType BSDF_ALL = BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR | BSDF_REFLECTION |
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

inline float3 SphericalDirection(float sinTheta, float cosTheta, float phi)
{
    return float3(sinTheta * cos(phi), sinTheta * sin(phi),
                    cosTheta);
}

bool MatchesFlags(BxDFType type, BxDFType t)
{
    return (type & t) == type;
}

#endif