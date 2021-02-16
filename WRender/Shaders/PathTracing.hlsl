//***************************************************************************************
// PathTracing.hlsl by LWJ (C) 2021 All Rights Reserved.
//***************************************************************************************
#include "IntersectionUtil.hlsl"

#define M_PI       3.14159265358979323846   // pi

// Constant data that varies per pass.
cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    float4 FogColor;
    float4 clearColor;
    float4 bgColor;
};

struct GeometryMaterial
{
    float3 diffuseAlbedo;
    float3 emission;
    float fPad0;
    float fPad1;
    int refl; // 0 for DIFF, 1 for SPEC, 2 for REFR
    int pad0;
    int pad1;
    int pad2;
};

StructuredBuffer<GeometryMaterial> gGeometryMaterials : register(t0, space1);
StructuredBuffer<Sphere> gSphereItems : register(t1, space1);
StructuredBuffer<Plane> gPlaneItems : register(t2, space1);
RWTexture2D<float4> gOutput : register(u0);

#define N 32

static float2 _Pixel;
static float _Seed;
static uint _Ns, ss;
static uint _Np, ps;

inline float Pow5(float x)
{
    return x * x * x * x * x;
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// F0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
inline float FresnelTerm(const float F0, const float cosA)
{

    float t = Pow5(1.0f - cosA);
    return F0 + (1.0f - F0) * t;

}

//float rand(inout float seed, float2 pixelId)
//{
//    float result = frac(sin(seed / 100.0f * dot(pixelId, float2(12.9898f, 78.233f))) * 43758.5453123f);
//    seed += 1.0f;
//    return result;
//}

float rand(inout int x, float2 pixelId)
{
    x = (314159269 * x + 453806245) % 2147483648;
    float result = x / 2147483648.0f;
    return result;
}


Ray CreateRay(float3 origin, float3 direction)
{
    Ray ray;
    ray.o = origin;
    ray.d = direction;
    return ray;
}

Ray CreateCameraRay(float2 uv)
{
    // 观察空间到世界空间变换
    float3 origin = gEyePosW;
    // 投影空间变换到观察空间
    float3 direction = mul(float4(uv, 0.0f, 1.0f), gInvProj).xyz;
    // 变换到世界空间 w=0只变换方向
    direction = mul(float4(direction, 0.0f), gInvView).xyz;
    direction = normalize(direction);
    return CreateRay(origin, direction);
}

float3 radiance(Ray r, inout float seed, float2 pixelId)
{
    int idx;
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    float3 energy = float3(1.0f, 1.0f, 1.0f);
    int depth = 0;
    int bumps = 8;
    int i = 0;
    float inf = 1e20;
    for (i; i < bumps; ++i)
    {
        if (!any(energy))
            break;
        float d, t = 1e20;
        int hitObjectType = 0; // 0 for sphere, 1 for plane
        // Intersection test
        for (uint si = 0; si < 3; ++si)
        {
            if ((d = raySphereIntersection(r, gSphereItems[si])) && d < t)
            {
                t = d;
                idx = si;
            }
        }
        for (uint pi = 0; pi < 5; ++pi)
        {
            if ((d = rayPlaneIntersection(r, gPlaneItems[pi])) && d < t)
            {
                t = d;
                idx = pi;
                hitObjectType = 1;
            }
        }
        // 与场景没有交点，退出循环
        if (!(t < inf)) break;
        float3 x = r.o + r.d * t;
        float3 n;
        int matIdx;
        if (hitObjectType == 0)
        {
            Sphere obj = gSphereItems[idx];
            matIdx = obj.matIdx;
            n = normalize(x - obj.p);
        }
        else if (hitObjectType == 1)
        {
            Plane obj = gPlaneItems[idx];
            matIdx = obj.matIdx;
            n = normalize(obj.n);
        }
        float3 f = gGeometryMaterials[matIdx].diffuseAlbedo;
        float3 e = gGeometryMaterials[matIdx].emission;
        int refl = gGeometryMaterials[matIdx].refl;
    
        float3 nl = dot(n, r.d) < 0 ? n : n * -1;
        float3 rl = r.d - n * 2 * dot(n, r.d);  // Direction of Reflect ray
        float p = f.x > f.y && f.x > f.z ? f.x : f.y > f.z ? f.y : f.z; // max refl 
        if (++depth > 5)
        {
            if (rand(seed, pixelId) < p)
                f = f * (1 / p);
            else
            {
                Lo += energy * e;
                break;
            }
        }
        // Ideal DIFFUSE reflection 
        if (refl == 0)
        {
            x += n * 0.001f;
            // 在半球上随机发射一条光线
            float r1 = 2 * M_PI * rand(seed, pixelId);
            float r2 = rand(seed, pixelId), r2s = sqrt(r2);
            float3 w = nl;
            float3 u = normalize(abs(w.x) > .1 ? float3(0, 1, 0) : cross(float3(1, 0, 0), w));
            float3 v = cross(u, w);
            float3 d = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2));
            // 重新发射光线
            r.o = x;
            r.d = d;
            // 总量累积
            Lo += energy * e;
            // 光线能量衰减
            energy *= f;
        }
        // Ideal SPECULAR reflection
        else if (refl == 1)
        {
            x += n * 0.001f;
            r.o = x;
            r.d = rl;
            // 总量累积
            Lo += energy * e;
            // 光线能量衰减
            energy *= f;
        }
        // Ideal dielectric REFRACTION 
        else
        {
            Ray reflRay = CreateRay(x, r.d - n * 2 * dot(n, (r.d)));
            bool into = dot(n, nl) > 0; // Ray from outside going in? 
            float nc = 1, nt = 1.5, nnt = into ? nc / nt : nt / nc, ddn = dot(r.d, nl), cos2t;
            if ((cos2t = 1 - nnt * nnt * (1 - ddn * ddn)) < 0)    // Total internal reflection 
            {
                r.o = x;
                r.d = rl;
                Lo += energy * e;
                energy *= f;
                continue;
            }
            float3 tdir = normalize(r.d * nnt - n * ((into ? 1 : -1) * (ddn * nnt + sqrt(cos2t))));
            float a = nt - nc, b = nt + nc, R0 = a * a / (b * b);
            float c = into ? -ddn : dot(tdir, (n));
            float Re = FresnelTerm(R0, c);
            float Tr = 1 - Re;
            float P = .25 + .5 * Re, RP = Re / P, TP = Tr / (1 - P);
            if (rand(seed, pixelId)<P)
            {
                r.o = x;
                r.d = rl;
                Lo += energy * e;
                energy *= RP;
            }
            else
            {
                r.o = x;
                r.d = tdir;
                Lo += energy * e;
                energy *= TP;
            }
        }
    }
    return Lo;
}

