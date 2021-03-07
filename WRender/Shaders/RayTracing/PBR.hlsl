//***************************************************************************************
// PBR.hlsl by lwj (C) 2020 All Rights Reserved.
//***************************************************************************************

#ifndef PBR_H_
#define PBR_H_

// Include common HLSL code. (For declaration of MaterialData)
#include "Common.hlsl"
#include "Sampling.hlsl"

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
	float t = Pow5(1.0f - cosA); // ala Schlick interpoliation
	return lerp(F0, F90, t);
}


float3 BRDF(const float3 diffColor, const float3 specColor, const float smoothness,
	float3 normal, const float3 viewDir, const float3 lightDir,
	const uint type)
{
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
	
	// diffuse
	if (type == 0) return diffuseTerm * diffColor;
	// specular
	else if (type == 1) return specularTerm * FresnelTerm(specColor, lh);
	// mixture
	else return diffuseTerm * nl * diffColor + specularTerm * FresnelTerm(specColor, lh);
}

float3 PBR(const SurfaceInfo IN, const float3 lightDir, const float3 viewDir, const uint type)
{
	float3 factor;

	float oneMinusReflectivity;
	float3 baseColor, specColor;
    baseColor = DiffuseAndSpecularFromMetallic(IN.baseColor, IN.metallic, /*ref*/specColor, /*ref*/oneMinusReflectivity);

	factor = BRDF(baseColor, specColor, IN.smoothness, IN.normal, viewDir, lightDir, type);

	return factor;
}

void ImportanceSampleGGX(const float2 E, const float smoothness, out float3 n, out float pd)
{
    float perceptualRoughness = SmoothnessToPerceptualRoughness(smoothness);
    float roughness = PerceptualRoughnessToRoughness(perceptualRoughness);
    float m = roughness * roughness;
    float m2 = m * m;

    float Phi = 2 * M_PI * E.x;
    float CosTheta = sqrt((1 - E.y) / (1 + (m2 - 1) * E.y));
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    n.x = SinTheta * cos(Phi);
    n.y = SinTheta * sin(Phi);
    n.z = CosTheta;

    float d = (CosTheta * m2 - CosTheta) * CosTheta + 1;
    float D = m2 / (M_PI * d * d);

    pd = max(D * CosTheta, 12.f);
}

#endif
