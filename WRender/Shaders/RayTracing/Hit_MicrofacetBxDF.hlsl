#include "Common.hlsl"
#include "Helpers.hlsl"
#include "Random.hlsl"
#include "HitCommon.hlsl"
#include "Sampling.hlsl"
#include "BSDFCommon.hlsl"
#include "PBR.hlsl"
#include "Light.hlsl"
#include "Intersection.hlsl"

inline float RoughnessToAlpha(float roughness)
{
    roughness = max(roughness, (float) 1e-3);
    float x = log(roughness);
    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x +
           0.000640711f * x * x * x * x;
}

// TrowbridgeReitzDistribution
float D(float3 wh, float alphax, float alphay)
{
    float tan2Theta = Tan2Theta(wh);
    if (isinf(tan2Theta))
        return 0.;
    const float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    float e =
        (Cos2Phi(wh) / (alphax * alphax) + Sin2Phi(wh) / (alphay * alphay)) *
        tan2Theta;
    return 1 / (M_PI * alphax * alphay * cos4Theta * (1 + e) * (1 + e));
}

float Lambda(float3 w, float alphax, float alphay) 
{
    float absTanTheta = abs(TanTheta(w));
    if (isinf(absTanTheta))
        return 0.;
    // Compute _alpha_ for direction _w_
    float alpha =
        sqrt(Cos2Phi(w) * alphax * alphax + Sin2Phi(w) * alphay * alphay);
    float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);
    return (-1 + sqrt(1.f + alpha2Tan2Theta)) / 2;
}

float G1(float3 w, float alphax, float alphay) {
    //    if (Dot(w, wh) * CosTheta(w) < 0.) return 0.;
    return 1 / (1 + Lambda(w, alphax, alphay));
}

float G(float3 wo, float3 wi, float alphax, float alphay) {
    return 1 / (1 + Lambda(wo, alphax, alphay) + Lambda(wi, alphax, alphay));
}

