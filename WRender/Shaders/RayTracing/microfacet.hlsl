#ifndef MICROFACET_H_
#define MICROFACET_H_

#include "Common.hlsl"
#include "BxDF/BSDFCommon.hlsl"

inline float RoughnessToAlpha(float roughness)
{
    roughness = max(roughness, (float) 1e-3);
    float x = log(roughness);
    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x +
           0.000640711f * x * x * x * x;
}

static void TrowbridgeReitzSample11(float cosTheta, float U1, float U2,
                                    out float slope_x, out float slope_y)
{
    // special case (normal incidence)
    if (cosTheta > .9999)
    {
        float r = sqrt(U1 / (1 - U1));
        float phi = 6.28318530718 * U2;
        slope_x = r * cos(phi);
        slope_y = r * sin(phi);
        return;
    }

    float sinTheta =
        sqrt(max((float) 0, (float) 1 - cosTheta * cosTheta));
    float tanTheta = sinTheta / cosTheta;
    float a = 1 / tanTheta;
    float G1 = 2 / (1 + sqrt(1.f + 1.f / (a * a)));

    // sample slope_x
    float A = 2 * U1 / G1 - 1;
    float tmp = 1.f / (A * A - 1.f);
    if (tmp > 1e10)
        tmp = 1e10;
    float B = tanTheta;
    float D = sqrt(
        max(float(B * B * tmp * tmp - (A * A - B * B) * tmp), float(0)));
    float slope_x_1 = B * tmp - D;
    float slope_x_2 = B * tmp + D;
    slope_x = (A < 0 || slope_x_2 > 1.f / tanTheta) ? slope_x_1 : slope_x_2;

    // sample slope_y
    float S;
    if (U2 > 0.5f)
    {
        S = 1.f;
        U2 = 2.f * (U2 - .5f);
    }
    else
    {
        S = -1.f;
        U2 = 2.f * (.5f - U2);
    }
    float z =
        (U2 * (U2 * (U2 * 0.27385f - 0.73369f) + 0.46341f)) /
        (U2 * (U2 * (U2 * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
    slope_y = S * z * sqrt(1.f + slope_x * slope_x);

}

static float3 TrowbridgeReitzSample(float3 wi, float alpha_x,
                                      float alpha_y, float U1, float U2)
{
    // 1. stretch wi
    float3 wiStretched =
        normalize(float3(alpha_x * wi.x, alpha_y * wi.y, wi.z));

    // 2. simulate P22_{wi}(x_slope, y_slope, 1, 1)
    float slope_x, slope_y;
    TrowbridgeReitzSample11(CosTheta(wiStretched), U1, U2, slope_x, slope_y);

    // 3. rotate
    float tmp = CosPhi(wiStretched) * slope_x - SinPhi(wiStretched) * slope_y;
    slope_y = SinPhi(wiStretched) * slope_x + CosPhi(wiStretched) * slope_y;
    slope_x = tmp;

    // 4. unstretch
    slope_x = alpha_x * slope_x;
    slope_y = alpha_y * slope_y;

    // 5. compute normal
    return normalize(float3(-slope_x, -slope_y, 1.));
}

struct TrowbridgeReitzDistribution
{
    float alphax;
    float alphay;
    
    float D(float3 wh)
    {
        float tan2Theta = Tan2Theta(wh);
        if (isinf(tan2Theta))
            return 0.;
        const float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
        float e = (Cos2Phi(wh) / (alphax * alphax) + Sin2Phi(wh) / (alphay * alphay)) * tan2Theta;
        return 1 / (M_PI * alphax * alphay * cos4Theta * (1 + e) * (1 + e));
    }
    
    float Lambda(float3 w)
    {
        float absTanTheta = abs(TanTheta(w));
        if (isinf(absTanTheta))
            return 0.;
        // Compute _alpha_ for direction _w_
        float alpha =
        sqrt(Cos2Phi(w) * alphax * alphax + Sin2Phi(w) * alphay * alphay);
        float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);
        return (-1 + sqrt(1.f + alpha2Tan2Theta)) / 2;
    }
    
    float G1(float3 w)
    {
        //    if (Dot(w, wh) * CosTheta(w) < 0.) return 0.;
        return 1 / (1 + Lambda(w));
    }

    float G(float3 wo, float3 wi)
    {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }
    
    float3 Sample_wh(float3 wo, float2 u)
    {
        float3 wh;
        bool flip = wo.z < 0;
        wh = TrowbridgeReitzSample(flip ? -wo : wo, alphax, alphay, u[0], u[1]);
        if (flip)
            wh = -wh;
        return wh;
    }

    float Pdf(const float3 wo, const float3 wh)
    {
        return D(wh) * G1(wo) * AbsDot(wo, wh) / AbsCosTheta(wo);
    }
};

TrowbridgeReitzDistribution createTrowbridgeReitzDistribution(float alphax, float alphay)
{
    TrowbridgeReitzDistribution distrib;
    distrib.alphax = alphax;
    distrib.alphay = alphay;
    return distrib;
}

struct BeckmannDistribution
{
    float alphax;
    float alphay;
    
    float D(const float3 wh)
    {
        float tan2Theta = Tan2Theta(wh);
        if (isinf(tan2Theta))
            return 0.;
        float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
        return exp(-tan2Theta * (Cos2Phi(wh) / (alphax * alphax) +
                                  Sin2Phi(wh) / (alphay * alphay))) /
           (M_PI * alphax * alphay * cos4Theta);
    }
    
    float Lambda(float3 w)
    {
        float absTanTheta = abs(TanTheta(w));
        if (isinf(absTanTheta))
            return 0.;
        
        // Compute _alpha_ for direction _w_
        float alpha =
            sqrt(Cos2Phi(w) * alphax * alphax + Sin2Phi(w) * alphay * alphay);
        float a = 1 / (alpha * absTanTheta);
        if (a >= 1.6f)
            return 0;
        return (1 - 1.259f * a + 0.396f * a * a) / (3.535f * a + 2.181f * a * a);
    }
    
    float G1(float3 w)
    {
        //    if (Dot(w, wh) * CosTheta(w) < 0.) return 0.;
        return 1 / (1 + Lambda(w));
    }

    float G(float3 wo, float3 wi)
    {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }
    
    float3 Sample_wh(const float3 wo, const float2 u)
    {
        // Sample full distribution of normals for Beckmann distribution

        // Compute $\tan^2 \theta$ and $\phi$ for Beckmann distribution sample
        float tan2Theta, phi;
        if (alphax == alphay)
        {
            float logSample = log(1 - u[0]);
            tan2Theta = -alphax * alphax * logSample;
            phi = u[1] * 2 * M_PI;
        }
        else
        {
            // Compute _tan2Theta_ and _phi_ for anisotropic Beckmann
            // distribution
            float logSample = log(1 - u[0]);
            phi = atan(alphay / alphax *
                            tan(2 * M_PI * u[1] + 0.5f * M_PI));
            if (u[1] > 0.5f)
                phi += M_PI;
            float sinPhi = sin(phi), cosPhi = cos(phi);
            float alphax2 = alphax * alphax, alphay2 = alphay * alphay;
            tan2Theta = -logSample /
                        (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
        }

        // Map sampled Beckmann angles to normal direction _wh_
        float cosTheta = 1 / sqrt(1 + tan2Theta);
        float sinTheta = sqrt(max((float) 0, 1 - cosTheta * cosTheta));
        float3 wh = SphericalDirection(sinTheta, cosTheta, phi);
        if (!SameHemisphere(wo, wh))
            wh = -wh;
        return wh;
    }
    
    float Pdf(const float3 wo, const float3 wh)
    {
        return D(wh) * G1(wo) * AbsDot(wo, wh) / AbsCosTheta(wo);
    }
};

BeckmannDistribution createBeckmannDistribution(float alphax, float alphay)
{
    BeckmannDistribution distrib;
    distrib.alphax = max(0.001f, alphax);
    distrib.alphay = max(0.001f, alphay);
    return distrib;

}

#endif