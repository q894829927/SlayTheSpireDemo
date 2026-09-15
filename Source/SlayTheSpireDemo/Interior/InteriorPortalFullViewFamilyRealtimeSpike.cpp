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

namespace InteriorPortalFullViewFamilyRealtimeSpikePrivate
{
	TAutoConsoleVariable<int32> CVarRealtimeSpikeDiagnostics(
		TEXT("portal.FullViewFamilyRealtimeDiagnostics"),
		0,
		TEXT("STEP 1B.9 diagnostics. 0=quiet, 1=log periodic per-frame full-view producer state."),
		ECVF_Default);

	TAtomic<uint64> GLastExtractionFrame { 0 };
	TAtomic<float> GMeasuredSecondaryPreExposure { 1.0f };

	void SetPreExposureRebaseCVars(const bool bEnabled, const float SecondaryPreExposure = 1.0f)
	{
		if (IConsoleVariable* Secondary = IConsoleManager::Get().FindConsoleVariable(TEXT("portal.SecondaryPreExposure")))
		{
			Secondary->Set(FMath::Max(SecondaryPreExposure, UE_SMALL_NUMBER), ECVF_SetByCode);
		}
		if (IConsoleVariable* Rebase = IConsoleManager::Get().FindConsoleVariable(TEXT("portal.PreExposureRebase")))
		{
			Rebase->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
		}
	}

	class FPortalRealtimeExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalRealtimeExtractionExtension(
			const FAutoRegister& AutoRegister, UWorld* InWorld, FRenderTarget* InExtractionTarget)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, ExtractionTarget(InExtractionTarget)
		{
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

					const float MeasuredPreExposure = View.State
						? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					GMeasuredSecondaryPreExposure.Store(MeasuredPreExposure);

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
					GLastExtractionFrame.Store(GFrameCounter);
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
	};

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

	UWorld* FindPlayableWorld()
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

	class FRealtimePortalProducer
	{
	public:
		bool Start(UWorld* World)
		{
			if (bRunning || !World || !World->Scene)
			{
				return false;
			}

			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			if (!PortalSystem || PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture)
			{
				UE_LOG(LogTemp, Error,
					TEXT("PortalRealtimeSpike: requires a playable world with RendererBackend=SceneCapture."));
				return false;
			}

			// Remove the previous one-shot 1B.7/1B.8 compositor if it is still armed.
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("portal.ClearFullViewFamilyMainCompositionSpike"));
			}

