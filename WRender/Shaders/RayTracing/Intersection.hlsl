#ifndef INTERSECTION_H_
#define INTERSECTION_H_

bool Parallelogram_Intersect(RayDesc ray, float3 corner, float3 n, float3 v1, float3 v2, out float tHit, out float3 hit_point, out float Ldist)
{
    float eps = 0.00001f;
    tHit = dot(corner - ray.Origin, n) / (eps + dot(ray.Direction, n));
    if (tHit < ray.TMin)
        return false;
    hit_point = ray.Origin + tHit * ray.Direction;
    Ldist = length(hit_point - ray.Origin);
    float3 dp = hit_point - corner;
    float b1 = dot(dp, v1) / length(v1);
    float b2 = dot(dp, v2) / length(v2);
    return b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1;
}

#endif