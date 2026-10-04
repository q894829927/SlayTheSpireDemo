#pragma once

#include "CoreMinimal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalProjectiveAperture.h"
#include "InteriorPortalRenderSample.h"
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
	/** Historical inverse homography retained for diagnostics/stencil compatibility. */
	InteriorPortalProjectiveAperture::FScreenToPortalMapping ProjectiveAperture;
	/** Production analytic ray/plane aperture geometry plus cosmetic-surface bias. */
	InteriorPortalProjectiveAperture::FScreenToPortalMapping ForegroundDepthReference;
	float ProjectiveNearClipW = 0.0f;
	FIntRect ViewRect;
	FIntRect ScissorRect;
	FPlane ExitClipPlane = FPlane(FVector::ForwardVector, 0.0f);
	bool bExitClipEncodedInProjection = false;
	uint64 HistoryIdentity = 0;
	uint64 RendererHistoryGeneration = 0;
	FRenderTarget* PortalRenderTarget = nullptr;
	FRenderTarget* PortalDepthRenderTarget = nullptr;
	TSharedPtr<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> ColorSample;
	bool bEnabled = false;
	bool bPlayerExposureAuthority = true;
	/** Full-fidelity composition owns the logical opening and rim, not the opaque fallback mesh. */
	bool bLogicalPlaneComposition = false;
	FPrimitiveComponentId CompositedSurface;
	uint32 CompositedSupportStencil = 0;
	FLinearColor RimColor = FLinearColor::Black;
	float RimTimeSeconds = 0.0f;

	/** An attached portal is a directed opening on the frame's positive X side.
	 * Test the receiving camera, never the transported child camera. */
	static bool IsFrontFacing(const FTransform& Frame, const FVector& ReceivingOrigin)
	{
		const double Distance = FVector::DotProduct(
			ReceivingOrigin - Frame.GetLocation(), Frame.GetUnitAxis(EAxis::X));
		return FMath::IsFinite(Distance) && Distance > 0.0;
	}

	bool CanComposeFrom(const FVector& ReceivingOrigin) const
	{
		return !bLogicalPlaneComposition || IsFrontFacing(EntryFrame, ReceivingOrigin);
	}

	FVector3f GetTranslatedPlaneCenter(const FVector& PreViewTranslation) const
	{
		// Subtract the receiving camera in double precision BEFORE GPU encoding.
		// World-space float geometry loses sub-centimeter slices before this step.
		return FVector3f(EntryFrame.GetLocation() + PreViewTranslation);
	}

	void ApplyCompositionVisibility(FSceneView& ReceivingView) const
	{
		if (IsValid() && bLogicalPlaneComposition && CompositedSurface.IsValid()
			&& CanComposeFrom(ReceivingView.ViewMatrices.GetViewOrigin()))
		{
			ReceivingView.HiddenPrimitives.Add(CompositedSurface);
		}
	}

	FVector4f GetReceivingPlaneDepthEquation(const FMatrix& ClipToTranslatedWorld,
		const FVector& PreViewTranslation, const FIntRect& ReceivingRect) const
	{
		const FVector Normal = EntryFrame.GetUnitAxis(EAxis::X);
		const FVector Center = EntryFrame.GetLocation() + PreViewTranslation;
		const FVector4 Plane = ClipToTranslatedWorld.GetTransposed().TransformFVector4(
			FVector4(Normal, -FVector::DotProduct(Normal, Center)));
		if (FMath::Abs(Plane.Z) <= UE_DOUBLE_SMALL_NUMBER
			|| ReceivingRect.Width() <= 0 || ReceivingRect.Height() <= 0)
		{
			return FVector4f(0, 0, 0, 0);
		}
		// A planar raster's DeviceZ is affine in raster pixel coordinates. Build
		// its coefficients in double, before GPU world reconstruction can cancel.
		const double X = -Plane.X / Plane.Z;
		const double Y = -Plane.Y / Plane.Z;
		const double Px = 2.0 * X / ReceivingRect.Width();
		const double Py = -2.0 * Y / ReceivingRect.Height();
		return FVector4f(float(Px), float(Py), float(-Plane.W / Plane.Z - X + Y
			- Px * ReceivingRect.Min.X - Py * ReceivingRect.Min.Y), 1.0f);
	}

	void UseLogicalPlaneComposition(FPrimitiveComponentId SurfaceId,
		const FLinearColor& InRimColor, float InTimeSeconds)
	{
		bLogicalPlaneComposition = true;
		CompositedSurface = SurfaceId;
		RimColor = InRimColor;
		RimTimeSeconds = InTimeSeconds;
		ForegroundDepthReference.Row2.W = 0.0f;
		// Strict-positive exit half-space: hardware clip keeps distance == 0,
		// which would retain the coplanar host wall. Exclude numerical ties with
		// a float-roundoff bound in translated view space, never cosmetic bias.
		const double ClipRoundoffCm = FMath::Max(
			(ExitFrame.GetLocation() - ViewLocation).Size() * 4.768371582e-7, 1.0e-4);
		InteriorPortalMath::BuildPortalClipPlane(ExitFrame, ClipRoundoffCm, ExitClipPlane);
	}

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
		if (InPortalId == INDEX_NONE || InEndpointIndex < 0
			|| InRecursionLevel < 0 || InRecursionLevel > 3)
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

		InteriorPortalProjectiveAperture::BuildScreenToPortalMapping(
			InEntryFrame, HalfWidth, HalfHeight, PortalViewProjection,
			OutRequest.ProjectiveAperture);
		InteriorPortalProjectiveAperture::BuildAnalyticRayPlaneGeometry(
			InEntryFrame, HalfWidth, HalfHeight, 0.0,
			OutRequest.ForegroundDepthReference);

		OutRequest.ProjectiveNearClipW = bPerspectiveProjection
			? float(FMath::Max(0.001, NearClip))
			: 0.0f;
		OutRequest.ViewRect = InViewRect;
		OutRequest.ScissorRect = ScissorRect;
		OutRequest.ExitClipPlane = ExitClipPlane;
		OutRequest.RendererHistoryGeneration = InRendererHistoryGeneration;
		OutRequest.HistoryIdentity = MakeHistoryIdentity(
			InEndpointIndex, InRecursionLevel, InRendererHistoryGeneration);
		OutRequest.bEnabled = true;
		OutRequest.bPlayerExposureAuthority = InRecursionLevel == 0;
		return true;
	}

	bool IsValid() const
	{
		return bEnabled
			&& PortalId != INDEX_NONE
			&& EndpointIndex >= 0
			&& RecursionLevel >= 0 && RecursionLevel <= 3
			&& ProjectedBounds.bHasVisiblePortion
			&& ViewRect.Width() > 0 && ViewRect.Height() > 0
			&& ScissorRect.Width() > 0 && ScissorRect.Height() > 0
			&& HistoryIdentity != 0;
	}
};

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
		OutInput.ViewStateInterface = CustomRenderPass->HasRenderTarget() ? nullptr : ViewState;
		OutInput.bIsSceneCapture = false;
		OutInput.bUseMainViewFamilyShowFlags = true;
		OutInput.CustomRenderPass = CustomRenderPass;
		return true;
	}
}

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

	/**
	 * Shared production compositor entry point. Recursive full-fidelity view
	 * extensions call the same function for an exact parent secondary ViewState;
	 * top-level composition still reaches it through the normal main-view hook.
	 */
	static FScreenPassTexture ComposePortalIntoSceneColor(
		FRDGBuilder& GraphBuilder, const FSceneView& InView,
		const FPostProcessMaterialInputs& Inputs,
		const FInteriorPortalRenderRequest& Request);

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	bool bEnabled = false;
	mutable FCriticalSection RequestMutex;
	TOptional<FInteriorPortalRenderRequest> PublishedRequest;
};
