#include "Common.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
#include "HitCommon.hlsl"
#include "Sampling.hlsl"
#include "BxDF/BSDFCommon.hlsl"
#include "PBR.hlsl"
#include "Light.hlsl"
#include "Intersection.hlsl"

float3 SpecularReflection_f(float3 wo, float3 wi)
{
    return (float3) 0.f;
}

float SpecularReflection_Pdf(float3 wo, float3 wi)
{
    return 0;
}

float3 SpecularReflection_Samplef(float3 wo, out float3 wi, const float2 u, out float pdf, float3 R, float3 F0)
{
    // Compute perfect specular reflection direction
    wi = float3(-wo.x, -wo.y, wo.z);
    pdf = 1;
    return FresnelTerm(F0, CosTheta(wi)) * R / AbsCosTheta(wi);
}

float3 SpecularTransmission_f(float3 wo, float3 wi)
{
    return (float3) 0.f;
}

float SpecularTransmission_Pdf(float3 wo, float3 wi)
{
    return 0;
}

float3 SpecularTransmission_Samplef(float3 wo /* from isect */, out float3 wi, const float2 u, out float pdf, float3 T, float etaA, float etaB, float3 F0)
{
    // Figure out which $\eta$ is incident and which is transmitted
    bool entering = CosTheta(wo) > 0;
    float etaI = entering ? etaA : etaB;
    float etaT = entering ? etaB : etaA;
    
    // Compute ray direction for specular transmission
    if (!refract(-wo, faceforward(float3(0, 0, 1), wo), etaI / etaT, wi))
        return 0;
    pdf = 1;
    float3 ft = T * ((float3) 1.f) - FresnelTerm(F0, CosTheta(wi));
    return ft / AbsCosTheta(wi);
}

float3 FresnelSpecular_f(float3 wo, float3 wi)
{
    return (float3) 0.f;
}

float FresnelSpecular_Pdf(float3 wo, float3 wi)
{
    return 0;
}

float3 FresnelSpecular_Samplef(const float3 wo, out float3 wi,
                               const float2 u, out float pdf,
                               float3 R, float3 T, float etaA, float etaB)
{
    float F = FrDielectric(CosTheta(wo), etaA, etaB);
    if (u[0] < F)
    {
        // Compute specular reflection for _FresnelSpecular_

        // Compute perfect specular reflection direction
        wi = float3(-wo.x, -wo.y, wo.z);
        pdf = F;
        return F * R / AbsCosTheta(wi);
    }
    else
    {
        // Compute specular transmission for _FresnelSpecular_

        // Figure out which $\eta$ is incident and which is transmitted
        bool entering = CosTheta(wo) > 0;
        float etaI = entering ? etaA : etaB;
        float etaT = entering ? etaB : etaA;

        // Compute ray direction for specular transmission
        if (!refract(wo, faceforward(float3(0, 0, 1), wo), etaI / etaT, wi))
            return 0;
        float3 ft = T * (1 - F);

        //// Account for non-symmetry with transmission to different medium
        //if (mode == TransportMode::Radiance)
        //    ft *= (etaI * etaI) / (etaT * etaT);
        pdf = 1 - F;
        return ft / AbsCosTheta(wi);
    }
}


[shader("closesthit")]
void ClosestHit_SpecularReflection(inout RayPayload current_payload, Attributes attrib)
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
        ffnormal = faceforward(world_shading_normal, -ray_direction);
        ffnormal = world_shading_normal;
    }
    else
    {
        float3 normal0 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId]].normal;
        float3 normal1 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 1]].normal;
        float3 normal2 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 2]].normal;
        float3 shading_normal = barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        ffnormal = faceforward(world_shading_normal, -ray_direction);
        ffnormal = world_shading_normal;
    }
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    
    // Fecth data needed by BxDF from MaterialData
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 baseColor = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 F0 = matData.F0;
    float3 emission = matData.Emission;
    
    // Transform wo to Local
    Onb onb;
    create_onb(ffnormal, onb);
    float3 wo = WorldToLocal(-ray_direction, onb);
    
    // Don't Estimate direct light for BxDF_SPECULAR
    current_payload.radiance = (float3) 0.f;

    // Sample BSDF to get new path direction
    float3 wi;
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float pdf;
    float3 f = SpecularReflection_Samplef(wo, wi, u, pdf, baseColor, F0);
    float3 wiWorld = LocalToWorld(wi, onb);
    current_payload.origin = hitpoint;
    current_payload.direction = wiWorld;
    current_payload.attenuation = f * AbsDot(wiWorld, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.specularBounce = true;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}

[shader("closesthit")]
void ClosestHit_SpecularTransmission(inout RayPayload current_payload, Attributes attrib)
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
        ffnormal = faceforward(world_shading_normal, -ray_direction);
    }
    else
    {
        float3 normal0 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId]].normal;
        float3 normal1 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 1]].normal;
        float3 normal2 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 2]].normal;
        float3 shading_normal = barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        ffnormal = faceforward(world_shading_normal, -ray_direction);
    }
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    
    // Fecth data needed by BxDF from MaterialData
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 baseColor = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 transColor = matData.TransColor.rgb;
    float3 F0 = matData.F0;
    float3 emission = matData.Emission;
    float etaA = 1;
    float etaB = 1.5;
    
    // Transform wo to Local
    Onb onb;
    create_onb(ffnormal, onb);
    float3 wo = WorldToLocal(-ray_direction, onb);

    // Don't Estimate direct light for BxDF_SPECULAR
    current_payload.radiance = (float3) 0.f;

    // Sample BSDF to get new path direction
    float3 wi;
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float pdf;
    float3 f = SpecularTransmission_Samplef(wo, wi, u, pdf, transColor, etaA, etaB, F0);
    float3 wiWorld = LocalToWorld(wi, onb);
    current_payload.origin = hitpoint;
    current_payload.direction = wiWorld;
    current_payload.attenuation = f * AbsDot(wiWorld, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.specularBounce = true;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}

[shader("closesthit")]
void ClosestHit_FresnelSpecular(inout RayPayload current_payload, Attributes attrib)
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
        ffnormal = faceforward(world_shading_normal, -ray_direction);
    }
    else
    {
        float3 normal0 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId]].normal;
        float3 normal1 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 1]].normal;
        float3 normal2 = gNormalBuffer[normalOffset + gNormalIndexBuffer[vertId + 2]].normal;
        float3 shading_normal = barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2;
        float3 world_shading_normal = mul(shading_normal, (float3x3) inverseTranspose);
        ffnormal = faceforward(world_shading_normal, -ray_direction);
    }
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    
    // Fecth data needed by BxDF from MaterialData
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 baseColor = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 transColor = matData.TransColor.rgb;
    float3 emission = matData.Emission;
    float etaA = 1.0f;
    float etaB = matData.RefraciveIndex;
    
    // Transform wo to Local
    Onb onb;
    create_onb(ffnormal, onb);
    float3 wo = WorldToLocal(-ray_direction, onb);
    
    // Don't Estimate direct light for BxDF_SPECULAR
    current_payload.radiance = (float3) 0.f;

    // Sample BSDF to get new path direction
    float3 wi;
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float pdf;
    float3 f = FresnelSpecular_Samplef(wo, wi, u, pdf, baseColor, transColor, etaA, etaB);
    float3 wiWorld = LocalToWorld(wi, onb);
    current_payload.origin = hitpoint;
    current_payload.direction = wiWorld;
    current_payload.attenuation = f * AbsDot(wiWorld, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.specularBounce = true;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}