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
#include "LegacyScreenPercentageDriver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> GFullViewFamilyMainCompositionExtension;

	class FPortalMainCompositionExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalMainCompositionExtractionExtension(
			const FAutoRegister& AutoRegister, UWorld* InWorld, FRenderTarget* InExtractionTarget)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, ExtractionTarget(InExtractionTarget)
		{
		}

		bool WasExecuted() const
		{
			return bExecuted.Load();
		}

		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
		{
			(void)InViewFamily;
		}

		virtual void PreRenderViewFamily_RenderThread(
			FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
		{
			(void)GraphBuilder;
			(void)InViewFamily;
		}

		virtual void PostRenderViewFamily_RenderThread(
			FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
		{
			(void)GraphBuilder;
			(void)InViewFamily;
		}

		virtual void SubscribeToPostProcessingPass(
			ISceneViewExtension::EPostProcessingPass Pass,
			const FSceneView& InView,
			FPostProcessingPassDelegateArray& InOutPassCallbacks,
			bool bIsPassEnabled) override
		{
			(void)bIsPassEnabled;
			if (Pass != ISceneViewExtension::EPostProcessingPass::BeforeDOF
				|| !InView.Family || !InView.Family->bAdditionalViewFamily
				|| !ExtractionTarget)
			{
				return;
			}

			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[this](FRDGBuilder& GraphBuilder, const FSceneView& View,
					const FPostProcessMaterialInputs& Inputs)
				{
					const FScreenPassTextureSlice SceneColorSlice =
						Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
					FScreenPassTexture SceneColor =
						FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneColorSlice);
					if (!SceneColor.IsValid() || !ExtractionTarget)
					{
						return SceneColor;
					}

					FRDGTextureRef ExtractionTexture =
						ExtractionTarget->GetRenderTargetTexture(GraphBuilder);
					if (!ExtractionTexture)
					{
						return SceneColor;
					}

					GraphBuilder.UseInternalAccessMode(ExtractionTexture);
					AddDrawTexturePass(
						GraphBuilder,
						View,
						SceneColor.Texture,
						ExtractionTexture,
						SceneColor.ViewRect.Min,
						SceneColor.ViewRect.Size(),
						FIntPoint::ZeroValue,
						ExtractionTexture->Desc.Extent,
						TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
					GraphBuilder.UseExternalAccessMode(ExtractionTexture, ERHIAccess::SRVMask);
					bExecuted.Store(true);
					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return ExtractionTarget != nullptr
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		FRenderTarget* ExtractionTarget = nullptr;
		TAtomic<bool> bExecuted { false };
	};

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

	void WriteMainCompositionResult(
		const FString& Status,
		const FString& Detail,
		const FInteriorPortalRenderRequest* Request,
		const FIntPoint TargetSize,
		const bool bBeforeDOFExecuted,
		const bool bCompositionArmed)
	{
		const FString RequestJson = Request
			? FString::Printf(
				TEXT("{\"portalId\":%d,\"endpointIndex\":%d,\"viewLocation\":[%.6f,%.6f,%.6f],")
				TEXT("\"exitClipPlane\":[%.8f,%.8f,%.8f,%.8f],")
				TEXT("\"projectedBounds\":[%.6f,%.6f,%.6f,%.6f]}"),
				Request->PortalId,
				Request->EndpointIndex,
				Request->ViewLocation.X, Request->ViewLocation.Y, Request->ViewLocation.Z,
				Request->ExitClipPlane.X, Request->ExitClipPlane.Y,
				Request->ExitClipPlane.Z, Request->ExitClipPlane.W,
				Request->ProjectedBounds.Min.X, Request->ProjectedBounds.Min.Y,
				Request->ProjectedBounds.Max.X, Request->ProjectedBounds.Max.Y)
			: TEXT("null");

		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"detail\":\"%s\",\n")
			TEXT("  \"rendererPath\":\"full transformed FSceneViewFamily -> secondary BeforeDOF HDR target -> main BeforeDOF aperture composition\",\n")
			TEXT("  \"targetSize\":[%d,%d],\n")
			TEXT("  \"beforeDOFCallbackExecuted\":%s,\n")
			TEXT("  \"mainCompositionArmed\":%s,\n")
			TEXT("  \"claimBoundary\":\"One-shot static main-view composition feasibility only; no per-frame producer, exposure parity, temporal, stencil/depth or recursion acceptance claim\",\n")
			TEXT("  \"request\":%s\n")
			TEXT("}\n"),
			*Status.ReplaceCharWithEscapedChar(),
			*Detail.ReplaceCharWithEscapedChar(),
			TargetSize.X, TargetSize.Y,
			bBeforeDOFExecuted ? TEXT("true") : TEXT("false"),
			bCompositionArmed ? TEXT("true") : TEXT("false"),
			*RequestJson);

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
			TEXT("PortalFullViewFamilyMainCompositionSpike.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		FFileHelper::SaveStringToFile(Json, *ReportPath);
	}

	void ClearPortalFullViewFamilyMainCompositionSpike()
	{
		if (GFullViewFamilyMainCompositionExtension)
		{
			GFullViewFamilyMainCompositionExtension->SetEnabled(false);
			GFullViewFamilyMainCompositionExtension->ClearRequest();
			FlushRenderingCommands();
			GFullViewFamilyMainCompositionExtension.Reset();
		}
		UE_LOG(LogTemp, Display, TEXT("PortalFullViewFamilyMainCompositionSpike: cleared."));
	}

	void RunPortalFullViewFamilyMainCompositionSpike()
	{
		UWorld* World = FindPortalSpikeWorld();
		AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		if (!World || !PortalSystem || !Player || !Player->PlayerCameraManager)
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Playable world, portal system, or player camera unavailable"),
				nullptr, FIntPoint::ZeroValue, false, false);
			return;
		}
		if (!PortalSystem->IsLinked())
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Portal pair is not linked/placed"),
				nullptr, FIntPoint::ZeroValue, false, false);
			return;
		}
		if (PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture)
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"),
				TEXT("Set RendererBackend=SceneCapture before this isolated composition spike so no other main-view composition backend competes with it"),
				nullptr, FIntPoint::ZeroValue, false, false);
			UE_LOG(LogTemp, Error,
				TEXT("PortalFullViewFamilyMainCompositionSpike: RendererBackend must be SceneCapture for this isolated proof."));
			return;
		}

		ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
		FSceneViewProjectionData ProjectionData;
		if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
			|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Player projection data unavailable"),
				nullptr, FIntPoint::ZeroValue, false, false);
			return;
		}

		const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
		if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Player constrained view rect is empty"),
				nullptr, FIntPoint::ZeroValue, false, false);
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
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("No linked portal aperture is visible in the player view"),
				nullptr, FIntPoint::ZeroValue, false, false);
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
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Immutable transformed portal request could not be built"),
				nullptr, FIntPoint::ZeroValue, false, false);
			return;
		}

		const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
		const int32 Height = FMath::Max(144,
			FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
		const FIntPoint TargetSize(Width, Height);

		// RenderTargets[0] is persistent on the endpoint and becomes the static
		// pre-tonemap portal texture consumed by the main-view composition proof.
		Entry->EnsureTargets(Width, Height, 1);
		UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets.IsValidIndex(0)
			? Entry->RenderTargets[0] : nullptr;
		FRenderTarget* PortalTargetResource = PortalTarget
			? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
		if (!PortalTarget || !PortalTargetResource)
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Persistent endpoint HDR render target unavailable"),
				&Request, TargetSize, false, false);
			return;
		}

		TStrongObjectPtr<UTextureRenderTarget2D> FinalScratch(
			NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
		if (!FinalScratch.IsValid())
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Could not allocate full-view final scratch target"),
				&Request, TargetSize, false, false);
			return;
		}
		FinalScratch->RenderTargetFormat = RTF_RGBA16f;
		FinalScratch->ClearColor = FLinearColor::Black;
		FinalScratch->bAutoGenerateMips = false;
		FinalScratch->InitCustomFormat(Width, Height, PF_FloatRGBA, true);
		FinalScratch->UpdateResourceImmediate(true);
		FRenderTarget* FinalScratchResource = FinalScratch->GameThread_GetRenderTargetResource();
		if (!FinalScratchResource || !World->Scene)
		{
			WriteMainCompositionResult(
				TEXT("BLOCKED"), TEXT("Full-view scratch resource or scene unavailable"),
				&Request, TargetSize, false, false);
			return;
		}

		TSharedPtr<FPortalMainCompositionExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
			FSceneViewExtensions::NewExtension<FPortalMainCompositionExtractionExtension>(
				World, PortalTargetResource);

		FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
			? GEngine->GameViewport->EngineShowFlags
			: FEngineShowFlags(ESFIM_Game);
		ShowFlags.SetEyeAdaptation(false);
		ShowFlags.SetMotionBlur(false);
		ShowFlags.SetTemporalAA(false);
		ShowFlags.SetScreenPercentage(false);

		FSceneViewStateReference ViewState;
		ViewState.Allocate(World->GetFeatureLevel());
		FSceneViewFamilyContext ViewFamily(
			FSceneViewFamily::ConstructionValues(FinalScratchResource, World->Scene, ShowFlags)
				.SetTime(World->GetTime())
				.SetResolveScene(true)
				.SetRealtimeUpdate(true)
				.SetAdditionalViewFamily(true));
		ViewFamily.EngineShowFlags = ShowFlags;
		ViewFamily.SceneCaptureSource = SCS_FinalColorHDR;
		ViewFamily.ViewMode = VMI_Lit;
		ViewFamily.SetScreenPercentageInterface(
			new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
		ViewInitOptions.ViewOrigin = Request.ViewLocation;
		ViewInitOptions.ViewLocation = Request.ViewLocation;
		ViewInitOptions.ViewRotation = Request.VirtualView.Rotator();
		ViewInitOptions.ViewRotationMatrix = Request.ViewRotationMatrix;
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

		FCanvas Canvas(FinalScratchResource, nullptr, World, World->GetFeatureLevel(),
			FCanvas::CDM_DeferDrawing, 1.0f);
		IRendererModule& RendererModule =
			FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
		RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);
		FlushRenderingCommands();

		const bool bBeforeDOFExecuted = ExtractionExtension.IsValid()
			&& ExtractionExtension->WasExecuted();
		if (!bBeforeDOFExecuted)
		{
			WriteMainCompositionResult(
				TEXT("BEFOREDOF_EXTRACTION_FAILED"),
				TEXT("Secondary full renderer completed but the BeforeDOF extraction callback did not execute"),
				&Request, TargetSize, false, false);
			return;
		}

		ClearPortalFullViewFamilyMainCompositionSpike();
		Request.PortalRenderTarget = PortalTargetResource;
		GFullViewFamilyMainCompositionExtension =
			FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
		GFullViewFamilyMainCompositionExtension->SetEnabled(true);
		GFullViewFamilyMainCompositionExtension->PublishRequest(Request);

		WriteMainCompositionResult(
			TEXT("MAIN_COMPOSITION_ARMED"),
			TEXT("A static full-renderer BeforeDOF HDR portal texture is ready and the dedicated main-view BeforeDOF aperture compositor is armed. Keep the camera still for this one-shot proof; use portal.ClearFullViewFamilyMainCompositionSpike when finished."),
			&Request, TargetSize, true, true);

		UE_LOG(LogTemp, Display,
			TEXT("PortalFullViewFamilyMainCompositionSpike: armed endpoint=%d bounds=(%.4f,%.4f)-(%.4f,%.4f). Keep camera still; clear with portal.ClearFullViewFamilyMainCompositionSpike."),
			EndpointIndex,
			Request.ProjectedBounds.Min.X, Request.ProjectedBounds.Min.Y,
			Request.ProjectedBounds.Max.X, Request.ProjectedBounds.Max.Y);
	}

	FAutoConsoleCommand GRunPortalFullViewFamilyMainCompositionSpike(
		TEXT("portal.RunFullViewFamilyMainCompositionSpike"),
		TEXT("Render one transformed full secondary view, extract BeforeDOF HDR SceneColor into the endpoint target, then arm the existing main-view BeforeDOF aperture compositor. Keep camera still; one-shot static proof."),
		FConsoleCommandDelegate::CreateStatic(&RunPortalFullViewFamilyMainCompositionSpike));

	FAutoConsoleCommand GClearPortalFullViewFamilyMainCompositionSpike(
		TEXT("portal.ClearFullViewFamilyMainCompositionSpike"),
		TEXT("Disable and release the one-shot full-view main composition proof."),
		FConsoleCommandDelegate::CreateStatic(&ClearPortalFullViewFamilyMainCompositionSpike));
}
