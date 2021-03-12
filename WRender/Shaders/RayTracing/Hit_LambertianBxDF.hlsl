#include "Common.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
#include "HitCommon.hlsl"
#include "Sampling.hlsl"
#include "BxDF/BSDFCommon.hlsl"
#include "BxDF/LambertianReflection.hlsl"
#include "PBR.hlsl"
#include "Light.hlsl"
#include "Intersection.hlsl"

// Multiple Importance Sampling
float3 EstimateDirect(Interaction it, LambertianReflection lambertRefl, float2 uScattering, AreaLight light, float2 uLight, bool specular, Onb onb, Spectrum baseColor)
{
    float scene_epsilon = 0.001f;
    uint bsdfFlags = specular ? BSDF_ALL : BSDF_ALL & ~BSDF_SPECULAR;
    float3 Ld = (float3) 0.f;
    // Sample light source with multiple importance sampling
    float3 wiWorld;
    float3 wo = WorldToLocal(it.wo, onb);
    float lightPdf = 0, scatteringPdf = 0, Ldist;
    float3 Li = light.Sample_Li(it, uLight, wiWorld, lightPdf, Ldist);
    if (lightPdf > 0 && !isBlack(Li))
    {
        float3 wi = WorldToLocal(wiWorld, onb);
        float3 f = lambertRefl.f(wo, wi) * AbsCosTheta(wi);
        scatteringPdf = lambertRefl.Pdf(wo, wi);
        if (!isBlack(f))
        {
            // Cast Shadow Ray
            RayPayload_shadow shadow_payload;
            shadow_payload.inShadow = 1;
            RayDesc shadow_ray = make_Ray(it.p, wiWorld, scene_epsilon, Ldist - scene_epsilon);
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, shadow_ray, shadow_payload);
            if (shadow_payload.inShadow != 0)
            {
                float weight = PowerHeuristic(1, lightPdf, 1, scatteringPdf);
                Ld += f * Li * weight / lightPdf;
            }
        }

    }
    // Sample BSDF with multiple importance sampling
    float3 f;
    bool sampledSpecular = false;
    // Sample scattered direction for surface interactions
    BxDFType sampledType;
    float3 wi;
    f = lambertRefl.Sample_f(wo, wi, uScattering, scatteringPdf, sampledType);
    wiWorld = LocalToWorld(wi, onb);
    f *= AbsCosTheta(wi);
    if (!isBlack(f) && scatteringPdf > 0)
    {
        // Account for light contributions along sampled direction _wi_
        float weight = 1;

        lightPdf = light.Pdf_Li(it, wi);
        if (lightPdf == 0)
            return Ld;
        weight = PowerHeuristic(1, scatteringPdf, 1, lightPdf);
        
        // Add light contribution from material sampling
        RayDesc testRay = make_Ray(it.p, wiWorld);
        float tHit;
        Interaction isectLight;
        light.Intersect(testRay, tHit, isectLight);
        float Ldist = distance(isectLight.p, it.p);
        
        // Find intersection (Cast Shadow Ray)
        RayPayload_shadow shadow_payload;
        shadow_payload.inShadow = 1;
        float3 hit_point;
        RayDesc shadow_ray = make_Ray(it.p, wiWorld, scene_epsilon, Ldist - scene_epsilon);
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, shadow_ray, shadow_payload);
        float3 Li = (float3) 0.f;
        if (shadow_payload.inShadow != 0)
        {
            Li = light.L(it, -wi);
            Ld += f * Li * weight / scatteringPdf;
        }

    }
    return Ld;
}

[shader("closesthit")]
void ClosestHit_LambertianReflection(inout RayPayload current_payload, Attributes attrib)
{
    // Fetch UV
    ObjectConstants objectData = gObjectBuffer[InstanceID()];
    uint vertId = 3 * PrimitiveIndex() + objectData.IndexOffset;
    uint vertOffset = objectData.VertexOffset;
    int texCoordOffset = objectData.TexCoordOffset;
    int normalOffset = objectData.NormalOffset;
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
    
    // Calculate Normal
    float3 geometric_normal = normalize(cross(v1 - v0, v2 - v0));
    float4x4 objectToWorld = objectData.ObjectToWorld;
    float4x4 inverseTranspose = objectData.InverseTranspose;
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) inverseTranspose);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal;
    int normalMapIdx = matData.NormalMapIdx;
    if (normalMapIdx >= 0)
    {
        float3 shading_normal = gTextureMaps[normalMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        //ffnormal = faceforward(world_shading_normal, -ray_direction);
        ffnormal = world_shading_normal;
    }
    else
    {
        float3 normal0 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId]].normal;
        float3 normal1 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 1]].normal;
        float3 normal2 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 2]].normal;
        float3 shading_normal = barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        //ffnormal = faceforward(world_shading_normal, -ray_direction);
        ffnormal = world_shading_normal;
    }
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float uLight1 = rnd(current_payload.seed);
    float uLight2 = rnd(current_payload.seed);
    float uScattering1 = rnd(current_payload.seed);
    float uScattering2 = rnd(current_payload.seed);
    float2 uLight = float2(uLight1, uLight2);
    float2 uScattering = float2(uScattering1, uScattering2);
    
    
    // Fecth data needed by BxDF from MaterialData
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 baseColor = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 F0 = matData.F0;
    float3 emission = matData.Emission;
    
    // Construct SurfaceInteration
    Interaction it = createInteraction(hitpoint, -ray_direction, ffnormal, world_geometric_normal);
    
    // Construct Reflection Coord System
    Onb onb;
    create_onb(ffnormal, onb);
    
    // Construct Lambertian Reflection
    LambertianReflection lambertRefl = createLambertianReflection(baseColor);
    
    // Construct Light
    AreaLight light = createAreaLight(gLightBuffer[0]);
    
    // Estimate direct light
    float3 Ld = EstimateDirect(it, lambertRefl, uScattering, light, uLight, false, onb, baseColor);
    current_payload.radiance = Ld;

    // Sample BSDF to get new path direction
    float3 wo = WorldToLocal(it.wo, onb), wi;
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float pdf;
    BxDFType sampledType;
    float3 f = lambertRefl.Sample_f(wo, wi, u, pdf, sampledType);
    float3 wiWorld = LocalToWorld(wi, onb);
    current_payload.origin = hitpoint;
    current_payload.direction = wiWorld;
    current_payload.attenuation = f * AbsDot(wiWorld, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.bxdfType = sampledType;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}

