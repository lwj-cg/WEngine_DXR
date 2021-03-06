#include "HitCommon.hlsl"
#include "BxDF/BSDFCommon.hlsl"
#include "BxDF/FresnelSpecular.hlsl"
#include "BxDF/SpecularReflection.hlsl"
#include "BxDF/SpecularTransmission.hlsl"
#include "microfacet.hlsl"
#include "Helpers.hlsl"
#include "Light.hlsl"
#include "Random.hlsl"
#include "Samplers/halton.hlsl"

struct GlassSpecularMaterial
{
    SpecularReflection specRefl;
    SpecularTransmission specTrans;
    
    Spectrum f(float3 woWorld, float3 wiWorld, BxDFType flags, Onb onb, float3 ng)
    {
        return (Spectrum) 0.f;
    }
    
    float Pdf(float3 woWorld, float3 wiWorld, BxDFType flags, Onb onb)
    {
        return 0.f;
    }
    
    Spectrum Sample_f(float3 woWorld, out float3 wiWorld, float2 u, out float pdf, BxDFType type, inout BxDFType sampledType, Onb onb)
    {
        int matchingComps = 2;
        int comp = min(floor(u[0] * matchingComps), matchingComps - 1);
        // Remap _BxDF_ sample _u_ to $[0,1)^2$
        float2 uRemapped = float2(min(u[0] * matchingComps - comp, 0.999999), u[1]);
        
        // Sample chosen _BxDF_
        float3 wi, wo = WorldToLocal(woWorld, onb);
        //if (wo.z == 0)
        //    return (float3) 0.;
        pdf = 0;
        float3 f;
        if (comp == 0)
        {
            f = specRefl.Sample_f(wo, wi, uRemapped, pdf, sampledType);
            sampledType = specRefl.type;
        }
        else
        {
            f = specTrans.Sample_f(wo, wi, uRemapped, pdf, sampledType);
            sampledType = specTrans.type;
        }
        pdf /= matchingComps;
        wiWorld = LocalToWorld(wi, onb);
        return f;
    }
};

GlassSpecularMaterial createGlassSpecularMaterial(float3 R, float3 T, float eta)
{
    GlassSpecularMaterial mat;
    UberFresnel uFresnel = createFresnel(1, eta);
    FresnelDielectric fresnel = createFresnelDielectric(1, eta);
    
    mat.specRefl = createSpecularReflection(R, uFresnel);
    mat.specTrans = createSpecularTransmission(T, 1, eta, fresnel);
    
    return mat;
}

[shader("closesthit")]
void ClosestHit_GlassSpecularMaterial(inout RayPayload current_payload, Attributes attrib)
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
    
    float3 hitpoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // Fecth data needed by BxDF from MaterialData
    int diffuseMapIdx = matData.DiffuseMapIdx;
    float3 R = diffuseMapIdx >= 0 ? gTextureMaps[diffuseMapIdx].SampleLevel(gsamAnisotropicWrap, uv, 0).rgb : matData.Albedo.rgb;
    float3 T = matData.TransColor.rgb;
    float eta = matData.RefraciveIndex;
    float3 emission = matData.Emission;
    
    // Construct Material of Interact Surface
    GlassSpecularMaterial mat = createGlassSpecularMaterial(
        R,
        T,
        eta
    );
    
    // Construct SurfaceInteration
    Interaction it = createInteraction(hitpoint, -ray_direction, ffnormal, world_geometric_normal);
    
    // Construct Reflection Coord System
    Onb onb;
    create_onb(ffnormal, onb);
    
    // Don't Estimate direct light for BxDF_SPECULAR
    current_payload.radiance = (float3) 0.f;

    // Sample BSDF to get new path direction
    HaltonSampler mySampler = createHaltonSampler(current_payload.seed);
    float2 u = mySampler.Get2D(current_payload.dimension);
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
