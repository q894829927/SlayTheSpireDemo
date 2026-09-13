#pragma once

#include "CoreMinimal.h"
#include "InteriorPortalMath.h"
#include "SceneViewExtension.h"

class FSceneViewFamily;
class FRDGBuilder;
struct FPostProcessingInputs;

/**
 * Immutable game-thread snapshot for a future main-view portal pass.
 *
 * This type deliberately contains no Actor, Component, RenderTarget or other
 * mutable UObject reference. It is safe to copy into a renderer-side queue.
 * STEP 1B currently builds and diagnoses this description, but does not claim
 * that UE 5.8's project-side API can submit it as a second scene pass.
 */
struct SLAYTHESPIREDEMO_API FInteriorPortalRenderRequest
{
	int32 PortalId = INDEX_NONE;
	int32 EndpointIndex = INDEX_NONE;
	int32 RecursionLevel = 0;
	FTransform EntryFrame;
	FTransform ExitFrame;
	FTransform VirtualView;
	InteriorPortalMath::FPortalScreenBounds ProjectedBounds;
	FIntRect ViewRect;
	FIntRect ScissorRect;
	FPlane ExitClipPlane = FPlane(FVector::ForwardVector, 0.0f);
	uint64 HistoryIdentity = 0;
	uint64 RendererHistoryGeneration = 0;
	bool bEnabled = false;
	/** The intended contract is one player-main-view exposure/tone-map owner. */
	bool bPlayerExposureAuthority = true;

	static uint64 MakeHistoryIdentity(int32 InEndpointIndex, int32 InRecursionLevel,
		uint64 InRendererHistoryGeneration)
	{
		return (InRendererHistoryGeneration << 8)
			| (uint64(FMath::Max(InEndpointIndex, 0)) << 4)
			| uint64(FMath::Max(InRecursionLevel, 0));
	}

	static bool Build(int32 InPortalId, int32 InEndpointIndex, int32 InRecursionLevel,
		const FTransform& PlayerView, const FTransform& InEntryFrame,
		const FTransform& InExitFrame, double HalfWidth, double HalfHeight,
		const FMatrix& PortalViewProjection, const FIntRect& InViewRect,
		bool bPerspectiveProjection, double NearClip, double ClipPlaneBias,
		uint64 InRendererHistoryGeneration, FInteriorPortalRenderRequest& OutRequest)
	{
		OutRequest = FInteriorPortalRenderRequest();
		if (InPortalId == INDEX_NONE || InEndpointIndex < 0 || InRecursionLevel != 0)
		{
			return false;
		}

		InteriorPortalMath::FPortalScreenBounds Bounds;
		if (!InteriorPortalMath::ProjectPortalApertureToScreenBounds(
			InEntryFrame, HalfWidth, HalfHeight, PortalViewProjection, InViewRect,
			Bounds, bPerspectiveProjection, NearClip))
		{
			return false;
		}

		FIntRect ScissorRect;
		if (!InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, InViewRect, ScissorRect))
		{
			return false;
		}

		FPlane ExitClipPlane;
		if (!InteriorPortalMath::BuildPortalClipPlane(InExitFrame, ClipPlaneBias, ExitClipPlane))
		{
			return false;
		}

		OutRequest.PortalId = InPortalId;
		OutRequest.EndpointIndex = InEndpointIndex;
		OutRequest.RecursionLevel = InRecursionLevel;
		OutRequest.EntryFrame = InEntryFrame;
		OutRequest.ExitFrame = InExitFrame;
		OutRequest.VirtualView = InteriorPortalMath::BuildVirtualViewTransform(
			PlayerView, InEntryFrame, InExitFrame);
		OutRequest.ProjectedBounds = Bounds;
		OutRequest.ViewRect = InViewRect;
		OutRequest.ScissorRect = ScissorRect;
		OutRequest.ExitClipPlane = ExitClipPlane;
		OutRequest.RendererHistoryGeneration = InRendererHistoryGeneration;
		OutRequest.HistoryIdentity = MakeHistoryIdentity(
			InEndpointIndex, InRecursionLevel, InRendererHistoryGeneration);
		OutRequest.bEnabled = true;
		OutRequest.bPlayerExposureAuthority = true;
		return true;
	}

	bool IsValid() const
	{
		return bEnabled
			&& PortalId != INDEX_NONE
			&& EndpointIndex >= 0
			&& RecursionLevel == 0
			&& ProjectedBounds.bHasVisiblePortion
			&& ViewRect.Width() > 0 && ViewRect.Height() > 0
			&& ScissorRect.Width() > 0 && ScissorRect.Height() > 0
			&& HistoryIdentity != 0;
	}
};

/** State of the project-side feasibility spike, not visual acceptance. */
enum class EInteriorPortalSpikeStatus : uint8
{
	Disabled,
	Ready,
	Submitted,
	Partial,
	Blocked
};

inline const TCHAR* InteriorPortalSpikeStatusToString(EInteriorPortalSpikeStatus Status)
{
	switch (Status)
	{
	case EInteriorPortalSpikeStatus::Disabled: return TEXT("Disabled");
	case EInteriorPortalSpikeStatus::Ready: return TEXT("Ready");
	case EInteriorPortalSpikeStatus::Submitted: return TEXT("Submitted");
	case EInteriorPortalSpikeStatus::Partial: return TEXT("Partial");
	case EInteriorPortalSpikeStatus::Blocked: return TEXT("Blocked");
	default: return TEXT("Unavailable / Unverified");
	}
}

namespace InteriorPortalRenderer
{
	/** Activation gate for the one-pair, one-layer feasibility request. */
	inline bool CanSubmitMainViewStencilRequest(bool bPairLinked, bool bPortalVisible,
		bool bValidBounds, bool bValidVirtualView, int32 RequestedRecursionDepth)
	{
		return bPairLinked && bPortalVisible && bValidBounds && bValidVirtualView
			&& RequestedRecursionDepth == 1;
	}
}

/**
 * Project-side UE 5.8 renderer extension used to establish the integration
 * boundary. It owns copied request data and registers at the real main-view
 * extension lifecycle. It intentionally does not issue a fake SceneCapture or
 * post-tonemap composite when the required scene-render pass is unavailable.
 */
class SLAYTHESPIREDEMO_API FInteriorPortalViewExtension final : public FWorldSceneViewExtension
{
public:
	FInteriorPortalViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	void SetEnabled(bool bInEnabled);
	bool IsEnabled() const { return bEnabled; }
	void PublishRequest(const FInteriorPortalRenderRequest& InRequest);
	void ClearRequest();
	bool HasPublishedRequest() const;
	FInteriorPortalRenderRequest GetPublishedRequest() const;

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder,
		FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder,
		FSceneViewFamily& InViewFamily) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	bool bEnabled = false;
	mutable FCriticalSection RequestMutex;
	TOptional<FInteriorPortalRenderRequest> PublishedRequest;
};
