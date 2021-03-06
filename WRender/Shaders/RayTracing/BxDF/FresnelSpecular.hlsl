#ifndef FRESNEL_SPECULAR_H_
#define FRESNEL_SPECULAR_H_

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../microfacet.hlsl"
#include "BSDFCommon.hlsl"

struct FresnelSpecular
{
    BxDFType type; // BSDF_REFLECION | BSDF_TRANSMISSION | BSDF_SPECULAR
    Spectrum R;
    Spectrum T;
    float etaA;
    float etaB;
    
    float3 f(float3 wo, float3 wi)
    {
        return (float3) 0.f;
    }

    float Pdf(float3 wo, float3 wi)
    {
        return 0;
    }
    
    float3 Sample_f(const float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
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
};

FresnelSpecular createFresnelSpecular(float3 R, float3 T, float etaA, float etaB, BxDFType type = BSDF_REFLECTION|BSDF_TRANSMISSION|BSDF_SPECULAR)
{
    FresnelSpecular frSpec;
    frSpec.type = type;
    frSpec.R = R;
    frSpec.T = T;
    frSpec.etaA = etaA;
    frSpec.etaB = etaB;
    return frSpec;
}

#endif