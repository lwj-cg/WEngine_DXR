#include "Common.hlsl"

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

TextureCube gCubeMap : register(t1);

[shader("miss")]
void Miss(inout RayPayload payload)
{
    //payload.radiance = float3(0.0f, 0.0f, 0.0f);
    //payload.emission = float3(0.0f, 0.0f, 0.0f);
    float3 ray_direction = WorldRayDirection();
    payload.radiance = gCubeMap.SampleLevel(gsamLinearWrap, normalize(ray_direction), 0).rgb;
    payload.emission = gCubeMap.SampleLevel(gsamLinearWrap, normalize(ray_direction), 0).rgb;
    payload.done = true;
}

[shader("miss")]
void Miss_Shadow(inout RayPayload_shadow payload)
{
    payload.inShadow = 1;
}