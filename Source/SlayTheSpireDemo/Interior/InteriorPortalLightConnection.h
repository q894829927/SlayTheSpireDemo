#pragma once

#include "CoreMinimal.h"
#include "PrimitiveComponentId.h"
#include "UObject/ObjectKey.h"

class AInteriorPortalSystem;
class ISceneViewExtension;
class FSceneViewFamily;

namespace InteriorPortalLighting
{
    // Renderer input only. No actor/component may be dereferenced on the render thread.
    struct FConnectionInput
    {
        uint32 Count = 0;
        FTransform Frames[2];
        FVector2D Sizes[2] = {};
        FObjectKey Endpoints[2]; // immutable identity tokens, never resolved on RT
        FPrimitiveComponentId Supports[2];
        FPrimitiveComponentId Surfaces[2]; // optional cosmetic exclusion, not connection authority

        SLAYTHESPIREDEMO_API bool IsValid() const;
        SLAYTHESPIREDEMO_API bool Matches(const FConnectionInput& Other) const;
    };

    struct FSealedConnection
    {
        FConnectionInput Input;
        uint64 Frame = 0;
        uint64 Generation = 0;
    };

    // The first renderer of a game frame seals the entire scene, including invisible endpoints.
    // Mid-frame changes become visible at the next frame boundary, never half a pair.
    class SLAYTHESPIREDEMO_API FConnectionTimeline
    {
    public:
        FSealedConnection Seal(const FConnectionInput& Input, uint64 Frame);
    private:
        FSealedConnection Current;
        bool bSealed = false;
    };

    SLAYTHESPIREDEMO_API TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> Acquire(AInteriorPortalSystem* System);
    void Attach(const TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe>& Connection, FSceneViewFamily& Family);
}

namespace InteriorPortalLightVisibilityProbe
{
    // Opt-in observer only; additional families do not gather global extensions automatically.
    void AttachToFamily(FSceneViewFamily& Family);
}
