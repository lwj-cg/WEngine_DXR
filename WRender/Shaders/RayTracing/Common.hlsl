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
	uint seed;
	uint depth;
    bool done;
    bool countEmitted;
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
    uint MatIdx;
	uint VertexOffset;
	uint IndexOffset;
    int NormalOffset;
    int TexCoordOffset;
};

struct MaterialData
{
	float4 Albedo;
    float3 Emission;
    float Transparent;
    float Smoothness;
    float Metallic;
    float MetalPad0;
    float MetalPad1;
    int DiffuseMapIdx;
    int NormalMapIdx;
    int MaterialPad0;
    int MaterialPad1;
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

#endif