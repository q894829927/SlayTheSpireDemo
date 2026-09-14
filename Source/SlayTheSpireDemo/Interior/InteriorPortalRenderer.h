#pragma once

#include "CoreMinimal.h"
#include "InteriorPortalMath.h"
#include "Math/RotationMatrix.h"
#include "Rendering/CustomRenderPass.h"
#include "SceneInterface.h"
#include "SceneViewExtension.h"

class FSceneViewFamily;
class FRDGBuilder;
class FRenderTarget;
struct FScreenPassTexture;
struct FPostProcessMaterialInputs;
struct FPostProcessingInputs;

/**
 * Immutable game-thread snapshot for a future main-view portal pass.
 *
 * This type deliberately contains no Actor, Component, RenderTarget or other
 * mutable UObject reference. It is safe to copy into a renderer-side queue.
 * STEP 1B.2 extends this description with the exact matrices consumed by
 * UE 5.8's public FCustomRenderPassRendererInput. The custom-pass backend
 * submits this snapshot to the main renderer. The optional render-target
 * resource is a copied external-resource identity used only by the explicit
 * composition spike; it is never a UObject or mutable Actor reference.
 */
struct SLAYTHESPIREDEMO_API FInteriorPortalRenderRequest
{
	int32 PortalId = INDEX_NONE;
	int32 EndpointIndex = INDEX_NONE;
	int32 RecursionLevel = 0;
	FTransform EntryFrame;
	FTransform ExitFrame;
	FTransform VirtualView;
	FVector ViewLocation = FVector::ZeroVector;
	FMatrix ViewRotationMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	InteriorPortalMath::FPortalScreenBounds ProjectedBounds;
	FIntRect ViewRect;
	FIntRect ScissorRect;
	FPlane ExitClipPlane = FPlane(FVector::ForwardVector, 0.0f);
	bool bExitClipEncodedInProjection = false;
	uint64 HistoryIdentity = 0;
	uint64 RendererHistoryGeneration = 0;
	/** External CRP output resource consumed by the same-frame BeforeDOF proof. */
	FRenderTarget* PortalRenderTarget = nullptr;
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
		const FMatrix& InProjectionMatrix,
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
		OutRequest.ViewLocation = OutRequest.VirtualView.GetLocation();
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		OutRequest.ViewRotationMatrix = FInverseRotationMatrix(
			OutRequest.VirtualView.Rotator()) * PortalViewPlanes;
		OutRequest.ProjectionMatrix = InProjectionMatrix;
		// FCustomRenderPassRendererInput has no GlobalClippingPlane field. Encode
		// the logical exit plane in the public projection matrix so the actual
		// custom-pass FSceneView still receives a deterministic clip contract.
		const FVector ExitNormal = InExitFrame.GetUnitAxis(EAxis::X);
		const FVector ExitPoint = InExitFrame.GetLocation() + ExitNormal * ClipPlaneBias;
		const FVector ViewPlaneNormal = OutRequest.VirtualView.InverseTransformVectorNoScale(ExitNormal);
		const FVector ViewPlanePoint = OutRequest.VirtualView.InverseTransformPositionNoScale(ExitPoint);
		const FVector4 ViewPlane(ViewPlaneNormal.Y, ViewPlaneNormal.Z, ViewPlaneNormal.X,
			-FVector::DotProduct(ViewPlaneNormal, ViewPlanePoint));
		FMatrix ObliqueProjection;
		if (InteriorPortalMath::TryObliqueProjection(InProjectionMatrix, ViewPlane, ObliqueProjection))
		{
			OutRequest.ProjectionMatrix = ObliqueProjection;
			OutRequest.bExitClipEncodedInProjection = true;
		}
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

/**
 * Project-side implementation of UE 5.8's public custom render-pass
 * contract. It intentionally writes to an existing external render target.
 * The separate composition spike consumes that target through the public
 * BeforeDOF post-process delegate; it does not claim public main-stencil or
 * depth-continuity access.
 */
class SLAYTHESPIREDEMO_API FInteriorPortalCustomRenderPass final : public FCustomRenderPassBase
{
public:
	FInteriorPortalCustomRenderPass(const FString& InDebugName, FRenderTarget* InRenderTarget,
		const FIntPoint& InRenderTargetSize);
	FInteriorPortalCustomRenderPass(const FInteriorPortalCustomRenderPass&) = delete;
	FInteriorPortalCustomRenderPass& operator=(const FInteriorPortalCustomRenderPass&) = delete;