			ActiveWorld = World;
			SecondaryViewState.Allocate(World->GetFeatureLevel());
			CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
			CompositionExtension->SetEnabled(true);
			GMeasuredSecondaryPreExposure.Store(1.0f);
			GLastExtractionFrame.Store(0);
			SetPreExposureRebaseCVars(true, 1.0f);

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FRealtimePortalProducer::OnWorldPostActorTick);
			bRunning = true;
			Status = TEXT("RUNNING");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalRealtimeSpike: started. Move/look normally; stop with portal.StopFullViewFamilyRealtimeSpike."));
			return true;
		}

		void Stop()
		{
			if (!bRunning && !WorldPostActorTickHandle.IsValid())
			{
				return;
			}

			if (WorldPostActorTickHandle.IsValid())
			{
				FWorldDelegates::OnWorldPostActorTick.Remove(WorldPostActorTickHandle);
				WorldPostActorTickHandle.Reset();
			}

			if (CompositionExtension)
			{
				CompositionExtension->SetEnabled(false);
				CompositionExtension->ClearRequest();
			}

			// Stop is allowed to block. Per-frame submission itself never flushes.
			FlushRenderingCommands();
			CompositionExtension.Reset();
			SecondaryViewState.Destroy();
			ReleaseFinalScratch();
			SetPreExposureRebaseCVars(false, 1.0f);
			ActiveWorld.Reset();
			bRunning = false;
			Status = TEXT("STOPPED");
			WriteReport();
			UE_LOG(LogTemp, Display, TEXT("PortalRealtimeSpike: stopped."));
		}

		bool IsRunning() const
		{
			return bRunning;
		}

		void DumpReport() const
		{
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalRealtimeSpike: report written. Submitted=%llu Skipped=%llu LastExtractionFrame=%llu"),
				FramesSubmitted, FramesSkipped, GLastExtractionFrame.Load());
		}

	private:
		void OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			(void)TickType;
			(void)DeltaSeconds;
			if (!bRunning)
			{
				return;
			}
			if (!ActiveWorld.IsValid())
			{
				Stop();
				return;
			}
			if (World != ActiveWorld.Get())
			{
				return;
			}

			SubmitFrame(World);
		}

		void ClearPublishedRequest()
		{
			if (CompositionExtension)
			{
				CompositionExtension->ClearRequest();
			}
		}

		bool EnsureFinalScratch(UWorld* World, const FIntPoint TargetSize)
		{
			if (!World || TargetSize.X <= 0 || TargetSize.Y <= 0)
			{
				return false;
			}
			if (FinalScratch && FinalScratchSize == TargetSize)
			{
				return true;
			}

			// Resolution changes are rare and may invalidate resources referenced by
			// an earlier queued secondary frame, so synchronize only on resize.
			if (FinalScratch)
			{
				FlushRenderingCommands();
				ReleaseFinalScratch();
			}

			FinalScratch = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!FinalScratch)
			{
				return false;
			}
			FinalScratch->AddToRoot();
			FinalScratch->RenderTargetFormat = RTF_RGBA16f;
			FinalScratch->ClearColor = FLinearColor::Black;
			FinalScratch->bAutoGenerateMips = false;
			FinalScratch->InitCustomFormat(TargetSize.X, TargetSize.Y, PF_FloatRGBA, true);
			FinalScratch->UpdateResourceImmediate(true);
			FinalScratchSize = TargetSize;
			return FinalScratch->GameThread_GetRenderTargetResource() != nullptr;
		}

		void ReleaseFinalScratch()
		{
			if (FinalScratch)
			{
				FinalScratch->RemoveFromRoot();
				FinalScratch = nullptr;
			}
			FinalScratchSize = FIntPoint::ZeroValue;
		}

		void SubmitFrame(UWorld* World)
		{
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager
				|| PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture
				|| !PortalSystem->IsLinked())
			{
				++FramesSkipped;
				Status = TEXT("WAITING_FOR_LINKED_SCENECAPTURE_PORTALS");
				ClearPublishedRequest();
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				++FramesSkipped;
				Status = TEXT("WAITING_FOR_PROJECTION_DATA");
				ClearPublishedRequest();
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
				++FramesSkipped;
				Status = TEXT("WAITING_FOR_VIEW_RECT");
				ClearPublishedRequest();
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
				++FramesSkipped;
				Status = TEXT("NO_VISIBLE_PORTAL");
				ClearPublishedRequest();
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
				++FramesSkipped;
				Status = TEXT("REQUEST_BUILD_FAILED");
				ClearPublishedRequest();
				return;
			}

			const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
			const int32 Height = FMath::Max(144,
				FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
			const FIntPoint TargetSize(Width, Height);

			if (LastTargetSize != FIntPoint::ZeroValue && LastTargetSize != TargetSize)
			{
				// The endpoint target can also be referenced by an earlier main frame.
				// Synchronize only on viewport resize before allowing EnsureTargets to recreate it.
				FlushRenderingCommands();
			}
			Entry->EnsureTargets(Width, Height, 1);
			UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets.IsValidIndex(0)
				? Entry->RenderTargets[0] : nullptr;
			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			if (!PortalTarget || !PortalTargetResource || !EnsureFinalScratch(World, TargetSize))
			{
				++FramesSkipped;
				Status = TEXT("RENDER_TARGET_UNAVAILABLE");
				ClearPublishedRequest();
				return;
			}

			FRenderTarget* FinalScratchResource = FinalScratch->GameThread_GetRenderTargetResource();
			if (!FinalScratchResource || !World->Scene)
			{
				++FramesSkipped;
				Status = TEXT("SCENE_OR_SCRATCH_UNAVAILABLE");
				ClearPublishedRequest();
				return;
			}

			TSharedRef<FPortalRealtimeExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FPortalRealtimeExtractionExtension>(
					World, PortalTargetResource);

			FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->EngineShowFlags
				: FEngineShowFlags(ESFIM_Game);
			ShowFlags.SetEyeAdaptation(false);
			ShowFlags.SetMotionBlur(false);
			ShowFlags.SetTemporalAA(false);
			ShowFlags.SetScreenPercentage(false);

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
			ViewFamily.ViewExtensions.Add(ExtractionExtension);

			FSceneViewInitOptions ViewInitOptions;
			ViewInitOptions.ViewFamily = &ViewFamily;
			ViewInitOptions.SceneViewStateInterface = SecondaryViewState.GetReference();
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
			// Temporal acceptance is intentionally deferred. Treat every secondary
			// frame as a camera cut so stale history cannot masquerade as tracking success.
			SceneView->bCameraCut = true;
			SceneView->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
			SceneView->GlobalClippingPlane = Request.ExitClipPlane;
			SceneView->StartFinalPostprocessSettings(Request.ViewLocation);
			SceneView->OverridePostProcessSettings(
				POV.PostProcessSettings, POV.PostProcessBlendWeight, true);
			SceneView->FinalPostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
			SceneView->FinalPostProcessSettings.DynamicGlobalIlluminationMethod =
				EDynamicGlobalIlluminationMethod::Lumen;
			SceneView->FinalPostProcessSettings.bOverride_ReflectionMethod = true;
			SceneView->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
			SceneView->EndFinalPostprocessSettings(ViewInitOptions);

			const float SecondaryPreExposure = FMath::Max(
				GMeasuredSecondaryPreExposure.Load(), UE_SMALL_NUMBER);
			SetPreExposureRebaseCVars(true, SecondaryPreExposure);

			FCanvas Canvas(FinalScratchResource, nullptr, World, World->GetFeatureLevel(),
				FCanvas::CDM_DeferDrawing, 1.0f);
			IRendererModule& RendererModule =
				FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
			RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

			// The render command above is deliberately not flushed here. It is queued
			// before the ordinary main viewport renderer, so the secondary extraction
			// reaches the external portal target before the main BeforeDOF compositor
			// consumes that target, while the game thread remains asynchronous.
			Request.PortalRenderTarget = PortalTargetResource;
			CompositionExtension->PublishRequest(Request);

			++FramesSubmitted;
			Status = TEXT("RUNNING");
			LastTargetSize = TargetSize;
			LastEndpointIndex = EndpointIndex;
			LastPlayerView = PlayerView;
			LastVirtualView = Request.VirtualView;
			LastBounds = Request.ProjectedBounds;

			if (CVarRealtimeSpikeDiagnostics.GetValueOnGameThread() != 0
				&& (FramesSubmitted == 1 || (FramesSubmitted % 60) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalRealtimeSpike Frame=%llu Submitted=%llu Endpoint=%d Player=(%.1f,%.1f,%.1f) Virtual=(%.1f,%.1f,%.1f) Bounds=(%.4f,%.4f)-(%.4f,%.4f) SecondaryPreExposure=%.9g LastExtractionFrame=%llu"),
					GFrameCounter,
					FramesSubmitted,
					EndpointIndex,
					PlayerView.GetLocation().X, PlayerView.GetLocation().Y, PlayerView.GetLocation().Z,
					Request.ViewLocation.X, Request.ViewLocation.Y, Request.ViewLocation.Z,
					Request.ProjectedBounds.Min.X, Request.ProjectedBounds.Min.Y,
					Request.ProjectedBounds.Max.X, Request.ProjectedBounds.Max.Y,
					SecondaryPreExposure,
					GLastExtractionFrame.Load());
			}
		}

		void WriteReport() const
		{
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"framesSubmitted\":%llu,\n")
				TEXT("  \"framesSkipped\":%llu,\n")
				TEXT("  \"lastExtractionFrame\":%llu,\n")
				TEXT("  \"lastEndpointIndex\":%d,\n")
				TEXT("  \"targetSize\":[%d,%d],\n")
				TEXT("  \"secondaryPreExposure\":%.9g,\n")
				TEXT("  \"playerLocation\":[%.6f,%.6f,%.6f],\n")
				TEXT("  \"virtualLocation\":[%.6f,%.6f,%.6f],\n")
				TEXT("  \"projectedBounds\":[%.6f,%.6f,%.6f,%.6f],\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.9 per-frame single-visible-portal full-view producer tracking only; no temporal AA/TSR, recursion, depth/stencil continuity or production performance acceptance claim\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				FramesSubmitted,
				FramesSkipped,
				GLastExtractionFrame.Load(),
				LastEndpointIndex,
				LastTargetSize.X, LastTargetSize.Y,
				GMeasuredSecondaryPreExposure.Load(),
				LastPlayerView.GetLocation().X, LastPlayerView.GetLocation().Y, LastPlayerView.GetLocation().Z,
				LastVirtualView.GetLocation().X, LastVirtualView.GetLocation().Y, LastVirtualView.GetLocation().Z,
				LastBounds.Min.X, LastBounds.Min.Y, LastBounds.Max.X, LastBounds.Max.Y);

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalFullViewFamilyRealtimeSpike.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		FSceneViewStateReference SecondaryViewState;
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 FramesSubmitted = 0;
		uint64 FramesSkipped = 0;
		FString Status = TEXT("STOPPED");
		FIntPoint LastTargetSize = FIntPoint::ZeroValue;
		int32 LastEndpointIndex = INDEX_NONE;
		FTransform LastPlayerView = FTransform::Identity;
		FTransform LastVirtualView = FTransform::Identity;
		InteriorPortalMath::FPortalScreenBounds LastBounds;
	};

	TUniquePtr<FRealtimePortalProducer> GRealtimeProducer;

	void StartRealtimeSpike()
	{
		if (GRealtimeProducer && GRealtimeProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalRealtimeSpike: already running."));
			return;
		}

		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalRealtimeSpike: PIE/Game world unavailable."));
			return;
		}

		GRealtimeProducer = MakeUnique<FRealtimePortalProducer>();
		if (!GRealtimeProducer->Start(World))
		{
			GRealtimeProducer.Reset();
		}
	}

	void StopRealtimeSpike()
	{
		if (GRealtimeProducer)
		{
			GRealtimeProducer->Stop();
			GRealtimeProducer.Reset();
		}
		else
		{
			SetPreExposureRebaseCVars(false, 1.0f);
			UE_LOG(LogTemp, Display, TEXT("PortalRealtimeSpike: not running."));
		}
	}

	void DumpRealtimeSpike()
	{
		if (GRealtimeProducer)
		{
			GRealtimeProducer->DumpReport();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalRealtimeSpike: not running; no live report to dump."));
		}
	}

	FAutoConsoleCommand GStartRealtimeSpikeCommand(
		TEXT("portal.StartFullViewFamilyRealtimeSpike"),
		TEXT("Start STEP 1B.9 per-frame transformed full-view producer. Requires RendererBackend=SceneCapture."),
		FConsoleCommandDelegate::CreateStatic(&StartRealtimeSpike));

	FAutoConsoleCommand GStopRealtimeSpikeCommand(
		TEXT("portal.StopFullViewFamilyRealtimeSpike"),
		TEXT("Stop STEP 1B.9 per-frame transformed full-view producer and release its persistent resources."),
		FConsoleCommandDelegate::CreateStatic(&StopRealtimeSpike));

	FAutoConsoleCommand GDumpRealtimeSpikeCommand(
		TEXT("portal.DumpFullViewFamilyRealtimeSpike"),
		TEXT("Write the current STEP 1B.9 per-frame producer state to Saved/AutomationReports."),
		FConsoleCommandDelegate::CreateStatic(&DumpRealtimeSpike));
}
