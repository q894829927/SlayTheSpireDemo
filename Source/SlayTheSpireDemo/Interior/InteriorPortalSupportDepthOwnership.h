#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
class UPrimitiveComponent;
class UWorld;

/** Rendering-only attachment leases. Never changes collision, visibility or assets.
 * Shared supports have one identity; unrelated custom-depth users retain their state. */
class SLAYTHESPIREDEMO_API FInteriorPortalSupportDepthOwnership
{
public:
	FInteriorPortalSupportDepthOwnership() = default;
	FInteriorPortalSupportDepthOwnership(const FInteriorPortalSupportDepthOwnership&) = delete;
	FInteriorPortalSupportDepthOwnership& operator=(const FInteriorPortalSupportDepthOwnership&) = delete;
	~FInteriorPortalSupportDepthOwnership();
	void Start(UWorld* World);
	void Refresh(UWorld* World, UPrimitiveComponent* Blue, UPrimitiveComponent* Orange);
	uint32 GetStencil(const UPrimitiveComponent* Support) const;
	void Reset();
private:
	struct FBinding
	{
		TWeakObjectPtr<UPrimitiveComponent> Support;
		int32 Stencil = 0;
		int32 PreviousStencil = 0;
		ERendererStencilMask PreviousWriteMask = ERendererStencilMask::ERSM_Default;
		bool bOwnsComponentState = false;
		bool bIdentityUnique = true;
	};
	static bool Matches(const FBinding& Binding);
	static void Release(const FBinding& Binding);
	void RestoreDepthMode();
	TWeakObjectPtr<UWorld> ActiveWorld;
	TArray<FBinding, TInlineAllocator<2>> Bindings;
	int32 PreviousDepthMode = 0;
	int32 PreviousDepthJitter = 1;
	uint32 DepthModePriority = 0;
	uint32 DepthJitterPriority = 0;
	bool bOwnsDepthMode = false;
	bool bOwnsDepthJitter = false;
};
