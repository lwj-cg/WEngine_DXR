#include "Common.hlsl"

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float ramp = launchIndex.y / dims.y;
    if (payload.depth>0)
        payload.radiance = (float3) 0.0f;
    else
        payload.radiance = float3(0.0f, 0.2f, 0.7f - 0.3f * ramp);
}