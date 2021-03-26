#include "HitCommon.hlsl"
#include "BxDF/LambertianReflection.hlsl"
#include "BxDF/OrenNayar.hlsl"
#include "microfacet.hlsl"
#include "Helpers.hlsl"
#include "Light.hlsl"
#include "Random.hlsl"

struct MatteMaterial
{
    Spectrum Kd;
    float sigma;
    LambertianReflection lambertianRefl;
    OrenNayar orenNayarDiffuse;
    
    Spectrum f(float3 woWorld, float3 wiWorld, BxDFType flags, Onb onb, float3 ng)
    {
        float3 wo = WorldToLocal(woWorld, onb), wi = WorldToLocal(wiWorld, onb);
        if (wo.z == 0)
            return 0.;
        bool reflect = dot(wiWorld, ng) * dot(woWorld, ng) > 0;
        Spectrum f = (float3) 0.f;
        if (reflect)
        {
            if (sigma<=0.1)
            {
                f = lambertianRefl.f(wo, wi);
            }
            else
            {
                f = orenNayarDiffuse.f(wo, wi);
            }
        }
        return f;
    }
    
    float Pdf(float3 woWorld, float3 wiWorld, BxDFType flags, Onb onb)
    {
        float3 wo = WorldToLocal(woWorld, onb), wi = WorldToLocal(wiWorld, onb);
        if (wo.z == 0)
            return 0.;
        float pdf = 0.f;
        if(sigma<=0.1)
        {
            if (MatchesFlags(lambertianRefl.type, flags))
            {
                pdf = lambertianRefl.Pdf(wo, wi);
            }
        }
        else if (MatchesFlags(orenNayarDiffuse.type, flags))
        {
            pdf = orenNayarDiffuse.Pdf(wo, wi);
        }
        return pdf;
    }
    
    Spectrum Sample_f(float3 woWorld, out float3 wiWorld, float2 u, out float pdf, BxDFType type, inout BxDFType sampledType, Onb onb)
    {        
        float3 wi, wo = WorldToLocal(woWorld, onb);
        if (wo.z == 0)
            return (float3) 0.;
        pdf = 0;
        Spectrum f;
        if (sigma <= 0.1)
        {
            f = lambertianRefl.Sample_f(wo, wi, u, pdf, sampledType);
            sampledType = lambertianRefl.type;
        }
        else
        {
            f = orenNayarDiffuse.Sample_f(wo, wi, u, pdf, sampledType);
            sampledType = orenNayarDiffuse.type;
        }
        wiWorld = LocalToWorld(wi, onb);
        return f;
    }
};

MatteMaterial createMatteMaterial(Spectrum Kd, float sigma)
{
    MatteMaterial mat;
    mat.Kd = Kd;
    mat.sigma = sigma;
    mat.lambertianRefl = createLambertianReflection(Kd);
    mat.orenNayarDiffuse = createOrenNayar(Kd, sigma);
    return mat;
}

