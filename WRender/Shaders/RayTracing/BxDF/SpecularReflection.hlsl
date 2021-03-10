#ifndef SPECULAR_REFLECTION_H_
#define SPECULAR_REFLECTION_H_

#include "../PBR.hlsl"
#include "../fresnel.hlsl"
#include "BSDFCommon.hlsl"

struct SpecularReflection
{
    BxDFType bxdfType; // BSDF_REFLECTION | BSDF_SPECULAR
    float3 R;
    FresnelDielectric fresnel;
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
        pdf = 1;
        return fresnel.Evaluate(CosTheta(wi)) * R / AbsCosTheta(wi);
    }
};

#endif