#include "Common.hlsl"
#include "PBR.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
    
// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

StructuredBuffer<ObjectConstants> gObjectBuffer : register(t0, space1);
StructuredBuffer<MaterialData> gMaterialBuffer : register(t0, space2);
StructuredBuffer<Vertex> gVertexBuffer : register(t0, space3);
StructuredBuffer<int> gIndexBuffer : register(t0, space4);
StructuredBuffer<ParallelogramLight> gLightBuffer : register(t0, space5);

[shader("closesthit")]
void ClosestHit(inout RayPayload current_payload, Attributes attrib)
{
    // Some global configurations
    uint max_depth = 8;
    uint camera_static_frames = 1;
    float refraction_index = 1.5f;
    
    ObjectConstants objectData = gObjectBuffer[InstanceID()];
    uint vertId = 3 * PrimitiveIndex() + objectData.IndexOffset;
    uint vertOffset = objectData.VertexOffset;
    float3 v0 = gVertexBuffer[vertOffset + gIndexBuffer[vertId]].pos;
    float3 v1 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 1]].pos;
    float3 v2 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 2]].pos;
    float3 geometric_normal = normalize(cross(v2 - v0, v1 - v0));
    float4x4 objectToWorld = objectData.ObjectToWorld;
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) objectToWorld);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal = faceforward(world_geometric_normal, -ray_direction);
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    current_payload.seed = tea16(current_payload.seed, camera_static_frames);
    float z1 = rnd(current_payload.seed);
    current_payload.seed = tea16(current_payload.seed, camera_static_frames);
    float z2 = rnd(current_payload.seed);
    
    uint matIdx = objectData.MatIdx;
    MaterialData matData = gMaterialBuffer[matIdx];
    
    float3 baseColor;
    int in_to_out = dot(ray_direction, world_geometric_normal) > 0;

    float3 a;
    float b;
    baseColor = DiffuseAndSpecularFromMetallic(matData.Albedo.rgb, matData.Metallic, a, b);
    b = current_payload.depth + 1;
    float cut_off = 1 / b;
    
    if (current_payload.depth < max_depth)
    {
        if (z2 < cut_off)
        {
            Onb onb;
            create_onb(ffnormal, onb);
            
            float3 p;
            RayPayload payload;
            RayDesc next_ray;
            payload.depth = current_payload.depth + 1;
            payload.seed = current_payload.seed;
            payload.radiance = float3(0, 0, 0);

            float max_diffuse = max(max(baseColor.x, baseColor.y), baseColor.z);

            float transparent = matData.Transparent;
            float diffuse_strength = 1.0f;
            float3 refr_diff_refl;
            refr_diff_refl.x = max_diffuse * transparent;
            refr_diff_refl.y = max_diffuse * (2 * in_to_out * transparent + 1 - in_to_out - transparent) * diffuse_strength;
            refr_diff_refl.z = (1 - max_diffuse);
            float sum_w = refr_diff_refl.x + refr_diff_refl.y + refr_diff_refl.z;
            refr_diff_refl /= sum_w;
            refr_diff_refl.y += refr_diff_refl.x;

            if (z1 < refr_diff_refl.x)
            { //透射部分
                float pd;
                float3 n;
                ImportanceSampleGGX(float2(z1, z2), matData.Smoothness, n, pd);
                inverse_transform_with_onb(n, onb);

                if (refract(ray_direction, n, in_to_out ? refraction_index : 1.0f / refraction_index, p))
                {
                    next_ray = make_Ray(hitpoint, p);

                    // Cast shadow ray
                    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 0, next_ray, payload);

                    current_payload.radiance = payload.radiance * baseColor / max_diffuse;
                }
            }
            else if (z1 < refr_diff_refl.y)
            { //漫射部分
                uniform_sample_hemisphere(z1, z2, p);
                inverse_transform_with_onb(p, onb);

                next_ray = make_Ray(hitpoint, p);

                // Trace the ray
                TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, next_ray, payload);
                
                current_payload.radiance = PBR(matData, p, payload.radiance, ffnormal, -ray_direction, 0) / max_diffuse;
            }
            else
            { // 反射部分
                float pd;
                float3 n;
                ImportanceSampleGGX(float2(z1, z2), matData.Smoothness, n, pd);

                inverse_transform_with_onb(n, onb);
                p = reflect(ray_direction, n);

                if (dot(p, ffnormal) > 0)
                {
                    next_ray = make_Ray(hitpoint, p);

                    // Trace the ray
                    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, next_ray, payload);

                    current_payload.radiance = PBR(matData, p, payload.radiance, ffnormal, -ray_direction, 1) / pd / (1 - max_diffuse);
                }
            }
            current_payload.radiance *= sum_w * b;
        }
    }

    current_payload.radiance += matData.Emission;
    //current_payload.radiance = world_geometric_normal;
}

[shader("closesthit")]
void ClosestHit_Diffuse(inout RayPayload current_payload, Attributes attrib)
{
    // Some global configurations
    uint max_depth = 8;
    uint camera_static_frames = 1;
    float refraction_index = 1.5f;
    float scene_epsilon = 0.001f;
    
    // Calculate world_geometric_normal
    ObjectConstants objectData = gObjectBuffer[InstanceID()];
    uint vertId = 3 * PrimitiveIndex() + objectData.IndexOffset;
    uint vertOffset = objectData.VertexOffset;
    float3 v0 = gVertexBuffer[vertOffset + gIndexBuffer[vertId]].pos;
    float3 v1 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 1]].pos;
    float3 v2 = gVertexBuffer[vertOffset + gIndexBuffer[vertId + 2]].pos;
    float3 geometric_normal = normalize(cross(v2 - v0, v1 - v0));
    float4x4 objectToWorld = objectData.ObjectToWorld;
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) objectToWorld);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal = faceforward(world_geometric_normal, -ray_direction);
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    current_payload.origin = hitpoint;
    
    float z1 = rnd(current_payload.seed);
    float z2 = rnd(current_payload.seed);
    float3 p;
    uniform_sample_hemisphere(z1, z2, p);
    Onb onb;
    create_onb(ffnormal, onb);
    inverse_transform_with_onb(p, onb);
    current_payload.direction = p;
    
    uint matIdx = objectData.MatIdx;
    MaterialData matData = gMaterialBuffer[matIdx];
    float3 diffuse_color = matData.Albedo.rgb;
    
    // NOTE: f/pdf = 1 since we are perfectly importance sampling lambertian
    // with cosine density.
    current_payload.attenuation = current_payload.attenuation * diffuse_color;
    
    //
    // Next event estimation (compute direct lighting).
    //
    uint num_lights, stride;
    gLightBuffer.GetDimensions(num_lights, stride);
    float3 result = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < num_lights; ++i)
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
        const float LnDl = dot(light.normal, L);

        // cast shadow ray
        //if (nDl > 0.0f && LnDl > 0.0f)
        if (nDl > 0.0f)
        {
            RayPayload_shadow shadow_payload;
            shadow_payload.inShadow = false;
            // Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
            RayDesc shadow_ray = make_Ray_Minmax(hitpoint, L, scene_epsilon, Ldist - scene_epsilon);
            // Trace the ray (Hit group 2 : shadow ray, Miss 1 : shadow miss)
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 1, shadow_ray, shadow_payload);

            if (!shadow_payload.inShadow)
            {
                const float A = length(cross(light.v1, light.v2));
                // convert area based pdf to solid angle
                const float weight = nDl * LnDl * A / (M_PI * Ldist * Ldist);
                result += light.emission * weight;
            }
        }
    }

    current_payload.radiance = result;
    //current_payload.radiance = float3(0,1,0);
}

