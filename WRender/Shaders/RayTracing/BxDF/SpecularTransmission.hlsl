#ifndef SPECULAR_TRANSMISSION_H_
#define SPECULAR_TRANSMISSION_H_

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "BSDFCommon.hlsl"

struct SpecularTransmission
{
    float3 T;
    float etaA;
    float etaB;
    FresnelDielectric fresnel;
    BxDFType bxdfType;
    float3 f(float3 wo, float3 wi)
    {
        return (float3) 0.f;
    }

    float Pdf(float3 wo, float3 wi)
    {
        return 0;
    }

    float3 Sample_f(float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Figure out which $\eta$ is incident and which is transmitted
        bool entering = CosTheta(wo) > 0;
        float etaI = entering ? etaA : etaB;
        float etaT = entering ? etaB : etaA;
    
        // Compute ray direction for specular transmission
        if (!refract(-wo, faceforward(float3(0, 0, 1), wo), etaI / etaT, wi))
            return 0;
        pdf = 1;
        float3 ft = T * ((float3) 1.f) - fresnel.Evaluate(CosTheta(wi));
        //// Account for non-symmetry with transmission to different medium
        //if (mode == TransportMode::Radiance)
        //    ft *= (etaI * etaI) / (etaT * etaT);
        return ft / AbsCosTheta(wi);
    }
};

#endif