#pragma once

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../microfacet.hlsl"
#include "BSDFCommon.hlsl"

struct FresnelSpecular
{
    BxDFType type; // BSDF_REFLECION | BSDF_TRANSMISSION | BSDF_SPECULAR
    float3 R;
    float3 T;
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
    
    float3 Sample_f(const float3 wo, out float3 wi, const float2 u, out float pdf)
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