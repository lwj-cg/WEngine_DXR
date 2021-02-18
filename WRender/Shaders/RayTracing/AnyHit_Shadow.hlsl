#include "Common.hlsl"

StructuredBuffer<ObjectConstants> gObjectBuffer : register(t0);
StructuredBuffer<MaterialData> gMaterialBuffer : register(t0, space1);

[shader("anyhit")]
void AnyHit_Shadow(inout RayPayload_shadow payload, Attributes attr)
{
    // Fetch Material Data
    ObjectConstants objectData = gObjectBuffer[InstanceIndex()];
    uint matIdx = objectData.MatIdx;
    MaterialData matData = gMaterialBuffer[matIdx];
    // Modify the val of inShadow
    float transparent = matData.Transparent;
    float3 emission = matData.Emission;
    if (transparent > 0)
    {
        payload.inShadow *= 0.8 * transparent;
        IgnoreHit();
    }
    else
    {
        payload.inShadow = 0;
        AcceptHitAndEndSearch();
    }
}