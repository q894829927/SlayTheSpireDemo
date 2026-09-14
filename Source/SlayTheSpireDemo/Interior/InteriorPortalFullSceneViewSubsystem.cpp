#include "InteriorPortalFullSceneViewSubsystem.h"

#include "InteriorPlayerController.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"

#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "UnrealClient.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPortalFullSceneViewSpike(
		TEXT("portal.FullSceneViewSpike"),
		0,
		TEXT("STEP 1B.5 full transformed SceneView feasibility spike. 0=off, 1=render one lit portal view and compose it BeforeDOF."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPortalFullSceneViewDiagnostics(
		TEXT("portal.FullSceneViewDiagnostics"),
		0,
		TEXT("Write Saved/PortalFullSceneViewDiagnostics.json while the STEP 1B.5 spike is active. 0=off, 1=on."),
		ECVF_Default);

	bool FullSceneViewSpikeEnabled()
	{
		return CVarPortalFullSceneViewSpike.GetValueOnGameThread() != 0;
	}

	bool FullSceneViewDiagnosticsEnabled()
	{
		return CVarPortalFullSceneViewDiagnostics.GetValueOnGameThread() != 0;
	}

	FMatrix PortalPlayerViewProjection(const FTransform& View, const FMatrix& ProjectionMatrix)
	{
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));
		return FTranslationMatrix(-View.GetLocation())
			* FInverseRotationMatrix(View.Rotator())
			* PortalViewPlanes
			* ProjectionMatrix;
	}

	void HidePortalPrimitives(const AInteriorPortalSystem* PortalSystem, FSceneViewInitOptions& ViewInitOptions)
	{
		if (!PortalSystem)
		{
			return;
		}

		for (const AInteriorPortal* Portal : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
		{
			if (!IsValid(Portal))
			{
				continue;
			}

			TArray<UPrimitiveComponent*> Components;
			Portal->GetComponents<UPrimitiveComponent>(Components);
			for (const UPrimitiveComponent* Component : Components)
			{
				if (IsValid(Component))
				{
					ViewInitOptions.HiddenPrimitives.Add(Component->GetPrimitiveSceneId());
				}
			}
		}
	}
}

void UInteriorPortalFullSceneViewSubsystem::Deactivate()
{
	if (CompositionExtension)
	{
		CompositionExtension->SetEnabled(false);
		CompositionExtension->ClearRequest();
	}
	LastSubmittedFrame = MAX_uint64;
}

void UInteriorPortalFullSceneViewSubsystem::Deinitialize()
{
	Deactivate();
	CompositionExtension.Reset();
	Super::Deinitialize();
}

