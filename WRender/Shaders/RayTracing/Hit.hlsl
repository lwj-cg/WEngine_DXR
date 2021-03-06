#include "Common.hlsl"
#include "PBR.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
#include "HitCommon.hlsl"
#include "Sampling.hlsl"

[shader("closesthit")]
void ClosestHit_Default(inout RayPayload current_payload, Attributes attrib)
{
    // Some global configurations
    uint max_depth = 16;
    uint camera_static_frames = 1;
    float refraction_index = 1.5f;
    float scene_epsilon = 0.001f;
    uint gNumLights = 1;
    
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
    float4x4 inverseTranspose = objectData.InverseTranspose;
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) inverseTranspose);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal;
    int normalMapIdx = matData.NormalMapIdx;
    if (normalMapIdx>=0)
    {
        float3 shading_normal = gTextureMaps[normalMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb;
        ffnormal = faceforward(shading_normal, -ray_direction);
    }
    else
    {
        ffnormal = faceforward(world_geometric_normal, -ray_direction);
    }
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float z1 = rnd(current_payload.seed);
    float z2 = rnd(current_payload.seed);
    
    if (any(matData.Emission))
    {
        current_payload.done = true;
        current_payload.radiance = current_payload.countEmitted ? matData.Emission : (float3) 0;
        return;
    }
    
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 albedo = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    
    // Initialize surface info
    SurfaceInfo IN;
    IN.baseColor = albedo;
    IN.transparent = matData.Transparent;
    IN.metallic = matData.Metallic;
    IN.smoothness = matData.Smoothness;
    IN.normal = ffnormal;
    
    float3 baseColor;
    int in_to_out = dot(ray_direction, world_geometric_normal) > 0;

    float3 a;
    float b;
    baseColor = DiffuseAndSpecularFromMetallic(albedo, matData.Metallic, a, b);
    b = current_payload.depth + 1;
    float cut_off = 1 / b;
    float3 attenuationFactor = float3(1, 1, 1);
    
    current_payload.origin = hitpoint;
    if (current_payload.depth < max_depth)
    {
        if (z2 < cut_off)
        {
            Onb onb;
            create_onb(ffnormal, onb);
            
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
            float3 p;

            if (z1 < refr_diff_refl.x)
            { 
                // Transparent
                float pd;
                float3 n;
                ImportanceSampleGGX(float2(z1, z2), matData.Smoothness, n, pd);
                inverse_transform_with_onb(n, onb);

                //if (refract(ray_direction, n, in_to_out ? refraction_index : 1.0f / refraction_index, p))
                //{
                //    attenuationFactor *= baseColor / max_diffuse;
                //    current_payload.direction = p;
                //}
                bool isRefract = refract(ray_direction, n, in_to_out ? refraction_index : 1.0f / refraction_index, p);
                if (isRefract && dot(ffnormal,p)<0)
                {
                    attenuationFactor *= baseColor / max_diffuse;
                    current_payload.direction = p;
                }
            }
            else if (z1 < refr_diff_refl.y)
            { 
                // Diffuse
                CosineSampleHemisphere(float2(z1, z2), p);
                inverse_transform_with_onb(p, onb);
                
                attenuationFactor *= PBR(IN, p, -ray_direction, 0) / max_diffuse;
                current_payload.direction = p;
            }
            else
            { 
                // Specular
                float pd;
                float3 n;
                ImportanceSampleGGX(float2(z1, z2), matData.Smoothness, n, pd);

                inverse_transform_with_onb(n, onb);
                p = reflect(ray_direction, n);

                if (dot(p, ffnormal) > 0)
                {
                    attenuationFactor *= PBR(IN, p, -ray_direction, 1) / pd / (1 - max_diffuse);
                    current_payload.direction = p;
                }
            }
            attenuationFactor *= sum_w * b;
            //current_payload.attenuation *= sum_w;
        }
    }
    current_payload.attenuation *= attenuationFactor;

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
            shadow_payload.inShadow = 1;
            // Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
            RayDesc shadow_ray = make_Ray(hitpoint, L, scene_epsilon, Ldist - scene_epsilon);
            // Trace the ray (Hit group 2 : shadow ray, Miss 1 : shadow miss)
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 1, shadow_ray, shadow_payload);

            if (shadow_payload.inShadow != 0)
            {
                const float A = length(cross(light.v1, light.v2));
                // convert area based pdf to solid angle
                const float weight = nDl * LnDl * A / (M_PI * Ldist * Ldist);
                float3 light_satu = light.emission * weight * shadow_payload.inShadow;
                //result += light_satu;
                result += PBR(IN, L, -ray_direction, 2) * light_satu;
                
            }
        }
    }
    //current_payload.radiance = result / attenuationFactor;
    current_payload.radiance = result;
    
}


