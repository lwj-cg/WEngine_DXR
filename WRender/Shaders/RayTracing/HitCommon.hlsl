#ifndef HIT_COMMON_H_
#define HIT_COMMON_H_

#include "Common.hlsl"
// Raytracing acceleration structure, accessed as a SRV
StructuredBuffer<ObjectConstants> gObjectBuffer : register(t0, space1);
StructuredBuffer<MaterialData> gMaterialBuffer : register(t0, space2);
StructuredBuffer<Vertex> gVertexBuffer : register(t0, space3);
StructuredBuffer<Normal> gNormalBuffer : register(t0, space4);
StructuredBuffer<TexCoord> gTexCoordBuffer : register(t0, space5);
StructuredBuffer<int> gIndexBuffer : register(t0, space6);
StructuredBuffer<int> gNormalIndexBuffer : register(t0, space7);
StructuredBuffer<int> gTexCoordIndexBuffer : register(t0, space8);
StructuredBuffer<ParallelogramLight> gLightBuffer : register(t0, space9);

RaytracingAccelerationStructure SceneBVH : register(t0);
Texture2D gTextureMaps[] : register(t1, space0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

#endif