static void TrowbridgeReitzSample11(float cosTheta, float U1, float U2,
                                    out float slope_x, out float slope_y)
{
    // special case (normal incidence)
    if (cosTheta > .9999)
    {
        float r = sqrt(U1 / (1 - U1));
        float phi = 6.28318530718 * U2;
        slope_x = r * cos(phi);
        slope_y = r * sin(phi);
        return;
    }

    float sinTheta =
        sqrt(max((float) 0, (float) 1 - cosTheta * cosTheta));
    float tanTheta = sinTheta / cosTheta;
    float a = 1 / tanTheta;
    float G1 = 2 / (1 + sqrt(1.f + 1.f / (a * a)));

    // sample slope_x
    float A = 2 * U1 / G1 - 1;
    float tmp = 1.f / (A * A - 1.f);
    if (tmp > 1e10)
        tmp = 1e10;
    float B = tanTheta;
    float D = sqrt(
        max(float(B * B * tmp * tmp - (A * A - B * B) * tmp), float(0)));
    float slope_x_1 = B * tmp - D;
    float slope_x_2 = B * tmp + D;
    slope_x = (A < 0 || slope_x_2 > 1.f / tanTheta) ? slope_x_1 : slope_x_2;

    // sample slope_y
    float S;
    if (U2 > 0.5f)
    {
        S = 1.f;
        U2 = 2.f * (U2 - .5f);
    }
    else
    {
        S = -1.f;
        U2 = 2.f * (.5f - U2);
    }
    float z =
        (U2 * (U2 * (U2 * 0.27385f - 0.73369f) + 0.46341f)) /
        (U2 * (U2 * (U2 * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
    slope_y = S * z * sqrt(1.f +  slope_x *  slope_x);

}

static float3 TrowbridgeReitzSample(float3 wi, float alpha_x,
                                      float alpha_y, float U1, float U2)
{
    // 1. stretch wi
    float3 wiStretched =
        normalize(float3(alpha_x * wi.x, alpha_y * wi.y, wi.z));

    // 2. simulate P22_{wi}(x_slope, y_slope, 1, 1)
    float slope_x, slope_y;
    TrowbridgeReitzSample11(CosTheta(wiStretched), U1, U2,  slope_x,  slope_y);

    // 3. rotate
    float tmp = CosPhi(wiStretched) * slope_x - SinPhi(wiStretched) * slope_y;
    slope_y = SinPhi(wiStretched) * slope_x + CosPhi(wiStretched) * slope_y;
    slope_x = tmp;

    // 4. unstretch
    slope_x = alpha_x * slope_x;
    slope_y = alpha_y * slope_y;

    // 5. compute normal
    return normalize(float3(-slope_x, -slope_y, 1.));
}

float3 Sample_wh(float3 wo, float2 u, float alphax, float alphay)
{
    float3 wh;
    bool flip = wo.z < 0;
    wh = TrowbridgeReitzSample(flip ? -wo : wo, alphax, alphay, u[0], u[1]);
    if (flip)
        wh = -wh;
    return wh;
}

// Pdf of MicrofacetDistribution
float Pdf(float3 wo, float3 wh, float alphax, float alphay)
{
   return D(wh, alphax, alphay) * G1(wo, alphax, alphax) * AbsDot(wo, wh) / AbsCosTheta(wo);
}

float3 MicrofacetReflection_f(float3 wo /* from intr */, float3 wi /* to light */, float3 F0, 
    float alphax, float alphay, float3 R /*baseColor*/)
{
    float cosThetaO = AbsCosTheta(wo), cosThetaI = AbsCosTheta(wi);
    float3 wh = wi + wo;
    // Handle degenerate cases for microfacet reflection
    if (cosThetaI == 0 || cosThetaO == 0)
        return (float3) (0.);
    if (wh.x == 0 && wh.y == 0 && wh.z == 0)
        return (float3) (0.);
    wh = normalize(wh);
    // For the Fresnel call, make sure that wh is in the same hemisphere
    // as the surface normal, so that TIR is handled correctly.
    float3 F = FresnelTerm(F0, dot(wi, faceforward(wh, float3(0, 0, 1))));
    return R * D(wh, alphax, alphay) * G(wo, wi, alphax, alphay) * F /
           (4 * cosThetaI * cosThetaO);
}

float MicrofacetReflection_Pdf(float3 wo, float3 wi, float alphax, float alphay)
{
    if (!SameHemisphere(wo, wi)) return 0;
    float3 wh = normalize(wo + wi);
    return Pdf(wo, wh, alphax, alphay) / (4 * dot(wo, wh));

}

float3 MicrofacetReflection_Samplef(const float3 wo, out float3 wi,
                                    const float2 u, out float pdf,
                                    float3 F0, float alphax, float alphay, float3 R)
{
    // Sample microfacet orientation $\wh$ and reflected direction $\wi$
    if (wo.z == 0)
        return 0.;
    float3 wh = Sample_wh(wo, u, alphax, alphay);
    if (dot(wo, wh) < 0)
        return 0.; // Should be rare
    wi = reflect(wo, wh);
    if (!SameHemisphere(wo,  wi))
        return (float3) (0.f);

    // Compute PDF of _wi_ for microfacet reflection
    pdf = Pdf(wo, wh, alphax, alphax) / (4 * dot(wo, wh));
    return MicrofacetReflection_f(wo, wi, F0, alphax, alphay, R);
}

float3 MicrofacetTransmission_f(float3 wo, float3 wi, float alphax, float alphay, float etaA, float etaB, float3 T)
{
    if (SameHemisphere(wo, wi))
        return 0; // transmission only

    float cosThetaO = CosTheta(wo);
    float cosThetaI = CosTheta(wi);
    if (cosThetaI == 0 || cosThetaO == 0)
        return (float3) (0);

    // Compute $\wh$ from $\wo$ and $\wi$ for microfacet transmission
    float eta = CosTheta(wo) > 0 ? (etaB / etaA) : (etaA / etaB);
    float3 wh = normalize(wo + wi * eta);
    if (wh.z < 0)
        wh = -wh;

    // Same side?
    if (dot(wo, wh) * dot(wi, wh) > 0)
        return (float3) (0);

    float F = FrDielectric(dot(wo, wh), etaA, etaB);

    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
    int mode = 0; // 0 for TransportMode::Radiance, 1 for TransportMode::Importance
    float factor = (mode == 0) ? (1 / eta) : 1;

    return ((float3) (1.f) - F) * T *
           abs(D(wh, alphax, alphay) * G(wo, wi, alphax, alphay) * eta * eta *
                    AbsDot(wi, wh) * AbsDot(wo, wh) * factor * factor /
                    (cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));
}

float3 MicrofacetTransmission_Samplef(const float3 wo, out float3 wi,
                                    const float2 u, out float pdf,
                                    float alphax, float alphay, float etaA, float etaB, float3 T)
{
    if (wo.z == 0)
        return 0.;
    float3 wh = Sample_wh(wo, u, alphax, alphay);
    if (dot(wo, wh) < 0)
        return 0.; // Should be rare

    float eta = CosTheta(wo) > 0 ? (etaA / etaB) : (etaB / etaA);
    if (!refract(-wo, wh, eta, wi))
        return 0;
    pdf = Pdf(wo,  wi, alphax, alphay);
    return MicrofacetTransmission_f(wo, wi, alphax, alphax, etaA, etaB, T);
}

// Multiple Importance Sampling
float3 EstimateDirect(SurfaceInteraction it, float2 uScattering, ParallelogramLight light, float2 uLight, bool specular, Onb onb)
{
    float scene_epsilon = 0.001f;
    uint bsdfFlags = specular ? BSDF_ALL : BSDF_ALL & ~BSDF_SPECULAR;
    float3 Ld = (float3) 0.f;
    // Sample light source with multiple importance sampling
    float3 wiWorld;
    float3 wo = WorldToLocal(it.wo, onb);
    float lightPdf = 0, scatteringPdf = 0, Ldist;
    float3 Li = ParallelogramLight_SampleLi(light, it.p, uLight, wiWorld, lightPdf, Ldist);
    if (lightPdf > 0 && !isBlack(Li))
    {
        float3 wi = WorldToLocal(wiWorld, onb);
        float3 f = MicrofacetReflection_f(wo, wi, it.F0, it.alphax, it.alphay, it.baseColor) * AbsCosTheta(wi);
        scatteringPdf = MicrofacetReflection_Pdf(wo, wi, it.alphax, it.alphay);
        if (!isBlack(f))
        {
            // Cast Shadow Ray
            RayPayload_shadow shadow_payload;
            shadow_payload.inShadow = 1;
            RayDesc shadow_ray = make_Ray(it.p, wiWorld, scene_epsilon, Ldist - scene_epsilon);
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 1, shadow_ray, shadow_payload);
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
    f = MicrofacetReflection_Samplef(wo, wi, uScattering, scatteringPdf, it.F0, it.alphax, it.alphay, it.baseColor);
    wiWorld = LocalToWorld(wi, onb);
    f *= AbsCosTheta(wi);
    if (!isBlack(f) && scatteringPdf > 0)
    {
        // Account for light contributions along sampled direction _wi_
        float weight = 1;

        lightPdf = ParallelogramLight_PdfLi(light, it.p, wiWorld);
        if (lightPdf == 0)
            return Ld;
        weight = PowerHeuristic(1, scatteringPdf, 1, lightPdf);
        
        // Find intersection (Cast Shadow Ray)
        RayPayload_shadow shadow_payload;
        shadow_payload.inShadow = 1;
        float3 hit_point;
        float tHit, Ldist;
        RayDesc isect_ray = make_Ray(it.p, wiWorld);
        bool isIntersect = Parallelogram_Intersect(isect_ray, light.corner, light.normal, light.v1, light.v2, tHit, hit_point, Ldist);
        RayDesc shadow_ray = make_Ray(it.p, wiWorld, scene_epsilon, Ldist - scene_epsilon);
        
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 1, shadow_ray, shadow_payload);
        float3 Li = (float3) 0.f;
        if (shadow_payload.inShadow != 0)
        {
            Li = ParallelogramLight_L(light, -wiWorld);
            Ld += f * Li * weight / scatteringPdf;
        }

    }
    return Ld;
}

[shader("closesthit")]
void ClosestHit_MicrofacetReflection(inout RayPayload current_payload, Attributes attrib)
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
    float alpha = RoughnessToAlpha(1 - matData.Smoothness);
    
    // Construct SurfaceInteration
    SurfaceInteraction it;
    it.p = hitpoint;
    it.wo = -ray_direction;
    it.n = ffnormal;
    it.baseColor = baseColor;
    it.alphax = alpha;
    it.alphay = alpha;
    it.F0 = F0;
    
    // Construct Reflection Coord System
    Onb onb;
    create_onb(ffnormal, onb);
    
    // Estimate direct light
    float3 Ld = EstimateDirect(it, uScattering, gLightBuffer[0], uLight, false, onb);
    current_payload.radiance = Ld;

    // Sample BSDF to get new path direction
    float3 wo = WorldToLocal(it.wo, onb), wi;
    float u1 = rnd(current_payload.seed);
    float u2 = rnd(current_payload.seed);
    float2 u = float2(u1, u2);
    float pdf;
    float3 f = MicrofacetReflection_Samplef(wo, wi, u, pdf, it.F0, it.alphax, it.alphay, it.baseColor);
    float3 wiWorld = LocalToWorld(wi, onb);
    current_payload.origin = hitpoint;
    current_payload.direction = wiWorld;
    current_payload.attenuation = f * AbsDot(wiWorld, ffnormal) / pdf;
    current_payload.emission = emission;
    current_payload.specularBounce = false;

    if (isBlack(f) || pdf == 0.f)
        current_payload.done = true;

}