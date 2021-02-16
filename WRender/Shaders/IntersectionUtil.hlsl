//***************************************************************************************
// PathTracing.hlsl by LWJ (C) 2021 All Rights Reserved.
//***************************************************************************************
struct Ray
{
    float3 o;
    float3 d;
};

struct Sphere
{
    float rad;
    float3 p;
    int matIdx;
    int pad0;
    int pad1;
    int pad2;
};

struct Plane
{
    float3 n;
    float3 p0;
    float d;
    int matIdx;
};

float raySphereIntersection(const Ray r, const Sphere s)
{
    // returns distance, 0 if nohit 
    // Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0 
    float3 op = s.p - r.o;
    float t, eps = 1e-4;
    float b = dot(op, r.d);
    float det = b * b - dot(op, op) + s.rad * s.rad;
    if (det < 0)
        return 0;
    else
        det = sqrt(det);
    return (t = b - det) > eps ? t : ((t = b + det) > eps ? t : 0);
}

float rayPlaneIntersection(const Ray r, const Plane p)
{
    float t, eps = 1e-4;
    // Solve n.(o+t*d-p0)=0 => t=n.(p0-o)/n.d
    t = dot(p.n, p.p0 - r.o) / dot(p.n, r.d);
    return t > eps ? t : 0;
}