void UInteriorPortalFullSceneViewSubsystem::Render(
	AInteriorPlayerController* Player,
	AInteriorPortalSystem* PortalSystem)
{
	if (!FullSceneViewSpikeEnabled())
	{
		Deactivate();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !IsValid(Player) || !IsValid(PortalSystem)
		|| !PortalSystem->IsLinked() || !Player->PlayerCameraManager
		|| !World->Scene || LastSubmittedFrame == GFrameCounter)
	{
		if (!World || !IsValid(Player) || !IsValid(PortalSystem) || !PortalSystem->IsLinked())
		{
			Deactivate();
		}
		return;
	}

	ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
		|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		PortalSystem->FidelityDiagnosticStatus = TEXT("STEP1B.5 FullSceneView blocked: player projection data unavailable");
		return;
	}

	const FIntRect PlayerViewRect = ProjectionData.GetConstrainedViewRect();
	if (PlayerViewRect.Width() <= 0 || PlayerViewRect.Height() <= 0)
	{
		PortalSystem->FidelityDiagnosticStatus = TEXT("STEP1B.5 FullSceneView blocked: empty player view rect");
		return;
	}

	const int32 Width = FMath::Clamp(
		FMath::RoundToInt(PlayerViewRect.Width() * PortalSystem->ResolutionScale), 256, 2560);
	const int32 Height = FMath::Max(
		144,
		FMath::RoundToInt(
			Width * double(PlayerViewRect.Height()) / FMath::Max(1, PlayerViewRect.Width())));

	const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
	const FTransform PlayerView(POV.Rotation, POV.Location);
	const FMatrix PlayerViewProjection = PortalPlayerViewProjection(
		PlayerView, ProjectionData.ProjectionMatrix);

	AInteriorPortal* SelectedEntry = nullptr;
	AInteriorPortal* SelectedExit = nullptr;
	FInteriorPortalRenderRequest Request;
	FString Blocker = TEXT("no visible portal aperture");
	int32 EndpointIndex = 0;

	for (AInteriorPortal* Entry : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
	{
		InteriorPortalMath::FPortalScreenBounds DirectBounds;
		const bool bVisible = IsValid(Entry)
			&& InteriorPortalMath::ProjectPortalApertureToScreenBounds(
				Entry->GetLogicalFrame(), Entry->HalfWidth, Entry->HalfHeight,
				PlayerViewProjection, PlayerViewRect, DirectBounds,
				ProjectionData.IsPerspectiveProjection(),
				ProjectionData.GetNearPlaneFromProjectionMatrix());
		if (!bVisible || !DirectBounds.bHasVisiblePortion)
		{
			++EndpointIndex;
			continue;
		}

		AInteriorPortal* Exit = Entry == PortalSystem->BluePortal
			? PortalSystem->OrangePortal.Get()
			: PortalSystem->BluePortal.Get();
		if (!IsValid(Exit))
		{
			Blocker = TEXT("paired exit endpoint is invalid");
			++EndpointIndex;
			continue;
		}

		FInteriorPortalRenderRequest Candidate;
		if (!FInteriorPortalRenderRequest::Build(
			EndpointIndex,
			EndpointIndex,
			0,
			PlayerView,
			Entry->GetLogicalFrame(),
			Exit->GetLogicalFrame(),
			Entry->HalfWidth,
			Entry->HalfHeight,
			PlayerViewProjection,
			PlayerViewRect,
			ProjectionData.ProjectionMatrix,
			ProjectionData.IsPerspectiveProjection(),
			ProjectionData.GetNearPlaneFromProjectionMatrix(),
			PortalSystem->ClipPlaneBias,
			1,
			Candidate))
		{
			Blocker = TEXT("immutable transformed-view request could not be built");
			++EndpointIndex;
			continue;
		}

		// This first full-renderer spike deliberately keeps the already-proven
		// oblique exit-plane contract. Do not silently render an unclipped room and
		// confuse a lighting proof with support-wall leakage.
		if (!Candidate.bExitClipEncodedInProjection)
		{
			Blocker = TEXT("exit clip could not be encoded into the transformed projection");
			++EndpointIndex;
			continue;
		}

		SelectedEntry = Entry;
		SelectedExit = Exit;
		Request = Candidate;
		break;
	}

	if (!SelectedEntry || !SelectedExit || !Request.IsValid())
	{
		if (CompositionExtension)
		{
			CompositionExtension->ClearRequest();
		}
		PortalSystem->FidelityDiagnosticStatus = FString::Printf(
			TEXT("STEP1B.5 FullSceneView blocked: %s"), *Blocker);
		return;
	}

	SelectedEntry->EnsureTargets(Width, Height, 1);
	UTextureRenderTarget2D* Target = SelectedEntry->RenderTargets.IsValidIndex(0)
		? SelectedEntry->RenderTargets[0]
		: nullptr;
	FTextureRenderTargetResource* TargetResource = Target
		? Target->GameThread_GetRenderTargetResource()
		: nullptr;
	if (!Target || !TargetResource)
	{
		PortalSystem->FidelityDiagnosticStatus = TEXT("STEP1B.5 FullSceneView blocked: HDR render target unavailable");
		return;
	}

	// The secondary family is deliberately stateless for the first feasibility
	// proof. It asks the normal renderer for lit scene stages, but final display
	// transforms remain owned by the player view after BeforeDOF composition.
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.SetScreenPercentage(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetTemporalAA(false);
	ShowFlags.SetEyeAdaptation(false);
	ShowFlags.SetLocalExposure(false);
	ShowFlags.SetTonemapper(false);
	ShowFlags.SetColorGrading(false);
	ShowFlags.SetBloom(false);

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(TargetResource, World->Scene, ShowFlags)
			.SetRealtimeUpdate(true)
			.SetResolveScene(true)
			.SetAdditionalViewFamily(true));
	ViewFamily.bIsHDR = true;
	ViewFamily.bIsMainViewFamily = false;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Target->SizeX, Target->SizeY));
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.ViewOrigin = Request.ViewLocation;
	ViewInitOptions.ViewLocation = Request.ViewLocation;
	ViewInitOptions.ViewRotation = Request.VirtualView.Rotator();
	ViewInitOptions.ViewRotationMatrix = Request.ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = Request.ProjectionMatrix;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.FOV = POV.FOV;
	ViewInitOptions.DesiredFOV = POV.DesiredFOV > 0.0f ? POV.DesiredFOV : POV.FOV;
	ViewInitOptions.SceneViewStateInterface = nullptr;
	ViewInitOptions.ExposureSceneViewStateInterface = nullptr;
	ViewInitOptions.bDisableGameScreenPercentage = true;
	ViewInitOptions.PlayerIndex = 0;
	HidePortalPrimitives(PortalSystem, ViewInitOptions);

	FSceneView* PortalView = new FSceneView(ViewInitOptions);
	PortalView->StartFinalPostprocessSettings(Request.ViewLocation);
	PortalView->OverridePostProcessSettings(
		POV.PostProcessSettings,
		POV.PostProcessBlendWeight,
		false);
	PortalView->EndFinalPostprocessSettings(ViewInitOptions);
	ViewFamily.Views.Add(PortalView);

	FCanvas Canvas(
		TargetResource,
		nullptr,
		World,
		World->GetFeatureLevel(),
		FCanvas::CDM_DeferDrawing);
	IRendererModule& RendererModule = FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
	RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

	if (!CompositionExtension)
	{
		CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
	}
	CompositionExtension->SetEnabled(true);
	Request.PortalRenderTarget = TargetResource;
	CompositionExtension->PublishRequest(Request);
	LastSubmittedFrame = GFrameCounter;

	PortalSystem->FidelityDiagnosticStatus = FString::Printf(
		TEXT("STEP1B.5 FullSceneView submitted: endpoint=%d %dx%d; full renderer family -> HDR RT -> BeforeDOF; stateless temporal policy"),
		Request.EndpointIndex, Target->SizeX, Target->SizeY);

	if (FullSceneViewDiagnosticsEnabled()
		&& (LastDiagnosticsFrame == 0 || GFrameCounter - LastDiagnosticsFrame >= 30))
	{
		const FString Json = FString::Printf(
			TEXT("{\n"
				"  \"frame\":%llu,\n"
				"  \"spike\":\"STEP1B.5 FullSceneView\",\n"
				"  \"enabled\":true,\n"
				"  \"rendererHook\":\"IRendererModule::BeginRenderingViewFamily\",\n"
				"  \"viewFamily\":\"standalone additional full FSceneViewFamily\",\n"
				"  \"endpoint\":%d,\n"
				"  \"recursionLevel\":0,\n"
				"  \"targetSize\":[%d,%d],\n"
				"  \"exitClip\":\"ObliqueProjectionEncoded\",\n"
				"  \"temporalHistory\":\"disabled for first feasibility spike\",\n"
				"  \"eyeAdaptation\":false,\n"
				"  \"localExposure\":false,\n"
				"  \"tonemapper\":false,\n"
				"  \"colorGrading\":false,\n"
				"  \"composition\":\"BeforeDOF analytic aperture\",\n"
				"  \"playerFinalDisplayAuthority\":true,\n"
				"  \"mainDepthStencilContinuity\":\"not implemented\",\n"
				"  \"trueScissor\":\"not implemented\",\n"
				"  \"status\":\"submitted; GPU lit result requires visual validation\"\n"
				"}\n"),
			GFrameCounter,
			Request.EndpointIndex,
			Target->SizeX,
			Target->SizeY);
		FFileHelper::SaveStringToFile(
			Json,
			*(FPaths::ProjectSavedDir() + TEXT("PortalFullSceneViewDiagnostics.json")));
		LastDiagnosticsFrame = GFrameCounter;
	}
}