[numthreads(N, N, 1)]
void PathTracingCS(int3 id : SV_DispatchThreadID)
{
    // 初始化全局变量
    float2 pixelId = id.xy;
    _Pixel = id.xy;
    gSphereItems.GetDimensions(_Ns, ss);
    gPlaneItems.GetDimensions(_Np, ps);
    int samps = 25;
    // 获取RT分辨率
    uint width, height;
    gOutput.GetDimensions(width, height);
    int2 idx = int2(id.x, height - 1 - id.y);
    int seed = pixelId.x * pixelId.y;
    //float seed = 0.0f;
    for (int sy = 0; sy < 2; sy++)     // 2x2 subpixel rows 
        for (int sx = 0; sx < 2; sx++) // 2x2 subpixel cols
        {  
            float3 ro = float3(0.0, 0.0, 0.0);
            for (int s = 0; s < samps; s++)
            {             
                float2 _PixelOffset = float2(sx * 0.5f, sy * 0.5f);
                //float2 _PixelOffset = float2(rand(seed, pixelId), rand(seed, pixelId));
                float2 uv = float2((idx + _PixelOffset) / float2(width, height) * 2.0f - 1.0f);
                ro += radiance(CreateCameraRay(uv), seed, pixelId);
            } 
            ro *= (1. / samps);
            gOutput[id.xy] += float4(saturate(ro), 0.0f);
        }
    float3 toneColor = pow(saturate(gOutput[id.xy].xyz * .25), 1 / 2.2f);
    //float3 toneColor = gOutput[id.xy].xyz * .25;
    gOutput[id.xy] = float4(toneColor, 1.0f);
    //Ray ray = CreateCameraRay(uv);
    //gOutput[id.xy] = float4(ray.direction * 0.5f + 0.5f, 1.0f);
}