	IMPLEMENT_CUSTOM_RENDER_PASS(FInteriorPortalCustomRenderPass)

	bool HasRenderTarget() const { return RenderTargetResource != nullptr; }

protected:
	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override;
	virtual void OnEndPass(FRDGBuilder& GraphBuilder) override;

private:
	FRenderTarget* RenderTargetResource = nullptr;
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

	inline bool CanSubmitCustomRenderPassRequest(bool bPairLinked, bool bPortalVisible,
		bool bValidBounds, bool bValidVirtualView, int32 RequestedRecursionDepth)
	{
		return bPairLinked && bPortalVisible && bValidBounds && bValidVirtualView
			&& RequestedRecursionDepth == 1;
	}

	inline bool CanSubmitCustomRenderPassCompositionRequest(bool bPairLinked, bool bPortalVisible,
		bool bValidBounds, bool bValidVirtualView, bool bHasPortalRenderTarget,
		int32 RequestedRecursionDepth)
	{
		return CanSubmitCustomRenderPassRequest(
			bPairLinked, bPortalVisible, bValidBounds, bValidVirtualView, RequestedRecursionDepth)
			&& bHasPortalRenderTarget;
	}

	inline bool BuildCustomRenderPassInput(
		const FInteriorPortalRenderRequest& Request,
		FSceneViewStateInterface* ViewState,
		FInteriorPortalCustomRenderPass* CustomRenderPass,
		FSceneInterface::FCustomRenderPassRendererInput& OutInput)
	{
		if (!Request.IsValid() || !CustomRenderPass)
		{
			return false;
		}

		OutInput.ViewLocation = Request.ViewLocation;
		OutInput.ViewRotationMatrix = Request.ViewRotationMatrix;
		OutInput.ProjectionMatrix = Request.ProjectionMatrix;
		// UE 5.8 validates that every FViewInfo assembled into one renderer owns a
		// unique ViewState. A target-backed public CRP can be queued into a renderer
		// alongside more than one view, and reusing our persistent endpoint state
		// triggers SceneRendering.cpp's UniqueViewStates assertion before any GPU
		// evidence is produced. The current STEP 1B.2/1B.3 CRP is only a spatial/
		// HDR feasibility pass (DepthAndBasePass, no temporal post-process), so keep
		// target-backed submissions intentionally stateless. A promoted renderer
		// candidate must reintroduce independently owned temporal histories with an
		// in-flight-safe lifetime model rather than sharing one state pointer.
		OutInput.ViewStateInterface = CustomRenderPass->HasRenderTarget() ? nullptr : ViewState;
		OutInput.bIsSceneCapture = false;
		OutInput.bUseMainViewFamilyShowFlags = true;
		OutInput.CustomRenderPass = CustomRenderPass;
		return true;
	}
}

/**
 * Project-side UE 5.8 renderer extension used to establish the integration
 * boundary. It owns copied request data and registers at the real main-view
 * extension lifecycle. The composition spike subscribes at BeforeDOF and
 * returns a new main SceneColor texture; it does not issue a post-tonemap
 * composite or read mutable UObject state on the render thread.
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
	virtual void SubscribeToPostProcessingPass(
		ISceneViewExtension::EPostProcessingPass Pass,
		const FSceneView& InView,
		FPostProcessingPassDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	static FScreenPassTexture ComposePortalIntoSceneColor(
		FRDGBuilder& GraphBuilder, const FSceneView& InView,
		const FPostProcessMaterialInputs& Inputs,
		const FInteriorPortalRenderRequest& Request);

	bool bEnabled = false;
	mutable FCriticalSection RequestMutex;
	TOptional<FInteriorPortalRenderRequest> PublishedRequest;
};
