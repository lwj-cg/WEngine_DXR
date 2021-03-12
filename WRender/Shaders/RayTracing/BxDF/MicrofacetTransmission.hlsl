#pragma once

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../microfacet.hlsl"
#include "BSDFCommon.hlsl"

struct MicrofacetTransmission
{
    BxDFType type; // BSDF_TRANSMISSION | BSDF_GLOSSY
    Spectrum T;
    float etaA;
    float etaB;
    TrowbridgeReitzDistribution distribution;
    FresnelDielectric fresnel;
    
    float3 f(const float3 wo, const float3 wi)
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

        float3 F = fresnel.Evaluate(dot(wo, wh));

        float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
        float factor = 1;
        //float factor = (mode == TransportMode::Radiance) ? (1 / eta) : 1;

        return ((Spectrum) (1.f) - F) * T *
           abs(distribution.D(wh) * distribution.G(wo, wi) * eta * eta *
                    AbsDot(wi, wh) * AbsDot(wo, wh) * factor * factor /
                    (cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));
    }
    
    float Pdf(const float3 wo, const float3 wi) 
    {
        if (SameHemisphere(wo, wi))
            return 0;
        // Compute $\wh$ from $\wo$ and $\wi$ for microfacet transmission
        float eta = CosTheta(wo) > 0 ? (etaB / etaA) : (etaA / etaB);
        float3 wh = normalize(wo + wi * eta);

        if (dot(wo, wh) * dot(wi, wh) > 0)
            return 0;

        // Compute change of variables _dwh\_dwi_ for microfacet transmission
        float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
        float dwh_dwi =
            abs((eta * eta * dot(wi, wh)) / (sqrtDenom * sqrtDenom));
        return distribution.Pdf(wo, wh) * dwh_dwi;
    }
    
    float3 Sample_f(const float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Sample microfacet orientation $\wh$ and reflected direction $\wi$
        if (wo.z == 0)
            return 0.;
        float3 wh = distribution.Sample_wh(wo, u);
        if (dot(wo, wh) < 0) return 0.; // Should be rare
        
        float eta = CosTheta(wo) > 0 ? (etaA / etaB) : (etaB / etaA);
        if (!refract(wo, (float3) wh, eta, wi)) return 0;
        pdf = Pdf(wo, wi);
        return f(wo, wi);
    }
};

MicrofacetTransmission createMicrofacetTransmission(Spectrum T, float etaA, float etaB, 
    TrowbridgeReitzDistribution distribution, FresnelDielectric fresnel, BxDFType type = BSDF_TRANSMISSION | BSDF_GLOSSY)
{
    MicrofacetTransmission microTrans;
    microTrans.type = type;
    microTrans.T = T;
    microTrans.etaA = etaA;
    microTrans.etaB = etaB;
    microTrans.distribution = distribution;
    microTrans.fresnel = fresnel;
    return microTrans;
}