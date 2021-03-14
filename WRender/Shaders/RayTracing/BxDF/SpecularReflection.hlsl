#ifndef SPECULAR_REFLECTION_H_
#define SPECULAR_REFLECTION_H_

#include "../Common.hlsl"
#include "../fresnel.hlsl"
#include "BSDFCommon.hlsl"

struct SpecularReflection
{
    BxDFType type; // BSDF_REFLECTION | BSDF_SPECULAR
    Spectrum R;
    UberFresnel fresnel;
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
        // Compute perfect specular reflection direction
        wi = float3(-wo.x, -wo.y, wo.z);
        Spectrum F = fresnel.Evaluate(CosTheta(wi));
        pdf = 1;
        return F * R / AbsCosTheta(wi);
    }
};

SpecularReflection createSpecularReflection(float3 R, UberFresnel fresnel, BxDFType type = BSDF_REFLECTION|BSDF_SPECULAR)
{
    SpecularReflection SpecRefl;
    SpecRefl.type = type;
    SpecRefl.R = R;
    SpecRefl.fresnel = fresnel;
    return SpecRefl;
}

#endif