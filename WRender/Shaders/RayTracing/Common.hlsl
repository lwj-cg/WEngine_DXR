#ifndef COMMON_H_
#define COMMON_H_

#include "fresnel.hlsl"
#include "BxDF/BSDFCommon.hlsl"

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

// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct RayPayload
{
    float3 radiance;
    float3 origin;
    float3 direction;
    float3 attenuation;
    float3 emission;
	uint seed;
	uint depth;
    BxDFType bxdfType;
    bool done;
};

struct RayPayload_shadow
{
    float inShadow;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

struct ObjectConstants
{
    float4x4 ObjectToWorld;
    float4x4 InverseTranspose;
    uint MatIdx;
	uint VertexOffset;
	uint IndexOffset;
    int NormalOffset;
    int TexCoordOffset;
};

struct MaterialData
{
	float4 Albedo;
    float4 TransColor;
    float3 Emission;
    float3 F0;
    float Transparent;
    float Smoothness;
    float Metallic;
    float RefraciveIndex;
    float Sigma;
    int DiffuseMapIdx;
    int NormalMapIdx;
};

struct ParallelogramLight
{
    float3 corner;
    float3 v1;
    float3 v2;
    float3 normal;
    float3 emission;
    float lightPad0;
};

struct Vertex
{
    float3 pos;
};

struct Normal
{
    float3 normal;
};

struct TexCoord
{
    float2 uv;
};

// Surface Info
struct SurfaceInfo
{
    float3 baseColor;
    float transparent;
    float metallic;
    float smoothness;
    float3 normal;
    float3 p;
};

struct SurfaceInteraction
{
    float alphax;
    float alphay;
    float etaA;
    float etaB;
    float3 F0;
    float3 baseColor;
    float3 transColor;
    float3 p;
    float3 wo; /* from isect */
    float3 n;
};

struct Interaction
{
    float3 p;
    float3 wo;
    float3 n; // shading normal
    float3 ng; // geometric normal
};

Interaction createInteraction(float3 p, float3 wo, float3 n, float3 ng)
{
    Interaction it;
    it.p = p;
    it.wo = wo;
    it.n = n;
    it.ng = ng;
    return it;
}

typedef float3 Spectrum;

inline float AbsDot(float3 v1, float3 v2)
{
    return abs(dot(v1, v2));
}

inline bool SameHemisphere(float3 w, float3 wp)
{
    return w.z * wp.z > 0;
}

inline float MaxComponentValue(float3 v)
{
    return max(max(v.x, v.y), v.z);

}

inline float LengthSquared(float3 v)
{
    return length(v) * length(v);
}

inline float DistanceSquared(float3 v1, float3 v2)
{
    return LengthSquared(v1 - v2);

}

#endif