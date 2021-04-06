#include "Common.hlsl"
#include "Random.hlsl"
#include "Helpers.hlsl"
#include "Samplers/lowdiscrepancy.hlsl"
#include "Samplers/halton.hlsl"

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
    float gSceneEpsilon;
    float4 gBackColor;
    uint gNumStaticFrame;
    uint gSqrtSamples;
    uint gMaxDepth;
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
    uint num_sqrt_samples = gSqrtSamples;
    uint max_depth = gMaxDepth;
    float scene_epsilon = gSceneEpsilon;
    int rr_begin_depth = 3;

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
    //uint2 launchIndex = DispatchRaysIndex().xy;
    //float2 dims = float2(DispatchRaysDimensions().xy);
    //float2 inv_screen = 1.0f / dims * 2.0f;
    //float2 jitter_scale = inv_screen / num_sqrt_samples;
    //uint samples_per_pixel = num_sqrt_samples * num_sqrt_samples;
    //uint pixel_id = (launchIndex.y * dims.x + launchIndex.x) * (samples_per_pixel + 1);
    //float2 pixel = launchIndex * inv_screen - 1.0f;
    //float3 color_result = float3(0.0f, 0.0f, 0.0f);
    //uint camera_static_frames = gNumStaticFrame;
    //uint seed = tea16(pixel_id, camera_static_frames);
    
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 inv_screen = 1.0f / dims;
    float2 jitter_scale = inv_screen;
    uint samples_per_pixel = num_sqrt_samples * num_sqrt_samples;
    float2 pixel = launchIndex * inv_screen;
    float3 color_result = float3(0.0f, 0.0f, 0.0f);
    uint camera_static_frames = gNumStaticFrame;
    
    HaltonSampler mySampler = createHaltonSampler(launchIndex);
    
    //float z = rnd(seed);
    for (uint samples_index = 0; samples_index < samples_per_pixel; ++samples_index)
    {
        //uint x = samples_index % num_sqrt_samples;
        //uint y = samples_index / num_sqrt_samples;
        //float2 jitter = float2(x - rnd(seed), y - rnd(seed));
        //float2 d = pixel + jitter * jitter_scale;
        
        mySampler.SetSampleNumber(camera_static_frames * samples_per_pixel + samples_index);
        uint seed = mySampler.intervalSampleIndex;
        float2 offset = mySampler.Get2D();
        float2 d = pixel + offset * inv_screen;
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
        payload.attenuation = float3(1, 1, 1);
        payload.dimension = 2;
        payload.seed = seed;
        payload.done = false;
        payload.bxdfType = BSDF_ALL;

		// Trace the ray (Hit group 1 : only diffuse)
		// Each iteration is a segment of the ray path.  The closest hit will
        // return new segments to be traced here.
        float3 L = float3(0, 0, 0);
        float3 beta = float3(1, 1, 1);
        bool specularBounce = false;
        for (int bounces = 0;; ++bounces)
        {
            RayDesc ray = make_Ray(origin, direction, scene_epsilon);
            // Trace the ray (Hit group 0 : default, Miss 0 : common miss)
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

            // Possibly add emitted light at intersection
            if (bounces == 0 || specularBounce)
            {
                // Add emitted light at path vertex or from the environment
                L += beta * payload.emission;
            }
            
            // Terminate path if ray escaped or _maxDepth_ was reached
            if (payload.done || bounces >= max_depth)
            {
                // Add emitted light at path vertex or from the environment
                L += beta * payload.emission;
                break;
            }

            float3 Ld = beta * payload.radiance;
            L += Ld;

            // Get new path direction
            beta *= payload.attenuation;
            origin = payload.origin;
            direction = payload.direction;
            specularBounce = (payload.bxdfType & BSDF_SPECULAR)!=0;
            
            // Russian roulette termination 
            float3 rrBeta = beta;
            if (bounces > rr_begin_depth)
            {
                float q = max(.05f, 1 - MaxComponentValue(rrBeta));
                if (mySampler.Get1D(payload.dimension) < q)
                    break;
                beta /= 1-q;
            }
        }

        color_result += L;
        //seed = payload.seed;
    }

    color_result /= samples_per_pixel;
    color_result = pow(saturate(color_result), 1 / 2.2f);
    uint2 outputIndex = uint2(launchIndex.x, DispatchRaysDimensions().y - launchIndex.y - 1);
    if (camera_static_frames>1)
    {
        float a = 1.0f / (float) camera_static_frames;
        float3 old_color = gOutput[outputIndex].xyz;
        gOutput[outputIndex] = float4(lerp(old_color, color_result, a), 1.0f);
    }
    else
    {
        gOutput[outputIndex] = float4(color_result, 1.f);
    }
}
