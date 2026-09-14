#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"

#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	UWorld* FindPortalSpikeWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
			{
				return World;
			}
		}
		return nullptr;
	}

	AInteriorPortalSystem* FindPortalSystem(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	bool IsFiniteTransform(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		return FMath::IsFinite(Location.X)
			&& FMath::IsFinite(Location.Y)
			&& FMath::IsFinite(Location.Z)
			&& Transform.GetRotation().IsNormalized();
	}

	void HidePortalPrimitives(const AInteriorPortal* Portal, FSceneViewInitOptions& ViewInitOptions)
	{
		if (!IsValid(Portal))
		{
			return;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Portal->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (IsValid(Primitive) && Primitive->IsRegistered())
			{
				ViewInitOptions.HiddenPrimitives.Add(Primitive->GetPrimitiveSceneId());
			}
		}
	}

	void WriteSpikeResult(const FString& Status, const FString& Detail,
		const FString& PngPath = FString(), const FString& ExrPath = FString(),
		const FInteriorPortalRenderRequest* Request = nullptr,
		const FIntPoint TargetSize = FIntPoint::ZeroValue)
	{
		const FString RequestJson = Request
			? FString::Printf(
				TEXT("{\"portalId\":%d,\"endpointIndex\":%d,\"viewLocation\":[%.6f,%.6f,%.6f],")
				TEXT("\"virtualRotation\":[%.8f,%.8f,%.8f,%.8f],\"exitClipPlane\":[%.8f,%.8f,%.8f,%.8f],")
				TEXT("\"projectedBounds\":[%.6f,%.6f,%.6f,%.6f]}"),
				Request->PortalId,
				Request->EndpointIndex,
				Request->ViewLocation.X, Request->ViewLocation.Y, Request->ViewLocation.Z,
				Request->VirtualView.GetRotation().X, Request->VirtualView.GetRotation().Y,
				Request->VirtualView.GetRotation().Z, Request->VirtualView.GetRotation().W,
				Request->ExitClipPlane.X, Request->ExitClipPlane.Y,
				Request->ExitClipPlane.Z, Request->ExitClipPlane.W,
				Request->ProjectedBounds.Min.X, Request->ProjectedBounds.Min.Y,
				Request->ProjectedBounds.Max.X, Request->ProjectedBounds.Max.Y)
			: TEXT("null");

		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"detail\":\"%s\",\n")
			TEXT("  \"rendererPath\":\"IRendererModule::BeginRenderingViewFamily + standalone FSceneViewFamilyContext\",\n")
			TEXT("  \"sceneViewIsSceneCapture\":false,\n")
			TEXT("  \"renderTargetFormat\":\"PF_FloatRGBA / RTF_RGBA16f\",\n")
			TEXT("  \"targetSize\":[%d,%d],\n")
			TEXT("  \"outputPNG\":\"%s\",\n")
			TEXT("  \"outputEXR\":\"%s\",\n")
			TEXT("  \"claimBoundary\":\"One-shot full renderer lighting feasibility only; no exposure, temporal, depth/stencil, recursion or production acceptance claim\",\n")
			TEXT("  \"request\":%s\n")
			TEXT("}\n"),
			*Status.ReplaceCharWithEscapedChar(),
			*Detail.ReplaceCharWithEscapedChar(),
			TargetSize.X, TargetSize.Y,
			*PngPath.ReplaceCharWithEscapedChar(),
			*ExrPath.ReplaceCharWithEscapedChar(),
			*RequestJson);

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("AutomationReports"), TEXT("PortalFullViewFamilySpike.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		FFileHelper::SaveStringToFile(Json, *ReportPath);
	}

	void RunPortalFullViewFamilySpike()
	{
		UWorld* World = FindPortalSpikeWorld();
		AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		if (!World || !PortalSystem || !Player || !Player->PlayerCameraManager)
		{
			WriteSpikeResult(TEXT("BLOCKED"),
				TEXT("PIE/Game world, AInteriorPortalSystem, player controller, or PlayerCameraManager unavailable"));
			UE_LOG(LogTemp, Error, TEXT("PortalFullViewFamilySpike: playable portal world unavailable."));
			return;
		}

		if (!PortalSystem->IsLinked())
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Portal pair is not linked/placed"));
			UE_LOG(LogTemp, Error, TEXT("PortalFullViewFamilySpike: place and link both portals first."));
			return;
		}

		ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
		FSceneViewProjectionData ProjectionData;
		if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
			|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Player projection data unavailable"));
			UE_LOG(LogTemp, Error, TEXT("PortalFullViewFamilySpike: player projection data unavailable."));
			return;
		}

		const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
		if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Player constrained view rect is empty"));
			return;
		}

		const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
		const FTransform PlayerView(POV.Rotation, POV.Location);
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		const FMatrix PlayerViewProjection =
			FTranslationMatrix(-PlayerView.GetLocation())
			* FInverseRotationMatrix(PlayerView.Rotator())
			* PortalViewPlanes
			* ProjectionData.ProjectionMatrix;

		AInteriorPortal* Entry = nullptr;
		AInteriorPortal* Exit = nullptr;
		int32 EndpointIndex = INDEX_NONE;
		int32 CandidateIndex = 0;
		for (AInteriorPortal* Candidate : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
		{
			InteriorPortalMath::FPortalScreenBounds CandidateBounds;
			if (IsValid(Candidate)
				&& InteriorPortalMath::ProjectPortalApertureToScreenBounds(
					Candidate->GetLogicalFrame(), Candidate->HalfWidth, Candidate->HalfHeight,
					PlayerViewProjection, PlayerRect, CandidateBounds,
					ProjectionData.IsPerspectiveProjection(),
					ProjectionData.GetNearPlaneFromProjectionMatrix()))
			{
				Entry = Candidate;
				Exit = Candidate == PortalSystem->BluePortal
					? PortalSystem->OrangePortal.Get() : PortalSystem->BluePortal.Get();
				EndpointIndex = CandidateIndex;
				break;
			}
			++CandidateIndex;
		}

		if (!IsValid(Entry) || !IsValid(Exit) || EndpointIndex == INDEX_NONE)
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("No linked portal aperture is visible in the player view"));
			UE_LOG(LogTemp, Error, TEXT("PortalFullViewFamilySpike: no visible linked portal."));
			return;
		}

		FInteriorPortalRenderRequest Request;
		if (!FInteriorPortalRenderRequest::Build(
			EndpointIndex, EndpointIndex, 0,
			PlayerView, Entry->GetLogicalFrame(), Exit->GetLogicalFrame(),
			Entry->HalfWidth, Entry->HalfHeight,
			PlayerViewProjection, PlayerRect,
			ProjectionData.ProjectionMatrix,
			ProjectionData.IsPerspectiveProjection(),
			ProjectionData.GetNearPlaneFromProjectionMatrix(),
			PortalSystem->ClipPlaneBias,
			1, Request)
			|| !Request.IsValid() || !IsFiniteTransform(Request.VirtualView))
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Immutable transformed portal render request could not be built"));
			UE_LOG(LogTemp, Error, TEXT("PortalFullViewFamilySpike: request construction failed."));
			return;
		}

		const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
		const int32 Height = FMath::Max(144,
			FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
		const FIntPoint TargetSize(Width, Height);

		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
		if (!Target.IsValid())
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Could not allocate transient RGBA16f render target"),
				FString(), FString(), &Request, TargetSize);
			return;
		}
		Target->RenderTargetFormat = RTF_RGBA16f;
		Target->ClearColor = FLinearColor::Black;
		Target->bAutoGenerateMips = false;
		Target->InitCustomFormat(Width, Height, PF_FloatRGBA, true);
		Target->UpdateResourceImmediate(true);

		FRenderTarget* TargetResource = Target->GameThread_GetRenderTargetResource();
		if (!TargetResource || !World->Scene)
		{
			WriteSpikeResult(TEXT("BLOCKED"), TEXT("Render target resource or scene interface unavailable"),
				FString(), FString(), &Request, TargetSize);
			return;
		}

		FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
			? GEngine->GameViewport->EngineShowFlags
			: FEngineShowFlags(ESFIM_Game);
		// Keep the first proof spatial + lighting focused. Temporal/exposure parity
		// are separate gates after the full renderer path is proven to produce lit data.
		ShowFlags.SetEyeAdaptation(false);
		ShowFlags.SetMotionBlur(false);
		ShowFlags.SetTemporalAA(false);
		ShowFlags.SetScreenPercentage(false);

		// The view state must outlive the view family because FSceneView retains a
		// pointer to it until FSceneViewFamilyContext destroys its owned views.
		FSceneViewStateReference ViewState;
		ViewState.Allocate(World->GetFeatureLevel());

		FSceneViewFamilyContext ViewFamily(
			FSceneViewFamily::ConstructionValues(TargetResource, World->Scene, ShowFlags)
				.SetTime(World->GetTime())
				.SetResolveScene(true)
				.SetRealtimeUpdate(true)
				.SetAdditionalViewFamily(true));
		ViewFamily.EngineShowFlags = ShowFlags;
		ViewFamily.SceneCaptureSource = SCS_FinalColorHDR;
		ViewFamily.ViewMode = VMI_Lit;

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
		ViewInitOptions.ViewOrigin = Request.ViewLocation;
		ViewInitOptions.ViewLocation = Request.ViewLocation;
		ViewInitOptions.ViewRotation = Request.VirtualView.Rotator();
		ViewInitOptions.ViewRotationMatrix = Request.ViewRotationMatrix;
		// Use the ordinary player projection here and let the FSceneView carry the
		// real logical exit GlobalClippingPlane. This deliberately differs from the
		// CRP's oblique-projection fallback and tests the full-view renderer contract.
		ViewInitOptions.ProjectionMatrix = ProjectionData.ProjectionMatrix;
		ViewInitOptions.FOV = POV.FOV;
		ViewInitOptions.DesiredFOV = POV.FOV;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::Transparent;
		ViewInitOptions.PlayerIndex = 0;
		ViewInitOptions.bUseFieldOfViewForLOD = true;
		HidePortalPrimitives(PortalSystem->BluePortal.Get(), ViewInitOptions);
		HidePortalPrimitives(PortalSystem->OrangePortal.Get(), ViewInitOptions);

		FSceneView* SceneView = new FSceneView(ViewInitOptions);
		ViewFamily.Views.Add(SceneView);
		SceneView->bIsGameView = true;
		SceneView->bIsSceneCapture = false;
		SceneView->bCameraCut = true;
		SceneView->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		SceneView->GlobalClippingPlane = Request.ExitClipPlane;
		SceneView->StartFinalPostprocessSettings(Request.ViewLocation);
		SceneView->OverridePostProcessSettings(POV.PostProcessSettings, POV.PostProcessBlendWeight, true);
		SceneView->FinalPostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		SceneView->FinalPostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		SceneView->FinalPostProcessSettings.bOverride_ReflectionMethod = true;
		SceneView->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
		SceneView->EndFinalPostprocessSettings(ViewInitOptions);

		FCanvas Canvas(TargetResource, nullptr, World, World->GetFeatureLevel(),
			FCanvas::CDM_DeferDrawing, 1.0f);
		IRendererModule& RendererModule =
			FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
		RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

		// This is intentionally a blocking, one-shot diagnostic. It is not the
		// production per-frame path. The flush makes the exported evidence belong
		// to the exact transformed view submitted above.
		FlushRenderingCommands();

		const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AutomationReports"));
		IFileManager::Get().MakeDirectory(*ReportDirectory, true);
		const FString PngPath = FPaths::Combine(ReportDirectory, TEXT("PortalFullViewFamilySpike.png"));
		const FString ExrPath = FPaths::Combine(ReportDirectory, TEXT("PortalFullViewFamilySpike.exr"));
		FImage Image;
		const bool bReadback = FImageUtils::GetRenderTargetImage(Target.Get(), Image);
		const bool bSavedPng = bReadback && FImageUtils::SaveImageByExtension(*PngPath, Image, 100);
		const bool bSavedExr = bReadback && FImageUtils::SaveImageByExtension(*ExrPath, Image, 0);

		const FString Detail = FString::Printf(
			TEXT("Full standalone FSceneViewFamily submitted from transformed endpoint %d; readback=%d png=%d exr=%d. ")
			TEXT("Inspect the PNG/EXR for direct lighting, shadows and indirect/reflection content. ")
			TEXT("This does not yet establish exposure or temporal parity."),
			EndpointIndex, bReadback ? 1 : 0, bSavedPng ? 1 : 0, bSavedExr ? 1 : 0);
		WriteSpikeResult(
			(bReadback && (bSavedPng || bSavedExr)) ? TEXT("OUTPUT_WRITTEN") : TEXT("RENDER_SUBMITTED_READBACK_FAILED"),
			Detail,
			bSavedPng ? PngPath : FString(),
			bSavedExr ? ExrPath : FString(),
			&Request,
			TargetSize);

		UE_LOG(LogTemp, Display,
			TEXT("PortalFullViewFamilySpike: %s PNG=%s EXR=%s"),
			*Detail,
			bSavedPng ? *PngPath : TEXT("<not written>"),
			bSavedExr ? *ExrPath : TEXT("<not written>"));
	}

	FAutoConsoleCommand GRunPortalFullViewFamilySpike(
		TEXT("portal.RunFullViewFamilySpike"),
		TEXT("Render one visible portal through a standalone full FSceneViewFamily and export PNG/EXR evidence. PIE/Game only; one-shot blocking diagnostic."),
		FConsoleCommandDelegate::CreateStatic(&RunPortalFullViewFamilySpike));
}
