#ifndef LIGHT_H_
#define LIGHT_H_
#include "Common.hlsl"
#include "Helpers.hlsl"
#include "Intersection.hlsl"

float2 UniformSampleParallelogram(const float2 u)
{
    return u;
}

SurfaceInteraction Parallelogram_Sample(ParallelogramLight light, const float2 u)
{
    float2 b = UniformSampleParallelogram(u);
    SurfaceInteraction it;
    it.p = light.corner + light.v1 * b[0] + light.v2 * b[1];
    it.n = light.normal;
    return it;
}

float ParallelogramLight_PdfLi(ParallelogramLight light, const float3 ref, const float3 wi /* to light*/)
{
    RayDesc ray = make_Ray(ref, wi);
    float tHit;
    float3 hit_point;
    float Ldist;
    if (!Parallelogram_Intersect(ray, light.corner, light.normal, light.v1, light.v2, tHit, hit_point, Ldist))
        return 0;
    const float Area = length(cross(light.v1, light.v2));
    float pdf = Ldist * Ldist / (AbsDot(light.normal, -wi) * Area);
    return pdf;
}

float3 ParallelogramLight_L(ParallelogramLight light, const float3 w)
{
    return dot(light.normal, w) > 0.f ? light.emission : (float3) 0.f;
}

float3 ParallelogramLight_SampleLi(ParallelogramLight light, const float3 ref, const float2 u, out float3 wi /* to light*/, out float pdf, out float Ldist)
{
    SurfaceInteraction pShape = Parallelogram_Sample(light, u);
    const float A = length(cross(light.v1, light.v2));
    Ldist = length(pShape.p - ref);
    wi = normalize(pShape.p - ref);
    pdf = ParallelogramLight_PdfLi(light, ref, wi);
    return ParallelogramLight_L(light, -wi);
}

#endif