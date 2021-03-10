#ifndef FRESNEL_H_
#define FRESNEL_H_

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
        return FrDielectric(cosThetaI, etaI, etaT);
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

#endif