//***************************************************************************************
// PBS.hlsl by lwj (C) 2020 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"
#pragma enable_d3d12_debug_symbols

#define M_E        2.71828182845904523536   // e
#define M_LOG2E    1.44269504088896340736   // log2(e)
#define M_LOG10E   0.434294481903251827651  // log10(e)
#define M_LN2      0.693147180559945309417  // ln(2)
#define M_LN10     2.30258509299404568402   // ln(10)
#define M_PI       3.14159265358979323846   // pi
#define M_PI_2     1.57079632679489661923   // pi/2
#define M_PI_4     0.785398163397448309616  // pi/4
#define M_1_PI     0.318309886183790671538  // 1/pi
#define M_2_PI     0.636619772367581343076  // 2/pi
#define M_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#define M_SQRT2    1.41421356237309504880   // sqrt(2)
#define M_SQRT1_2  0.707106781186547524401  // 1/sqrt(2)

inline float Pow4(float x)
{
	return x * x * x * x;
}

inline float2 Pow4(float2 x)
{
	return x * x * x * x;
}

inline float3 Pow4(float3 x)
{
	return x * x * x * x;
}

inline float4 Pow4(float4 x)
{
	return x * x * x * x;
}

inline float Pow5(float x)
{
	return x * x * x * x * x;
}

inline float2 Pow5(float2 x)
{
	return x * x * x * x * x;
}

inline float3 Pow5(float3 x)
{
	return x * x * x * x * x;
}

inline float4 Pow5(float4 x)
{
	return x * x * x * x * x;
}

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

inline float OneMinusReflectivityFromMetallic(const float metallic)
{
	float oneMinusDielectricSpec = 1.0f - 0.220916301f;
	return oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
}


inline float3 DiffuseAndSpecularFromMetallic(const float3 albedo, const float metallic, out float3 specColor, out float oneMinusReflectivity)
{
	specColor = lerp(float3(0.220916301f, 0.220916301f, 0.220916301f), albedo, metallic);
	oneMinusReflectivity = OneMinusReflectivityFromMetallic(metallic);
	return albedo * oneMinusReflectivity;
}

inline float SmoothnessToPerceptualRoughness(const float smoothness)
{
	return (1 - smoothness);
}

float DisneyDiffuse(const float NdotV, const float NdotL, const float LdotH, const float perceptualRoughness)
{
	float fd90 = 0.5 + 2 * LdotH * LdotH * perceptualRoughness;
	// Two schlick fresnel term
	float lightScatter = (1 + (fd90 - 1) * Pow5(1 - NdotL));
	float viewScatter = (1 + (fd90 - 1) * Pow5(1 - NdotV));

	return lightScatter * viewScatter;
}

inline float PerceptualRoughnessToRoughness(const float perceptualRoughness)
{
	return perceptualRoughness * perceptualRoughness;
}

inline float SmithJointGGXVisibilityTerm(const float NdotL, const float NdotV, const float roughness)
{
	// Approximation of the above formulation (simplify the sqrt, not mathematically correct but close enough)
	float a = roughness;
	float lambdaV = NdotL * (NdotV * (1 - a) + a);
	float lambdaL = NdotV * (NdotL * (1 - a) + a);

#ifndef SHADER_API_SWITCH
	return 0.5f / (lambdaV + lambdaL + 1e-5f);
#else
	return 0.5f / (lambdaV + lambdaL + 1e-4f); // work-around against hlslcc rounding error
#endif
}

inline float GGXTerm(const float NdotH, const float roughness)
{
	float a2 = roughness * roughness;
	float d = (NdotH * a2 - NdotH) * NdotH + 1.0f; // 2 mad
	return M_1_PI * a2 / (d * d + 1e-7f); // This function is not intended to be running on Mobile,
										  // therefore epsilon is smaller than what can be represented by float
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// F0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
inline float3 FresnelTerm(const float3 F0, const float cosA)
{

	float t = Pow5(1.0f - cosA);
	return F0 + (1.0f - F0) * t;

}

inline float3 FresnelLerp(const float3 F0, const float3 F90, const float cosA)
{
	float t = Pow5(1.0f - cosA);   // ala Schlick interpoliation
	return lerp(F0, F90, t);
}


float3 BRDF(const float3 diffColor, const float3 specColor, const float smoothness,
	float3 normal, const float3 viewDir, const float3 lightDir,
	const float3 lightSatu, const float3 fresnelR0) {
	float perceptualRoughness = SmoothnessToPerceptualRoughness(smoothness);
	float3 halfVec = normalize(lightDir + viewDir);

	float shiftAmount = dot(normal, viewDir);
	normal = shiftAmount < 0.0f ? normal + viewDir * (-shiftAmount + 1e-5f) : normal;

	float nv = saturate(dot(normal, viewDir));

	float nl = saturate(dot(normal, lightDir));
	float nh = saturate(dot(normal, halfVec));

	float lv = saturate(dot(lightDir, viewDir));
	float lh = saturate(dot(lightDir, halfVec));

	float diffuseTerm = DisneyDiffuse(nv, nl, lh, perceptualRoughness);

	float roughness = PerceptualRoughnessToRoughness(perceptualRoughness);

	roughness = max(roughness, 0.002f);
	float G = SmithJointGGXVisibilityTerm(nl, nv, roughness);
	float D = GGXTerm(nh, roughness);

	float specularTerm = G * D * M_PI;

	specularTerm = max(0.0f, specularTerm * nl);

	specularTerm *= any(specColor) ? 1.0f : 0.0f;

    return diffuseTerm * nl * lightSatu * diffColor + specularTerm * lightSatu * FresnelTerm(fresnelR0, lh);
}

float3 PBS(const MaterialData matData, const float3 lightDir, const float3 lightSatu,
	const float3 normal, const float3 viewDir, const float2 TexC) {
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	float  metallic = matData.Metallic;
	uint diffuseTexIndex = matData.DiffuseMapIndex;

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, TexC);

	float3 color;

	float oneMinusReflectivity;
	float3 baseColor, specColor;
	baseColor = DiffuseAndSpecularFromMetallic(diffuseAlbedo.rgb, metallic, /*ref*/ specColor, /*ref*/ oneMinusReflectivity);

	color = BRDF(baseColor, specColor, 1.0f-roughness, normal, viewDir, lightDir, lightSatu, fresnelR0);
    float3 ambient = (gAmbientLight * diffuseAlbedo).rgb;
    color = color + ambient;

	return color;
}

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	// Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
	// Interpolating normal can unnormalize it, so renormalize it.
	float3 normalW = normalize(pin.NormalW);

    // PBS.
	Light L = gLights[0];
	float3 lightDir = -L.Direction;
	float3 lightSatu = saturate(L.Strength);
	float3 litColor = PBS(matData, lightDir, lightSatu, normalW, toEyeW, pin.TexC);

    // Common convention to take alpha from diffuse albedo.
	float4 outColor;
	outColor.rgb = litColor;
    outColor.a = diffuseAlbedo.a;

    return outColor;
}


