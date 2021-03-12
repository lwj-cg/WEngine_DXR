#ifndef HELP_H_
#define HELP_H_

// Some useful help functions
#include "Common.hlsl"

// Force n facefoward to v
inline float3 faceforward(float3 n, float3 v)
{
    return dot(n, v) > .0f ? n : n * -1;
}

// Local coordinate system of normal space
struct Onb
{
    float3 u;
    float3 v;
    float3 w;
};

// Create ONB from normal.  Resulting W is parallel to normal
void create_onb(float3 n, out Onb onb)
{
    onb.w = normalize(n);
    onb.u = cross(onb.w, float3(0, 1, 0));
    if (abs(onb.u.x) < 0.001f && abs(onb.u.y) < 0.001f && abs(onb.u.z) < 0.001f)
        onb.u = cross(onb.w, float3(1, 0, 0));
    onb.u = normalize(onb.u);
    onb.v = cross(onb.w, onb.u);
}

float3 LocalToWorld(float3 v, Onb onb)
{
    return v.x * onb.u + v.y * onb.v + v.z * onb.w;
}

float3 WorldToLocal(float3 v, Onb onb)
{
    float3 ss = onb.u;
    float3 ts = onb.v;
    float3 ns = onb.w;
    return float3(dot(ss, v), dot(ts, v), dot(ns, v));
}

RayDesc make_Ray(float3 origin, float3 direction, float tmin=0.001f, float tmax=100000)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = tmin;
    ray.TMax = tmax;
    return ray;
}

// reflect l with n
float3 reflect(float3 wo, float3 n)
{
    return -wo + 2 * dot(wo, n) * n;
}

// refract i with n
bool refract(float3 i /* from isect */, float3 n, float eta, out float3 t)
{
    // eta is n1 / n2
    float cosi = dot(i, n);
    float cost2 = 1.0f - eta * eta * (1.0f - cosi * cosi);
    if (cost2 <= 0)
        return false;
    t = eta * -i + ((eta * cosi - sqrt(abs(cost2))) * n);
    t = t * (float3) (cost2 > 0);
    return true;
}

inline bool isBlack(float3 L)
{
    return all(L <= (float3) 0.0f);
}

inline float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
    float f = nf * fPdf, g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}

inline float Radians(float deg)
{
    return (M_PI / 180.f) * deg;
}

#endif