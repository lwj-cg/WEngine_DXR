#ifndef LAMBERTION_Transmission_H_
#define LAMBERTION_Transmission_H_

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "BSDFCommon.hlsl"

struct LambertianTransmission
{
    float3 T;
    float3 f(float3 wo, float3 wi)
    {
        return T * M_1_PI;
    }
    
    float Pdf(float3 wo, float3 wi)
    {
        return !SameHemisphere(wo, wi) ? AbsCosTheta(wi) * M_1_PI : 0;
    }
    
    float3 Sample_f(float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Cosine-sample the hemisphere, flipping the direction if necessary
        CosineSampleHemisphere(u, wi);
        if (wo.z > 0)
            wi.z *= -1;
        pdf = Pdf(wo, wi);
        return f(wo, wi);
    }
};

#endif