Spectrum EstimateDirect(Interaction it, MatteMaterial mat, float2 uScattering,
                      AreaLight light, float2 uLight,
                      bool specular, Onb onb, float scene_epsilon)
{
    BxDFType bsdfFlags = specular ? BSDF_ALL : BSDF_ALL & ~BSDF_SPECULAR;
    Spectrum Ld = (Spectrum) 0.f;
    // Sample light source with multiple importance sampling
    float3 wi;
    float lightPdf = 0, scatteringPdf = 0;
    float Ldist;
    float3 Li = light.Sample_Li(it, uLight, wi, lightPdf, Ldist);
    if (lightPdf > 0 && !isBlack(Li))
    {
        // Compute BSDF or phase function's value for light sample
        Spectrum f;
        f = mat.f(it.wo, wi, bsdfFlags, onb, it.ng) * AbsDot(wi, it.n);
        scatteringPdf = mat.Pdf(it.wo, wi, bsdfFlags, onb);
        if (!isBlack(f))
        {
            // Cast Shadow Ray
            RayPayload_shadow shadow_payload;
            shadow_payload.inShadow = 1;
            RayDesc shadow_ray = make_Ray(it.p, wi, scene_epsilon, Ldist - scene_epsilon);
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
    BxDFType sampledType;
    f = mat.Sample_f(it.wo, wi, uScattering, scatteringPdf, bsdfFlags, sampledType, onb);
    if (!isBlack(f) && scatteringPdf > 0)
    {
        // Account for light contributions along sampled direction _wi_
        float weight = 1;
        lightPdf = light.Pdf_Li(it, wi);
        if (lightPdf == 0)
            return Ld;
        weight = PowerHeuristic(1, scatteringPdf, 1, lightPdf);
        
        // Add light contribution from material sampling
        RayDesc testRay = make_Ray(it.p, wi);
        float tHit;
        Interaction isectLight;
        light.Intersect(testRay, tHit, isectLight);
        float Ldist = distance(isectLight.p, it.p);
        
        // Cast Shadow Ray
        RayPayload_shadow shadow_payload;
        shadow_payload.inShadow = 1;
        RayDesc shadow_ray = make_Ray(it.p, wi, scene_epsilon, Ldist - scene_epsilon);
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, shadow_ray, shadow_payload);
        Spectrum Li = (Spectrum) 0.f;
        if (shadow_payload.inShadow != 0)
        {
            Li = light.L(it, -wi);
            Ld += f * Li * weight / scatteringPdf;
        }
    }
    return Ld;
}

[shader("closesthit")]
void ClosestHit_MatteMaterial(inout RayPayload current_payload, Attributes attrib)
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
    
    // Calculate Tangent & Bitangent
    float4x4 inverseTranspose = objectData.InverseTranspose;
    float3 tangent, bitangent;
    caluculateTangentAndBitangent(v0, v1, v2, uv0, uv1, uv2, tangent, bitangent);
    float3 tangentW = mul(tangent, (float3x3) inverseTranspose);
    
    // Calculate Normal
    float3 geometric_normal = normalize(cross(v1 - v0, v2 - v0));
    float3 world_geometric_normal = mul(geometric_normal, (float3x3) inverseTranspose);
    world_geometric_normal = normalize(world_geometric_normal);
    float3 ray_direction = normalize(WorldRayDirection());
    float3 ffnormal;
    int normalMapIdx = matData.NormalMapIdx;
    if (normalMapIdx >= 0)
    {
        float3 shading_normal = gTextureMaps[normalMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb;
        float3 world_shading_normal = NormalSampleToWorldSpace(shading_normal, world_geometric_normal, tangentW);
        ffnormal = normalize(world_shading_normal);
    }
    else
    {
        float3 normal0 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId]].normal;
        float3 normal1 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 1]].normal;
        float3 normal2 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 2]].normal;
        float3 shading_normal = barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        ffnormal = normalize(world_shading_normal);
    }
    //ffnormal = world_geometric_normal;
    
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
    float3 emission = matData.Emission;
    float sigma = matData.Sigma;
    
    // Construct Material of Interact Surface
    MatteMaterial mat = createMatteMaterial(
        baseColor,
        sigma
    );
    
    // Construct SurfaceInteration
    Interaction it = createInteraction(hitpoint, -ray_direction, ffnormal, world_geometric_normal);
    
    // Construct Reflection Coord System
    Onb onb;
    create_onb(ffnormal, onb);
    
    // Construct Light
    AreaLight light = createAreaLight(gLightBuffer[0]);
    
    // Estimate direct light
    float scene_epsilon = 0.001f;
    Spectrum Ld = EstimateDirect(it, mat, uScattering, light, uLight, false, onb, scene_epsilon);
    current_payload.radiance = Ld;

    // Sample BSDF to get new path direction
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float3 wi;
    float pdf;
    BxDFType type = BSDF_ALL;
    BxDFType sampledType;
    float3 f = mat.Sample_f(it.wo, wi, u, pdf, type, sampledType, onb);
    current_payload.origin = it.p;
    current_payload.direction = wi;
    current_payload.attenuation = f * AbsDot(wi, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.bxdfType = sampledType;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}