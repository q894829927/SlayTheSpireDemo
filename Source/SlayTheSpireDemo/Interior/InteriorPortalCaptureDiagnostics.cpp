#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalSystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "SceneView.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPortalCaptureFrameDiagnostics(
		TEXT("portal.CaptureFrameDiagnostics"),
		0,
		TEXT("When non-zero, log the SceneCapture depth-0 update contract, capture transform, and native exit clip-plane contract every game frame.\n")
		TEXT("Use this only for short Portal diagnostics; it intentionally produces one line per endpoint per frame."),
		ECVF_Default);

	void LogPortalCaptureFrame(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		(void)TickType;
		(void)DeltaSeconds;
		if (!World || !World->IsGameWorld()
			|| CVarPortalCaptureFrameDiagnostics.GetValueOnGameThread() == 0)
		{
			return;
		}

		AInteriorPortalSystem* PortalSystem = nullptr;
		for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				PortalSystem = *It;
				break;
			}
		}
		if (!PortalSystem || !PortalSystem->IsLinked()
			|| !AInteriorPortalSystem::UsesSceneCapture(PortalSystem->RendererBackend))
		{
			return;
		}

		APlayerController* Player = World->GetFirstPlayerController();
		if (!Player || !Player->PlayerCameraManager)
		{
			return;
		}

		ULocalPlayer* Local = Player->GetLocalPlayer();
		FSceneViewProjectionData ProjectionData;
		if (!Local || !Local->ViewportClient
			|| !Local->GetProjectionData(Local->ViewportClient->Viewport, ProjectionData))
		{
			return;
		}

		const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
		const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
		const FTransform PlayerView(POV.Rotation, POV.Location);
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));

		const auto ViewProjectionForTransform = [&ProjectionData, &PortalViewPlanes](const FTransform& View)
		{
			return FTranslationMatrix(-View.GetLocation()) * FInverseRotationMatrix(View.Rotator())
				* PortalViewPlanes * ProjectionData.ProjectionMatrix;
		};
		const auto IsVisible = [&ProjectionData, &ViewRect, &ViewProjectionForTransform](
			const AInteriorPortal* Portal, const FTransform& View,
			InteriorPortalMath::FPortalScreenBounds& OutBounds)
		{
			return Portal && InteriorPortalMath::ProjectPortalApertureToScreenBounds(
				Portal->GetLogicalFrame(), Portal->HalfWidth, Portal->HalfHeight,
				ViewProjectionForTransform(View), ViewRect, OutBounds,
				ProjectionData.IsPerspectiveProjection(),
				ProjectionData.GetNearPlaneFromProjectionMatrix());
		};

		for (AInteriorPortal* Entry : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
		{
			if (!IsValid(Entry))
			{
				continue;
			}
			AInteriorPortal* Exit = Entry == PortalSystem->BluePortal
				? PortalSystem->OrangePortal.Get() : PortalSystem->BluePortal.Get();
			if (!IsValid(Exit))
			{
				continue;
			}

			InteriorPortalMath::FPortalScreenBounds DirectBounds;
			const bool bDirectVisible = IsVisible(Entry, PlayerView, DirectBounds);
			const FTransform ExpectedDepth0 = InteriorPortalMath::BuildVirtualViewTransform(
				PlayerView, Entry->GetLogicalFrame(), Exit->GetLogicalFrame());
			InteriorPortalMath::FPortalScreenBounds RecursiveBounds;
			const bool bFirstVirtualSeesEntry = IsVisible(Entry, ExpectedDepth0, RecursiveBounds);

			USceneCaptureComponent2D* Capture = Entry->GetCaptureForDepth(0);
			const bool bHasCapture = IsValid(Capture);
			const FTransform CaptureTransform = bHasCapture
				? Capture->GetComponentTransform() : FTransform::Identity;
			const bool bLocationMatches = bHasCapture
				&& CaptureTransform.GetLocation().Equals(ExpectedDepth0.GetLocation(), 0.1f);
			const bool bRotationMatches = bHasCapture
				&& CaptureTransform.GetRotation().AngularDistance(ExpectedDepth0.GetRotation()) < FMath::DegreesToRadians(0.1f);
			const bool bCaptureTransformMatchesExpected = bLocationMatches && bRotationMatches;

			// Depth 0 follows direct player visibility only. Seeing Entry again from
			// the mapped view is now solely the gate for a deeper recursive layer.
			const bool bDepth0CaptureRequired = bDirectVisible;
			const bool bRecursiveDepth1Visible = bDirectVisible && bFirstVirtualSeesEntry;

			const FTransform ExitFrame = Exit->GetLogicalFrame();
			const FVector ExpectedClipNormal = ExitFrame.GetUnitAxis(EAxis::X).GetSafeNormal();
			const FVector ExpectedClipBase = ExitFrame.GetLocation()
				+ ExpectedClipNormal * PortalSystem->ClipPlaneBias;
			const double VirtualCameraPlaneDistance = FVector::DotProduct(
				ExpectedDepth0.GetLocation() - ExpectedClipBase, ExpectedClipNormal);
			const bool bNativeClipMode = PortalSystem->RenderClipMode == EInteriorPortalRenderClipMode::NativeClipPlane;
			const bool bCaptureClipEnabled = bHasCapture && Capture->bEnableClipPlane;
			const bool bCaptureClipBaseMatches = bHasCapture
				&& Capture->ClipPlaneBase.Equals(ExpectedClipBase, 0.1f);
			const FVector CaptureClipNormal = bHasCapture
				? Capture->ClipPlaneNormal.GetSafeNormal() : FVector::ZeroVector;
			const bool bCaptureClipNormalMatches = bHasCapture
				&& FVector::DotProduct(CaptureClipNormal, ExpectedClipNormal) > 0.9999f;
			const bool bNativeClipConfigMatchesExpected = bNativeClipMode
				&& bCaptureClipEnabled && bCaptureClipBaseMatches && bCaptureClipNormalMatches;

			UE_LOG(LogTemp, Display,
				TEXT("PortalCaptureDiag Frame=%llu Entry=%s PlayerPitch=%.3f VirtualPitch=%.3f DirectVisible=%d Depth0CaptureRequired=%d RecursiveDepth1Visible=%d CaptureTransformMatchesExpected=%d CapturePitch=%.3f RecursionDepth=%d ClipMode=%s ClipPlaneEnabled=%d NativeClipConfigMatchesExpected=%d CameraPlaneDistance=%.3f ExpectedClipBase=(%.2f,%.2f,%.2f) ExpectedClipNormal=(%.4f,%.4f,%.4f) CaptureClipBase=(%.2f,%.2f,%.2f) CaptureClipNormal=(%.4f,%.4f,%.4f)"),
				GFrameCounter,
				Entry == PortalSystem->BluePortal ? TEXT("Blue") : TEXT("Orange"),
				POV.Rotation.Pitch,
				ExpectedDepth0.Rotator().Pitch,
				bDirectVisible ? 1 : 0,
				bDepth0CaptureRequired ? 1 : 0,
				bRecursiveDepth1Visible ? 1 : 0,
				bCaptureTransformMatchesExpected ? 1 : 0,
				bHasCapture ? CaptureTransform.Rotator().Pitch : 0.0f,
				PortalSystem->RecursionDepth,
				bNativeClipMode ? TEXT("NativeClipPlane") : TEXT("ObliqueFallbackKnownBroken"),
				bCaptureClipEnabled ? 1 : 0,
				bNativeClipConfigMatchesExpected ? 1 : 0,
				VirtualCameraPlaneDistance,
				ExpectedClipBase.X, ExpectedClipBase.Y, ExpectedClipBase.Z,
				ExpectedClipNormal.X, ExpectedClipNormal.Y, ExpectedClipNormal.Z,
				bHasCapture ? Capture->ClipPlaneBase.X : 0.0,
				bHasCapture ? Capture->ClipPlaneBase.Y : 0.0,
				bHasCapture ? Capture->ClipPlaneBase.Z : 0.0,
				CaptureClipNormal.X, CaptureClipNormal.Y, CaptureClipNormal.Z);
		}
	}

	struct FPortalCaptureDiagnosticsRegistration
	{
		FDelegateHandle Handle;

		FPortalCaptureDiagnosticsRegistration()
		{
			Handle = FWorldDelegates::OnWorldPostActorTick.AddStatic(&LogPortalCaptureFrame);
		}

		~FPortalCaptureDiagnosticsRegistration()
		{
			if (Handle.IsValid())
			{
				FWorldDelegates::OnWorldPostActorTick.Remove(Handle);
			}
		}
	};

	FPortalCaptureDiagnosticsRegistration GPortalCaptureDiagnosticsRegistration;
}
