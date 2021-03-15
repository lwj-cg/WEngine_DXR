#ifndef MICROFACET_REFLECTION_H_
#define MICROFACET_REFLECTION_H_

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../microfacet.hlsl"
#include "BSDFCommon.hlsl"

struct MicrofacetReflection
{
    BxDFType type; // BSDF_REFLECTION | BSDF_GLOSSY
    Spectrum R;
    TrowbridgeReitzDistribution distribution;
    UberFresnel fresnel;
    
    Spectrum f(float3 wo /* from isect */, float3 wi /* to light */)
    {
        float cosThetaO = AbsCosTheta(wo), cosThetaI = AbsCosTheta(wi);
        float3 wh = wi + wo;
        // Handle degenerate cases for microfacet reflection
        if (cosThetaI == 0 || cosThetaO == 0)
            return (float3) (0.);
        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
            return (float3) (0.);
        wh = normalize(wh);
        // For the Fresnel call, make sure that wh is in the same hemisphere
        // as the surface normal, so that TIR is handled correctly.
        Spectrum F = fresnel.Evaluate(dot(wi, faceforward(wh, float3(0, 0, 1))));
        return R * distribution.D(wh) * distribution.G(wo, wi) * F /
           (4 * cosThetaI * cosThetaO);
    }
    
    float Pdf(float3 wo, float3 wi)
    {
        if (!SameHemisphere(wo, wi))
            return 0;
        float3 wh = normalize(wo + wi);
        return distribution.Pdf(wo, wh) / (4 * dot(wo, wh));
    }
    
    Spectrum Sample_f(const float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Sample microfacet orientation $\wh$ and reflected direction $\wi$
        if (wo.z == 0)
            return 0.;
        float3 wh = distribution.Sample_wh(wo, u);
        if (dot(wo, wh) < 0)
            return 0.; // Should be rare
        wi = my_reflect(wo, wh);
        if (!SameHemisphere(wo, wi))
            return (Spectrum) (0.f);

        // Compute PDF of _wi_ for microfacet reflection
        pdf = distribution.Pdf(wo, wh) / (4 * dot(wo, wh));
        return f(wo, wi);
    }
};

MicrofacetReflection createMicrofacetReflection(Spectrum R, TrowbridgeReitzDistribution distribution, UberFresnel fresnel, BxDFType type = BSDF_REFLECTION | BSDF_GLOSSY)
{
    MicrofacetReflection microRefl;
    microRefl.type = type;
    microRefl.R = R;
    microRefl.distribution = distribution;
    microRefl.fresnel = fresnel;
    return microRefl;
}

//struct MetalMicrofacetReflection
//{
//    BxDFType type; // BSDF_REFLECTION | BSDF_GLOSSY
//    Spectrum R;
//    TrowbridgeReitzDistribution distribution;
//    FresnelConductor fresnel;
    
//    Spectrum f(float3 wo /* from isect */, float3 wi /* to light */)
//    {
//        float cosThetaO = AbsCosTheta(wo), cosThetaI = AbsCosTheta(wi);
//        float3 wh = wi + wo;
//        // Handle degenerate cases for microfacet reflection
//        if (cosThetaI == 0 || cosThetaO == 0)
//            return (float3) (0.);
//        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
//            return (float3) (0.);
//        wh = normalize(wh);
//        // For the Fresnel call, make sure that wh is in the same hemisphere
//        // as the surface normal, so that TIR is handled correctly.
//        Spectrum F = fresnel.Evaluate(dot(wi, faceforward(wh, float3(0, 0, 1))));
//        return R * distribution.D(wh) * distribution.G(wo, wi) * F /
//           (4 * cosThetaI * cosThetaO);
//    }
    
//    float Pdf(float3 wo, float3 wi)
//    {
//        if (!SameHemisphere(wo, wi))
//            return 0;
//        float3 wh = normalize(wo + wi);
//        return distribution.Pdf(wo, wh) / (4 * dot(wo, wh));
//    }
    
//    Spectrum Sample_f(const float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
//    {
//        // Sample microfacet orientation $\wh$ and reflected direction $\wi$
//        if (wo.z == 0)
//            return 0.;
//        float3 wh = distribution.Sample_wh(wo, u);
//        if (dot(wo, wh) < 0)
//            return 0.; // Should be rare
//        wi = my_reflect(wo, wh);
//        if (!SameHemisphere(wo, wi))
//            return (Spectrum) (0.f);

//        // Compute PDF of _wi_ for microfacet reflection
//        pdf = distribution.Pdf(wo, wh) / (4 * dot(wo, wh));
//        return f(wo, wi);
//    }
//};

//MetalMicrofacetReflection createMetalMicrofacetReflection(Spectrum R, TrowbridgeReitzDistribution distribution, FresnelConductor fresnel, BxDFType type = BSDF_REFLECTION | BSDF_GLOSSY)
//{
//    MetalMicrofacetReflection microRefl;
//    microRefl.type = type;
//    microRefl.R = R;
//    microRefl.distribution = distribution;
//    microRefl.fresnel = fresnel;
//    return microRefl;
//}

#endif