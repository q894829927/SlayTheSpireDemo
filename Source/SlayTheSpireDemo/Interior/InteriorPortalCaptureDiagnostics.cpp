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
		TEXT("When non-zero, log the SceneCapture depth-0 visibility gate and capture transform every game frame.\n")
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

			// This exactly mirrors the current SceneCapture depth-0 gate in RenderViews:
			// the player must see Entry, then the mapped virtual view must also see Entry
			// before Views[0] is added and CaptureScene() can be reached.
			const bool bCurrentCodeWouldCaptureDepth0 = bDirectVisible && bFirstVirtualSeesEntry;

			UE_LOG(LogTemp, Display,
				TEXT("PortalCaptureDiag Frame=%llu Entry=%s PlayerPitch=%.3f VirtualPitch=%.3f DirectVisible=%d FirstVirtualSeesEntry=%d CurrentCodeWouldCaptureDepth0=%d CaptureTransformMatchesExpected=%d CapturePitch=%.3f RecursionDepth=%d ClipMode=%s"),
				GFrameCounter,
				Entry == PortalSystem->BluePortal ? TEXT("Blue") : TEXT("Orange"),
				POV.Rotation.Pitch,
				ExpectedDepth0.Rotator().Pitch,
				bDirectVisible ? 1 : 0,
				bFirstVirtualSeesEntry ? 1 : 0,
				bCurrentCodeWouldCaptureDepth0 ? 1 : 0,
				bCaptureTransformMatchesExpected ? 1 : 0,
				bHasCapture ? CaptureTransform.Rotator().Pitch : 0.0f,
				PortalSystem->RecursionDepth,
				PortalSystem->RenderClipMode == EInteriorPortalRenderClipMode::NativeClipPlane
					? TEXT("NativeClipPlane") : TEXT("ObliqueFallback"));
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
