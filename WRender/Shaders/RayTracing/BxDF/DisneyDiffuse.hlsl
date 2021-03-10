#pragma once

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../fresnel.hlsl"
#include "BSDFCommon.hlsl"

struct DisneyDiffuse
{
    float3 R;
    float3 f(float3 wo, float3 wi)
    {
        float Fo = SchlickWeight(AbsCosTheta(wo)),
          Fi = SchlickWeight(AbsCosTheta(wi));
        
        // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing.
        // Burley 2015, eq (4).
        return R * M_1_PI * (1 - Fo / 2) * (1 - Fi / 2);
    }
    
    float Pdf(float3 wo, float3 wi)
    {
        return SameHemisphere(wo, wi) ? AbsCosTheta(wi) * M_1_PI : 0;
    }
    
    float3 Sample_f(float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Cosine-sample the hemisphere, flipping the direction if necessary
        CosineSampleHemisphere(u, wi);
        if (wo.z < 0)
            wi.z *= -1;
        pdf = Pdf(wo, wi);
        return f(wo, wi);
    }
};