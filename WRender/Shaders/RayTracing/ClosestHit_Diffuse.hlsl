#include "Common.hlsl"
#include "PBR.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
#include "HitCommon.hlsl"
#include "Sampling.hlsl"

[shader("closesthit")]
void ClosestHit_Diffuse(inout RayPayload current_payload, Attributes attrib)
{
    // Some global configurations
    float refraction_index = 1.5f;
    float scene_epsilon = 0.001f;
    uint gNumLights = 1;
    
    // Calculate world_geometric_normal
    ObjectConstants objectData = gObjectBuffer[InstanceID()];
    uint vertId = 3 * PrimitiveIndex() + objectData.IndexOffset;
    uint vertOffset = objectData.VertexOffset;
    int texCoordOffset = objectData.TexCoordOffset;
    float3 v0 = gVertexBuffer[vertOffset + gIndexBuffer[vertId]].pos;
    float3 v1 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 1]].pos;
    float3 v2 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 2]].pos;
    float2 uv0, uv1, uv2;
    if (texCoordOffset >= 0)
    {
        uv0 = gTexCoordBuffer[texCoordOffset + gTexCoordIndexBuffer[vertId]].uv;
        uv1 = gTexCoordBuffer[texCoordOffset + gTexCoordIndexBuffer[vertId + 1]].uv;
        uv2 = gTexCoordBuffer[texCoordOffset + gTexCoordIndexBuffer[vertId + 2]].uv;
    }
    else
    {
        uv0 = float2(0, 0);
        uv1 = float2(1, 0);
        uv2 = float2(1, 1);
    }
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float2 uv = barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2;
    // Fetch Material Data
    uint matIdx = objectData.MatIdx;
    MaterialData matData = gMaterialBuffer[matIdx];

    float3 geometric_normal = normalize(cross(v1 - v0, v2 - v0));
    float4x4 objectToWorld = objectData.ObjectToWorld;
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) objectToWorld);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal;
    int normalMapIdx = matData.NormalMapIdx;
    if (normalMapIdx >= 0)
    {
        float3 shading_normal = gTextureMaps[normalMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb;
        ffnormal = faceforward(shading_normal, -ray_direction);
    }
    else
    {
        ffnormal = faceforward(world_geometric_normal, -ray_direction);
    }
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    current_payload.origin = hitpoint;
    
    float z1 = rnd(current_payload.seed);
    float z2 = rnd(current_payload.seed);
    float3 p;
    CosineSampleHemisphere(float2(z1, z2), p);
    Onb onb;
    create_onb(ffnormal, onb);
    inverse_transform_with_onb(p, onb);
    current_payload.direction = p;
    
    int diffuseMapIdx = matData.DiffuseMapIdx;
    //float3 diffuse_color = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamLinearWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 diffuse_color = matData.Albedo.rgb;
    
    if (any(matData.Emission))
    {
        current_payload.done = true;
        return;
    }
    
    // NOTE: f/pdf = 1 since we are perfectly importance sampling lambertian
    // with cosine density.
    current_payload.attenuation = current_payload.attenuation * diffuse_color;
    
    uint num_lights, stride;
    gLightBuffer.GetDimensions(num_lights, stride);
    //
    // Next event estimation (compute direct lighting).
    //
    float3 result = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < gNumLights; ++i)
    {
        // Choose random point on light
        ParallelogramLight light = gLightBuffer[i];
        const float z1 = rnd(current_payload.seed);
        const float z2 = rnd(current_payload.seed);
        const float3 light_pos = light.corner + light.v1 * z1 + light.v2 * z2;

        // Calculate properties of light sample (for area based pdf)
        const float Ldist = length(light_pos - hitpoint);
        const float3 L = normalize(light_pos - hitpoint);
        const float nDl = dot(ffnormal, L);
        const float LnDl = -dot(light.normal, L);

        // cast shadow ray
        //if (nDl > 0.0f)
        if (nDl > 0.0f && LnDl > 0.0f)
        {
            RayPayload_shadow shadow_payload;
            // ?
            shadow_payload.inShadow = 0;
            // Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
            RayDesc shadow_ray = make_Ray(hitpoint, L, scene_epsilon, Ldist - scene_epsilon);
            // Trace the ray (Hit group 2 : shadow ray, Miss 1 : shadow miss)
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 1, shadow_ray, shadow_payload);

            if (shadow_payload.inShadow != 0)
            {
                const float A = length(cross(light.v1, light.v2));
                // convert area based pdf to solid angle
                const float weight = nDl * LnDl * A / (M_PI * Ldist * Ldist);
                result += light.emission * weight * shadow_payload.inShadow;
            }
        }
    }
    
    current_payload.radiance = result;
    
}