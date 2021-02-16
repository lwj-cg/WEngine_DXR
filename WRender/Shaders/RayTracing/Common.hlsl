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
	uint seed;
	uint depth;
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
	uint ObjPad2;
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

#endif