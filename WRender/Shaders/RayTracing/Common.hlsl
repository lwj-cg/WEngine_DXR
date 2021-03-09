#ifndef COMMON_H_
#define COMMON_H_
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
    bool done;
    bool specularBounce;
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

#endif