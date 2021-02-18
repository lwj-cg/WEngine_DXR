#include "Common.hlsl"

[shader("anyhit")]
void AnyHit_Shadow(inout RayPayload_shadow payload, Attributes attr)
{
    // Have not consider transparent yet
    payload.inShadow = true;
    AcceptHitAndEndSearch();
}