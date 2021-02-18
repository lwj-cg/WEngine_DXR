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

void inverse_transform_with_onb(inout float3 n, Onb onb)
{
    n = (n.z * onb.w + n.x * onb.u + n.y * onb.v);
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
float3 reflect(float3 l, float3 n)
{
    return l - 2 * dot(l, n) * n;
}

// refract i with n
bool refract(float3 i, float3 n, float eta, out float3 t)
{
    // eta is n1 / n2
    float cosi = dot(-i, n);
    float cost2 = 1.0f - eta * eta * (1.0f - cosi * cosi);
    t = eta * i + ((eta * cosi - sqrt(abs(cost2))) * n);
    t = t * (float3) (cost2 > 0);
    return cost2 > 0;
}

#endif