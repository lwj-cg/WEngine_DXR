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

struct AreaLight
{
    float3 corner;
    float3 v1;
    float3 v2;
    float3 normal;
    float3 emission;
    bool twoSided;
    
    float Area()
    {
        return length(cross(v1, v2));
    }
    
    bool Intersect(RayDesc ray, out float tHit, out Interaction isect)
    {
        float eps = 0.00001f;
        tHit = dot(corner - ray.Origin, normal) / (eps + dot(ray.Direction, normal));
        float3 hit_point = ray.Origin + tHit * ray.Direction;
        isect.p = hit_point;
        isect.n = normal;
        isect.ng = normal;
        float3 dp = hit_point - corner;
        float b1 = dot(dp, v1) / (LengthSquared(v1));
        float b2 = dot(dp, v2) / (LengthSquared(v2));
        if (tHit < ray.TMin)
            return false;
        return b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1;
    }
    
    // Pdf for sample Parallelogram
    float Pdf(const Interaction ref, const float3 wi)
    {
        RayDesc ray = make_Ray(ref.p, wi);
        float tHit;
        Interaction isectLight;
        if (!Intersect(ray, tHit, isectLight))
            return 0;
        
        // Convert light sample weight to solid angle measure
        float pdf = DistanceSquared(ref.p, isectLight.p) /
                (AbsDot(isectLight.n, -wi) * Area());
        if (isinf(pdf))
            pdf = 0.f;
        return pdf;
    }
    
    Interaction Sample(const Interaction ref, const float2 u, out float pdf)
    {
        // Sample Parallelogram
        Interaction it;
        it.p = corner + v1 * u[0] + v2 * u[1];
        it.n = normal;
        float Area = length(cross(v1, v2));
        pdf = 1 / Area;
        
        float3 wi = it.p - ref.p;
        wi = normalize(wi);
        pdf *= DistanceSquared(it.p, ref.p) / AbsDot(it.n, -wi);
        return it;
    }
    
    float3 L(Interaction intr, const float3 w /* from light */)
    {
        return (twoSided || dot(intr.n, w) > 0) ? emission : (float3) 0.f;
    }
    
    float Pdf_Li(const Interaction ref, const float3 wi)
    {
        return Pdf(ref, wi);
    }
    
    float3 Sample_Li(const Interaction ref, const float2 u, out float3 wi /* to light*/, out float pdf, out float Ldist)
    {
        Interaction pShape = Sample(ref, u, pdf);
        if (pdf == 0 || LengthSquared(pShape.p - ref.p) == 0)
        {
            pdf = 0;
            return 0.f;
        }
        Ldist = length(pShape.p - ref.p);
        wi = normalize(pShape.p - ref.p);
        return L(pShape, -wi);
    }
};

AreaLight createAreaLight(float3 corner, float3 v1, float3 v2, float3 normal, float3 emission, bool twoSided=true)
{
    AreaLight light;
    light.corner = corner;
    light.v1 = v1;
    light.v2 = v2;
    light.normal = normal;
    light.emission = emission;
    light.twoSided = twoSided;
    return light;
}

AreaLight createAreaLight(ParallelogramLight pLight, bool twoSided = true)
{
    AreaLight light;
    light.corner = pLight.corner;
    light.v1 = pLight.v1;
    light.v2 = pLight.v2;
    light.normal = pLight.normal;
    light.emission = pLight.emission;
    light.twoSided = twoSided;
    return light;
}

#endif