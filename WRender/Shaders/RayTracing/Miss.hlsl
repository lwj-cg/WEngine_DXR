#include "Common.hlsl"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float ramp = launchIndex.y / dims.y;
    //payload.radiance = float3(0.0f, 0.2f, 0.7f - 0.3f * ramp);
    payload.radiance = float3(0.0f, 0.0f, 0.0f);
    payload.done = true;
}

[shader("miss")]
void Miss_Shadow(inout RayPayload_shadow payload)
{
    payload.inShadow = 1;
}