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

inline float FrSchlick(float R0, float cosTheta)
{
    return lerp(R0, 1, SchlickWeight(cosTheta));
}

inline Spectrum FrSchlick(const Spectrum R0, float cosTheta)
{
    return lerp(R0, (Spectrum) (1.), SchlickWeight(cosTheta));
}

// For a dielectric, R(0) = (eta - 1)^2 / (eta + 1)^2, assuming we're
// coming from air..
inline float SchlickR0FromEta(float eta)
{
    return sqr(eta - 1) / sqr(eta + 1);
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

///////////////////////////////////////////////////////////////////////////
// FresnelDielectric

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

///////////////////////////////////////////////////////////////////////////
// DisneyFresnel

struct DisneyFresnel
{
    Spectrum R0;
    float metallic, eta;
    Spectrum Evaluate(float cosI)
    {
        return lerp((float3) (FrDielectric(cosI, 1, eta)), FrSchlick(R0, cosI), metallic);
    }
};

//DisneyFresnel createDisneyFresnel(Spectrum R0, float metallic, float eta)
//{
//    DisneyFresnel fr;
//    fr.R0 = R0;
//    fr.metallic = metallic;
//    fr.eta = eta;
//    return fr;
//}

///////////////////////////////////////////////////////////////////////////
// SchlickFresnel

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

///////////////////////////////////////////////////////////////////////////
// FresnelConductor

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

typedef uint FRESNEL_TYPE;
static const FRESNEL_TYPE FRESNEL_DIELECTRIC = 1 << 0;
static const FRESNEL_TYPE FRESNEL_CONDUCTOR = 1 << 1;
static const FRESNEL_TYPE DISNEY_FRESNEL = 1 << 2;
static const FRESNEL_TYPE FRESNEL_NOOP = 1 << 3;

///////////////////////////////////////////////////////////////////////////
// UberFresnel

struct UberFresnel
{
    // For FresnelDielectric
    float etaA;
    float etaB;
    // For FresnelConductor
    Spectrum etaI;
    Spectrum etaT;
    Spectrum k;
    // For DisneyFresnel (Reuse etaB & etaT)
    float metallic;
    // Type
    FRESNEL_TYPE type;
    
    Spectrum Evaluate(float cosThetaI)
    {
        if (type&FRESNEL_DIELECTRIC)
        {
            return FrDielectric(cosThetaI, etaA, etaB);
        }
        else if (type&FRESNEL_CONDUCTOR)
        {
            return FrConductor(cosThetaI, etaI, etaT, k);
        }
        else if (type&DISNEY_FRESNEL)
        {
            return lerp((Spectrum) (FrDielectric(cosThetaI, 1, etaB)), FrSchlick(etaT, cosThetaI), metallic);
        }
        else
        {
            return (Spectrum) (1.f);
        }
    }
};

UberFresnel createFresnel(float etaA, float etaB)
{
    UberFresnel fresnel;
    fresnel.etaA = etaA;
    fresnel.etaB = etaB;
    fresnel.type = FRESNEL_DIELECTRIC;
    return fresnel;
}

UberFresnel createFresnel(Spectrum etaI, Spectrum etaT, Spectrum k)
{
    UberFresnel fresnel;
    fresnel.etaI = etaI;
    fresnel.etaT = etaT;
    fresnel.k = k;
    fresnel.type = FRESNEL_CONDUCTOR;
    return fresnel;
}

UberFresnel createDisneyFresnel(Spectrum R0, float eta, float metallic)
{
    UberFresnel fresnel;
    fresnel.etaB = eta;
    fresnel.etaT = R0;
    fresnel.metallic = metallic;
    fresnel.type = DISNEY_FRESNEL;
    return fresnel;
}

UberFresnel createFresnel()
{
    UberFresnel fresnel;
    fresnel.type = FRESNEL_NOOP;
    return fresnel;
}

#endif