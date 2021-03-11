#ifndef OREN_NAYAR_H_
#define OREN_NAYAR_H_

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../microfacet.hlsl"
#include "BSDFCommon.hlsl"

struct OrenNayar
{
    BxDFType type;
    Spectrum R;
    float A, B;
    
    Spectrum f(const float3 wo, const float3 wi)
    {
        float sinThetaI = SinTheta(wi);
        float sinThetaO = SinTheta(wo);
        // Compute cosine term of Oren-Nayar model
        float maxCos = 0;
        if (sinThetaI > 1e-4 && sinThetaO > 1e-4)
        {
            float sinPhiI = SinPhi(wi), cosPhiI = CosPhi(wi);
            float sinPhiO = SinPhi(wo), cosPhiO = CosPhi(wo);
            float dCos = cosPhiI * cosPhiO + sinPhiI * sinPhiO;
            maxCos = max((float) 0, dCos);
        }

        // Compute sine and tangent terms of Oren-Nayar model
        float sinAlpha, tanBeta;
        if (AbsCosTheta(wi) > AbsCosTheta(wo))
        {
            sinAlpha = sinThetaO;
            tanBeta = sinThetaI / AbsCosTheta(wi);
        }
        else
        {
            sinAlpha = sinThetaI;
            tanBeta = sinThetaO / AbsCosTheta(wo);
        }
        return R * M_1_PI * (A + B * maxCos * sinAlpha * tanBeta);
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

OrenNayar createOrenNayar(Spectrum R, float sigma, BxDFType type = BSDF_REFLECTION | BSDF_DIFFUSE)
{
    OrenNayar o;
    sigma = Radians(sigma);
    float sigma2 = sigma * sigma;
    o.type = type;
    o.R = R;
    o.A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
    o.B = 0.45f * sigma2 / (sigma2 + 0.09f);
    return o;
}

#endif