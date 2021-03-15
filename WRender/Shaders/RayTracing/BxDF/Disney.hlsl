#pragma once

#include "../fresnel.hlsl"
#include "../Helpers.hlsl"
#include "../Common.hlsl"
#include "../Sampling.hlsl"
#include "../fresnel.hlsl"
#include "BSDFCommon.hlsl"

struct DisneyDiffuse
{
    BxDFType type;
    Spectrum R;
    Spectrum f(float3 wo, float3 wi)
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
    
    Spectrum Sample_f(float3 wo, out float3 wi, const float2 u, out float pdf, inout BxDFType sampledType)
    {
        // Cosine-sample the hemisphere, flipping the direction if necessary
        CosineSampleHemisphere(u, wi);
        if (wo.z < 0)
            wi.z *= -1;
        pdf = Pdf(wo, wi);
        return f(wo, wi);
    }
};

DisneyDiffuse createDisneyDiffuse(Spectrum R, BxDFType type = BSDF_REFLECTION | BSDF_DIFFUSE)
{
    DisneyDiffuse disneyDiff;
    disneyDiff.type = type;
    disneyDiff.R = R;
    return disneyDiff;
}

///////////////////////////////////////////////////////////////////////////
// DisneyRetro

struct DisneyRetro
{
    BxDFType type;
    Spectrum R;
    float roughness;
    
    Spectrum f(float3 wo, float3 wi)
    {
        float3 wh = wi + wo;
        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
            return (Spectrum) (0.);
        wh = normalize(wh);
        float cosThetaD = dot(wi, wh);

        float Fo = SchlickWeight(AbsCosTheta(wo)),
          Fi = SchlickWeight(AbsCosTheta(wi));
        float Rr = 2 * roughness * cosThetaD * cosThetaD;

        // Burley 2015, eq (4).
        return R * M_1_PI * Rr * (Fo + Fi + Fo * Fi * (Rr - 1));
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

DisneyRetro createDisneyRetro(Spectrum R, float roughness, BxDFType type = BSDF_REFLECTION|BSDF_DIFFUSE)
{
    DisneyRetro disneyRetro;
    disneyRetro.type = type;
    disneyRetro.R = R;
    disneyRetro.roughness = roughness;
    return disneyRetro;
}

///////////////////////////////////////////////////////////////////////////
// DisneySheen

struct DisneySheen
{
    BxDFType type;
    Spectrum R;
    
    Spectrum f(const float3 wo, const float3 wi) 
    {
        float3 wh = wi + wo;
        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
            return (Spectrum) (0.);
        wh = normalize(wh);
        float cosThetaD = dot(wi, wh);

        return R * SchlickWeight(cosThetaD);
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

DisneySheen createDisneySheen(Spectrum R, BxDFType type=BSDF_REFLECTION|BSDF_DIFFUSE)
{
    DisneySheen disneySheen;
    disneySheen.type = type;
    disneySheen.R = R;
    return disneySheen;
}

///////////////////////////////////////////////////////////////////////////
// DisneyClearcoat

inline float GTR1(float cosTheta, float alpha)
{
    float alpha2 = alpha * alpha;
    return (alpha2 - 1) /
           (M_PI * log(alpha2) * (1 + (alpha2 - 1) * cosTheta * cosTheta));
}

// Smith masking/shadowing term.
inline float smithG_GGX(float cosTheta, float alpha)
{
    float alpha2 = alpha * alpha;
    float cosTheta2 = cosTheta * cosTheta;
    return 1 / (cosTheta + sqrt(alpha2 + cosTheta2 - alpha2 * cosTheta2));
}

struct DisneyClearcoat
{
    BxDFType type;
    float weight;
    float gloss;
    
    Spectrum f(const float3 wo, const float3 wi) 
    {
        float3 wh = wi + wo;
        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
            return (Spectrum) (0.);
        wh = normalize(wh);

        // Clearcoat has ior = 1.5 hardcoded -> F0 = 0.04. It then uses the
        // GTR1 distribution, which has even fatter tails than Trowbridge-Reitz
        // (which is GTR2).
        float Dr = GTR1(AbsCosTheta(wh), gloss);
        float Fr = FrSchlick(.04, dot(wo, wh));
        // The geometric term always based on alpha = 0.25.
        float Gr =
            smithG_GGX(AbsCosTheta(wo), .25) * smithG_GGX(AbsCosTheta(wi), .25);

        return (Spectrum) (weight * Gr * Fr * Dr / 4);
    }
    
    float Pdf(const float3 wo, const float3 wi) 
    {
        if (!SameHemisphere(wo, wi))
            return 0;

        float3 wh = wi + wo;
        if (wh.x == 0 && wh.y == 0 && wh.z == 0)
            return 0;
        wh = normalize(wh);

        // The sampling routine samples wh exactly from the GTR1 distribution.
        // Thus, the final value of the PDF is just the value of the
        // distribution for wh converted to a mesure with respect to the
        // surface normal.
        float Dr = GTR1(AbsCosTheta(wh), gloss);
        return Dr * AbsCosTheta(wh) / (4 * dot(wo, wh));
    }
    
    Spectrum Sample_f(const float3 wo, out float3 wi,
                    const float2 u, out float pdf,
                    inout BxDFType sampledType)
    {
        // TODO: double check all this: there still seem to be some very
        // occasional fireflies with clearcoat; presumably there is a bug
        // somewhere.
        if (wo.z == 0)
            return 0.;

        float alpha2 = gloss * gloss;
        float cosTheta = sqrt(
        max(float(0), (1 - pow(alpha2, 1 - u[0])) / (1 - alpha2)));
        float sinTheta = sqrt(max((float) 0, 1 - cosTheta * cosTheta));
        float phi = 2 * M_PI * u[1];
        float3 wh = SphericalDirection(sinTheta, cosTheta, phi);
        if (!SameHemisphere(wo, wh))
            wh = -wh;

        wi = Reflect(wo, wh);
        if (!SameHemisphere(wo,  wi))
            return (Spectrum) (0.f);

        pdf = Pdf(wo,  wi);
        return f(wo,  wi);
    }

};

DisneyClearcoat createDisneyClearcoat(float weight, float gloss, BxDFType type = BSDF_REFLECTION|BSDF_GLOSSY)
{
    DisneyClearcoat disneyClearcoat;
    disneyClearcoat.type = type;
    disneyClearcoat.weight = weight;
    disneyClearcoat.gloss = gloss;
    return disneyClearcoat;
}

