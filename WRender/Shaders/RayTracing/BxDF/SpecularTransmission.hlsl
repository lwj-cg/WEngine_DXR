#ifndef SPECULAR_TRANSMISSION_H_
#define SPECULAR_TRANSMISSION_H_

#include "../Common.hlsl"
#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "BSDFCommon.hlsl"

struct SpecularTransmission
{
    BxDFType type;
    float3 T;
    float etaA;
    float etaB;
    FresnelDielectric fresnel;
    float3 f(float3 wo, float3 wi)
    {
        return (float3) 0.f;
    }

    float Pdf(float3 wo, float3 wi)
    {
        return 0;
    }

    Spectrum Sample_f(float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Figure out which $\eta$ is incident and which is transmitted
        bool entering = CosTheta(wo) > 0;
        float etaI = entering ? etaA : etaB;
        float etaT = entering ? etaB : etaA;
    
        pdf = 1;
        // Compute ray direction for specular transmission
        if (!refract(-wo, faceforward(float3(0, 0, 1), wo), etaI / etaT, wi))
            return 0;
        Spectrum ft = T * ((float3) 1.f - fresnel.Evaluate(CosTheta(wi)));
        //// Account for non-symmetry with transmission to different medium
        //if (mode == TransportMode::Radiance)
        //    ft *= (etaI * etaI) / (etaT * etaT);
        return ft / AbsCosTheta(wi);
    }
};

SpecularTransmission createSpecularTransmission(float3 T, float etaA, float etaB, FresnelDielectric fresnel, BxDFType type = BSDF_TRANSMISSION | BSDF_SPECULAR)
{
    SpecularTransmission specTrans;
    specTrans.type = type;
    specTrans.T = T;
    specTrans.etaA = etaA;
    specTrans.etaB = etaB;
    specTrans.fresnel = fresnel;
    return specTrans;
}

#endif