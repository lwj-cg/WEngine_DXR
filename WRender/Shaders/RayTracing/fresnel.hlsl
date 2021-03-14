#ifndef FRESNEL_H_
#define FRESNEL_H_
#include "Common.hlsl"

inline float sqr(float x)
{
    return x * x;
}

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
//
// The Schlick Fresnel approximation is:
//
// R = R(0) + (1 - R(0)) (1 - cos theta)^5,
//
// where R(0) is the reflectance at normal indicence.
inline float SchlickWeight(float cosTheta)
{
    float m = clamp(1 - cosTheta, 0, 1);
    return (m * m) * (m * m) * m;
}

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
Spectrum FrConductor(float cosThetaI, const Spectrum etai,
                     const Spectrum etat, const Spectrum k)
{
    cosThetaI = clamp(cosThetaI, -1, 1);
    Spectrum eta = etat / etai;
    Spectrum etak = k / etai;

    float cosThetaI2 = cosThetaI * cosThetaI;
    float sinThetaI2 = 1. - cosThetaI2;
    Spectrum eta2 = eta * eta;
    Spectrum etak2 = etak * etak;

    Spectrum t0 = eta2 - etak2 - sinThetaI2;
    Spectrum a2plusb2 = sqrt(t0 * t0 + 4 * eta2 * etak2);
    Spectrum t1 = a2plusb2 + cosThetaI2;
    Spectrum a = sqrt(0.5f * (a2plusb2 + t0));
    Spectrum t2 = (float) 2 * cosThetaI * a;
    Spectrum Rs = (t1 - t2) / (t1 + t2);

    Spectrum t3 = cosThetaI2 * a2plusb2 + sinThetaI2 * sinThetaI2;
    Spectrum t4 = t2 * sinThetaI2;
    Spectrum Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5 * (Rp + Rs);
}

inline float FrSchlick(float R0, float cosTheta)
{
    return lerp(R0, 1, SchlickWeight(cosTheta));
}

inline float3 FrSchlick(const float3 R0, float cosTheta)
{
    return lerp(R0, (float3) (1.), SchlickWeight(cosTheta));
}

float FrDielectric(float cosThetaI, float etaI, float etaT)
{
    cosThetaI = clamp(cosThetaI, -1, 1);
    // Potentially swap indices of refraction
    bool entering = cosThetaI > 0.f;
    if (!entering)
    {
        float tmp = etaI;
        etaI = etaT;
        etaT = tmp;
        cosThetaI = abs(cosThetaI);
    }

    // Compute _cosThetaT_ using Snell's law
    float sinThetaI = sqrt(max((float) 0, 1 - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;

    // Handle total internal reflection
    if (sinThetaT >= 1)
        return 1;
    float cosThetaT = sqrt(max((float) 0, 1 - sinThetaT * sinThetaT));
    float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
                  ((etaT * cosThetaI) + (etaI * cosThetaT));
    float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
                  ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2;
}

struct FresnelDielectric
{
    float etaI;
    float etaT;
    float3 Evaluate(float cosThetaI)
    {
        return (float3)  FrDielectric(cosThetaI, etaI, etaT);
    }
};

FresnelDielectric createFresnelDielectric(float etaI, float etaT)
{
    FresnelDielectric fr;
    fr.etaI = etaI;
    fr.etaT = etaT;
    return fr;
}

struct DisneyFresnel
{
    float3 R0;
    float metallic, eta;
    float3 Evaluate(float cosI)
    {
        return lerp((float3) (FrDielectric(cosI, 1, eta)), FrSchlick(R0, cosI), metallic);
    }
};

DisneyFresnel createDisneyFresnel(float3 R0, float metallic, float eta)
{
    DisneyFresnel fr;
    fr.R0 = R0;
    fr.metallic = metallic;
    fr.eta = eta;
    return fr;
}

struct SchlickFresnel
{
    float3 R0;
    float3 Evaluate(float cosI)
    {
        return FrSchlick(R0, cosI);
    }
};

SchlickFresnel createSchlickFresnel(float3 R0)
{
    SchlickFresnel fr;
    fr.R0 = R0;
    return fr;
}

struct FresnelConductor
{
    Spectrum etaI;
    Spectrum etaT;
    Spectrum k;
    Spectrum Evaluate(float cosThetaI)
    {
        return FrConductor(abs(cosThetaI), etaI, etaT, k);
    }
};

FresnelConductor createFresnelConductor(Spectrum etaI, Spectrum etaT, Spectrum k)
{
    FresnelConductor fr;
    fr.etaI = etaI;
    fr.etaT = etaT;
    fr.k = k;
    return fr;
}

#endif