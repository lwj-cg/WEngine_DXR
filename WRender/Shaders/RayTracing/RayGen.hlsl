#include "Common.hlsl"
#include "Random.hlsl"

// Constant data that varies per material.
cbuffer cbPass : register(b0)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float PassPad0;
	float4 gBackColor;
};

StructuredBuffer<ObjectConstants> gObjectBuffer : register(t0, space1);
StructuredBuffer<MaterialData> gMaterialBuffer : register(t0, space2);

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

[shader("raygeneration")]
void RayGen()
{
	// Global variables
	int gSqrtSamples = 4;
	int gMaxDepth = 8;

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);
	float2 inv_screen = 1.0f / dims * 2.0f;
	float2 jitter_scale = inv_screen / gSqrtSamples;
	uint samples_per_pixel = gSqrtSamples * gSqrtSamples;
	uint pixel_id = (launchIndex.y * dims.x + launchIndex.x) * (samples_per_pixel + 1);
	float2 pixel = launchIndex * inv_screen - 1.0f;
	float3 color_result = float3(0.0f, 0.0f, 0.0f);
	uint camera_static_frames = 1;
	uint seed = tea16(pixel_id, camera_static_frames);
	float z = rnd(seed);
	for (uint samples_index = 0; samples_index < samples_per_pixel;++samples_index)
	{
		uint x = samples_index % gSqrtSamples;
		uint y = samples_index / gSqrtSamples;
		float2 jitter = float2(x - z, y - z);
		float2 d = pixel + jitter * jitter_scale;
		// 观察空间到世界空间变换
		float3 origin = gEyePosW;
		// 投影空间变换到观察空间
		float3 direction = mul(float4(d, 0.0f, 1.0f), gInvProj).xyz;
		// 变换到世界空间 w=0只变换方向
		direction = mul(float4(direction, 0.0f), gInvView).xyz;
		direction = normalize(direction);
		// Initialize the ray payload
		RayPayload payload;
		payload.radiance = float3(0, 0, 0);
		payload.depth = 0;
		payload.seed = seed;
		// Define a ray, consisting of origin, direction, and the min-max distance values
		RayDesc ray;
		ray.Origin = origin;
		ray.Direction = direction;
		ray.TMin = 0;
		ray.TMax = 100000;

		// Trace the ray
		TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
		color_result += payload.radiance;
	}

	color_result = saturate(color_result / samples_per_pixel);
	uint2 outputIndex = uint2(launchIndex.x, DispatchRaysDimensions().y - launchIndex.y - 1);
	gOutput[outputIndex] = float4(color_result, 1.f);
}
