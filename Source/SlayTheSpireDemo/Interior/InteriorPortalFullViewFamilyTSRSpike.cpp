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

namespace InteriorPortalFullViewFamilyTSRSpikePrivate
{
	TAutoConsoleVariable<float> CVarTSRPrimaryFraction(
		TEXT("portal.FullViewFamilyTSRPrimaryFraction"),
		0.67f,
		TEXT("STEP 1B.10B fixed primary-resolution fraction for the secondary TSR spike. Clamped to [0.5,1.0] and latched at start."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarTSRDiagnostics(
		TEXT("portal.FullViewFamilyTSRDiagnostics"),
		0,
		TEXT("STEP 1B.10B diagnostics. 0=quiet, 1=periodic TSR/screen-percentage/jitter telemetry."),
		ECVF_Default);

	TAtomic<uint64> GLastExtractionFrame { 0 };
	TAtomic<float> GMeasuredSecondaryPreExposure { 1.0f };
	TAtomic<int32> GExtractionInputWidth { 0 };
	TAtomic<int32> GExtractionInputHeight { 0 };
	TAtomic<int32> GObservedAAMethod { -1 };
	TAtomic<float> GLastTemporalJitterX { 0.0f };
	TAtomic<float> GLastTemporalJitterY { 0.0f };
	TAtomic<bool> GTemporalJitterObserved { false };

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

	class FPortalTSRExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalTSRExtractionExtension(
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

			// UE 5.8 TSR executes after DOF. The earlier BeforeDOF hook used by
			// STEP 1B.6-1B.10A is therefore pre-TSR and cannot prove that the
			// PortalTexture itself receives temporal reconstruction. Tonemap is a
			// reliable post-TSR, pre-tonemap linear-HDR hook. Secondary motion blur
			// and DOF are disabled for this spike so the main view remains the owner
			// of those later presentation effects.
			if (Pass != ISceneViewExtension::EPostProcessingPass::Tonemap
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
					GObservedAAMethod.Store(static_cast<int32>(View.AntiAliasingMethod));
					GExtractionInputWidth.Store(SceneColor.ViewRect.Width());
					GExtractionInputHeight.Store(SceneColor.ViewRect.Height());

					const FVector2D TemporalJitter = View.ViewMatrices.GetTemporalAAJitter();
					GLastTemporalJitterX.Store(static_cast<float>(TemporalJitter.X));
					GLastTemporalJitterY.Store(static_cast<float>(TemporalJitter.Y));
					if (FMath::Abs(TemporalJitter.X) > 1.0e-8
						|| FMath::Abs(TemporalJitter.Y) > 1.0e-8)
					{
						GTemporalJitterObserved.Store(true);
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

	bool PortalFrameChanged(const FTransform& A, const FTransform& B)
	{
		return !A.GetLocation().Equals(B.GetLocation(), 0.01)
			|| !A.GetRotation().Equals(B.GetRotation(), 1.0e-5);
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

	class FTSRPortalProducer
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
					TEXT("PortalTSRSpike: requires a playable world with RendererBackend=SceneCapture."));
				return false;
			}

			// Keep the validated 1B.9/1B.10A producer intact and stop it before the
			// independent TSR spike takes ownership of the same endpoint target.
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("portal.StopFullViewFamilyRealtimeSpike"));
				GEngine->Exec(World, TEXT("portal.ClearFullViewFamilyMainCompositionSpike"));
			}

			ActiveWorld = World;
			PrimaryResolutionFraction = FMath::Clamp(
				CVarTSRPrimaryFraction.GetValueOnGameThread(), 0.5f, 1.0f);
			SecondaryViewState.Allocate(World->GetFeatureLevel());
			CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
			CompositionExtension->SetEnabled(true);

			GLastExtractionFrame.Store(0);
			GMeasuredSecondaryPreExposure.Store(1.0f);
			GExtractionInputWidth.Store(0);
			GExtractionInputHeight.Store(0);
			GObservedAAMethod.Store(-1);
			GLastTemporalJitterX.Store(0.0f);
			GLastTemporalJitterY.Store(0.0f);
			GTemporalJitterObserved.Store(false);
			SetPreExposureRebaseCVars(true, 1.0f);
			InvalidateTemporalHistory(TEXT("producer start"));

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FTSRPortalProducer::OnWorldPostActorTick);
			bRunning = true;
			Status = TEXT("RUNNING_TSR");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalTSRSpike: started. PrimaryFraction=%.3f ExtractionPass=Tonemap."),
				PrimaryResolutionFraction);
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

			FlushRenderingCommands();
			CompositionExtension.Reset();
			SecondaryViewState.Destroy();
			ReleaseFinalScratch();
			SetPreExposureRebaseCVars(false, 1.0f);
			ActiveWorld.Reset();
			bRunning = false;
			Status = TEXT("STOPPED");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalTSRSpike: stopped. Submitted=%llu Skipped=%llu CameraCuts=%llu ContinuousHistoryFrames=%llu JitterObserved=%d."),
				FramesSubmitted,
				FramesSkipped,
				CameraCutCount,
				ContinuousHistoryFrames,
				GTemporalJitterObserved.Load() ? 1 : 0);
		}

		bool IsRunning() const
		{
			return bRunning;
		}

		void DumpReport() const
		{
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalTSRSpike: report written. Submitted=%llu Skipped=%llu LastExtractionFrame=%llu CameraCuts=%llu ContinuousHistoryFrames=%llu Input=%dx%d ObservedAA=%d Jitter=(%.9g,%.9g) JitterObserved=%d"),
				FramesSubmitted,
				FramesSkipped,
				GLastExtractionFrame.Load(),
				CameraCutCount,
				ContinuousHistoryFrames,
				GExtractionInputWidth.Load(),
				GExtractionInputHeight.Load(),
				GObservedAAMethod.Load(),
				GLastTemporalJitterX.Load(),
				GLastTemporalJitterY.Load(),
				GTemporalJitterObserved.Load() ? 1 : 0);
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

		void InvalidateTemporalHistory(const FString& Reason)
		{
			bTemporalHistoryValid = false;
			LastCameraCutReason = Reason;
		}

		void SkipFrame(const FString& InStatus, const FString& HistoryReason)
		{
			++FramesSkipped;
			Status = InStatus;
			if (CompositionExtension)
			{
				CompositionExtension->ClearRequest();
			}
			InvalidateTemporalHistory(HistoryReason);
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

		bool DetermineCameraCut(
			const int32 EndpointIndex,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			FString& OutReason) const
		{
			if (!bTemporalHistoryValid)
			{
				OutReason = TEXT("history invalid / first visible frame");
				return true;
			}
			if (LastHistoryEndpointIndex != EndpointIndex)
			{
				OutReason = TEXT("visible endpoint changed");
				return true;
			}
			if (LastHistoryTargetSize != TargetSize)
			{
				OutReason = TEXT("render target size changed");
				return true;
			}
			if (PortalFrameChanged(LastHistoryEntryFrame, EntryFrame)
				|| PortalFrameChanged(LastHistoryExitFrame, ExitFrame))
			{
				OutReason = TEXT("portal logical frame changed");
				return true;
			}
			OutReason = TEXT("continuous history");
			return false;
		}

		void CommitTemporalHistory(
			const int32 EndpointIndex,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			const bool bCameraCut,
			const FString& CameraCutReason)
		{
			bLastCameraCut = bCameraCut;
			LastCameraCutReason = CameraCutReason;
			if (bCameraCut)
			{
				++CameraCutCount;
			}
			else
			{
				++ContinuousHistoryFrames;
			}

			bTemporalHistoryValid = true;
			LastHistoryEndpointIndex = EndpointIndex;
			LastHistoryTargetSize = TargetSize;
			LastHistoryEntryFrame = EntryFrame;
			LastHistoryExitFrame = ExitFrame;
		}

		void SubmitFrame(UWorld* World)
		{
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager
				|| PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture
				|| !PortalSystem->IsLinked())
			{
				SkipFrame(TEXT("WAITING_FOR_LINKED_SCENECAPTURE_PORTALS"), TEXT("portal pair/player/backend unavailable"));
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				SkipFrame(TEXT("WAITING_FOR_PROJECTION_DATA"), TEXT("projection data unavailable"));
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
				SkipFrame(TEXT("WAITING_FOR_VIEW_RECT"), TEXT("view rect unavailable"));
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
				SkipFrame(TEXT("NO_VISIBLE_PORTAL"), TEXT("portal left the visible set"));
				return;
			}

			const FTransform EntryFrame = Entry->GetLogicalFrame();
			const FTransform ExitFrame = Exit->GetLogicalFrame();
			FInteriorPortalRenderRequest Request;
			if (!FInteriorPortalRenderRequest::Build(
				EndpointIndex, EndpointIndex, 0,
				PlayerView, EntryFrame, ExitFrame,
				Entry->HalfWidth, Entry->HalfHeight,
				PlayerViewProjection, PlayerRect,
				ProjectionData.ProjectionMatrix,
				ProjectionData.IsPerspectiveProjection(),
				ProjectionData.GetNearPlaneFromProjectionMatrix(),
				PortalSystem->ClipPlaneBias,
				1, Request)
				|| !Request.IsValid() || !IsFiniteTransform(Request.VirtualView))
			{
				SkipFrame(TEXT("REQUEST_BUILD_FAILED"), TEXT("portal request build failed"));
				return;
			}

			const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
			const int32 Height = FMath::Max(144,
				FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
			const FIntPoint TargetSize(Width, Height);

			if (LastTargetSize != FIntPoint::ZeroValue && LastTargetSize != TargetSize)
			{
				FlushRenderingCommands();
				InvalidateTemporalHistory(TEXT("viewport / target resize"));
			}

			Entry->EnsureTargets(Width, Height, 1);
			UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets.IsValidIndex(0)
				? Entry->RenderTargets[0] : nullptr;
			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			if (!PortalTarget || !PortalTargetResource || !EnsureFinalScratch(World, TargetSize))
			{
				SkipFrame(TEXT("RENDER_TARGET_UNAVAILABLE"), TEXT("portal/scratch render target unavailable"));
				return;
			}

			FRenderTarget* FinalScratchResource = FinalScratch->GameThread_GetRenderTargetResource();
			if (!FinalScratchResource || !World->Scene)
			{
				SkipFrame(TEXT("SCENE_OR_SCRATCH_UNAVAILABLE"), TEXT("scene or scratch resource unavailable"));
				return;
			}

			TSharedRef<FPortalTSRExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FPortalTSRExtractionExtension>(
					World, PortalTargetResource);

			FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->EngineShowFlags
				: FEngineShowFlags(ESFIM_Game);
			ShowFlags.SetEyeAdaptation(false);
			ShowFlags.SetMotionBlur(false);
			ShowFlags.SetDepthOfField(false);
			ShowFlags.SetTemporalAA(true);
			ShowFlags.SetScreenPercentage(true);

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
				new FLegacyScreenPercentageDriver(ViewFamily, PrimaryResolutionFraction));
			ViewFamily.ViewExtensions.Add(ExtractionExtension);

			FString CameraCutReason;
			const bool bCameraCut = DetermineCameraCut(
				EndpointIndex, TargetSize, EntryFrame, ExitFrame, CameraCutReason);

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
			SceneView->bCameraCut = bCameraCut;
			SceneView->bAllowTemporalJitter = true;
			SceneView->AntiAliasingMethod = EAntiAliasingMethod::AAM_TSR;
			SceneView->SetupAntiAliasingMethod();
			SceneView->GlobalClippingPlane = Request.ExitClipPlane;
			SceneView->StartFinalPostprocessSettings(Request.ViewLocation);
			SceneView->OverridePostProcessSettings(
				POV.PostProcessSettings, POV.PostProcessBlendWeight, true);
			SceneView->FinalPostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
			SceneView->FinalPostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
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

			Request.PortalRenderTarget = PortalTargetResource;
			CompositionExtension->PublishRequest(Request);

			++FramesSubmitted;
			Status = TEXT("RUNNING_TSR");
			LastTargetSize = TargetSize;
			LastEndpointIndex = EndpointIndex;
			LastPlayerView = PlayerView;
			LastVirtualView = Request.VirtualView;
			LastBounds = Request.ProjectedBounds;
			CommitTemporalHistory(
				EndpointIndex, TargetSize, EntryFrame, ExitFrame, bCameraCut, CameraCutReason);

			if (CVarTSRDiagnostics.GetValueOnGameThread() != 0
				&& (FramesSubmitted == 1 || (FramesSubmitted % 60) == 0))
			{
				const int32 ExpectedPrimaryWidth = FMath::Max(
					1, FMath::RoundToInt(TargetSize.X * PrimaryResolutionFraction));
				const int32 ExpectedPrimaryHeight = FMath::Max(
					1, FMath::RoundToInt(TargetSize.Y * PrimaryResolutionFraction));
				UE_LOG(LogTemp, Display,
					TEXT("PortalTSRSpike Frame=%llu Submitted=%llu Endpoint=%d PrimaryFraction=%.3f ExpectedPrimary=%dx%d Output=%dx%d ExtractionInput=%dx%d ObservedAA=%d Jitter=(%.9g,%.9g) JitterObserved=%d CameraCut=%d CameraCuts=%llu ContinuousHistoryFrames=%llu CutReason=%s"),
					GFrameCounter,
					FramesSubmitted,
					EndpointIndex,
					PrimaryResolutionFraction,
					ExpectedPrimaryWidth, ExpectedPrimaryHeight,
					TargetSize.X, TargetSize.Y,
					GExtractionInputWidth.Load(), GExtractionInputHeight.Load(),
					GObservedAAMethod.Load(),
					GLastTemporalJitterX.Load(), GLastTemporalJitterY.Load(),
					GTemporalJitterObserved.Load() ? 1 : 0,
					bCameraCut ? 1 : 0,
					CameraCutCount,
					ContinuousHistoryFrames,
					*CameraCutReason);
			}
		}

		void WriteReport() const
		{
			const int32 ExpectedPrimaryWidth = FMath::Max(
				1, FMath::RoundToInt(LastTargetSize.X * PrimaryResolutionFraction));
			const int32 ExpectedPrimaryHeight = FMath::Max(
				1, FMath::RoundToInt(LastTargetSize.Y * PrimaryResolutionFraction));

			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"framesSubmitted\":%llu,\n")
				TEXT("  \"framesSkipped\":%llu,\n")
				TEXT("  \"lastExtractionFrame\":%llu,\n")
				TEXT("  \"lastEndpointIndex\":%d,\n")
				TEXT("  \"targetSize\":[%d,%d],\n")
				TEXT("  \"primaryResolutionFraction\":%.6f,\n")
				TEXT("  \"expectedPrimarySize\":[%d,%d],\n")
				TEXT("  \"extractionPass\":\"Tonemap (post-TSR / pre-tonemap linear HDR)\",\n")
				TEXT("  \"extractionInputSize\":[%d,%d],\n")
				TEXT("  \"observedAAMethod\":%d,\n")
				TEXT("  \"temporalJitterObserved\":%s,\n")
				TEXT("  \"lastTemporalJitter\":[%.9g,%.9g],\n")
				TEXT("  \"secondaryPreExposure\":%.9g,\n")
				TEXT("  \"temporalHistoryValid\":%s,\n")
				TEXT("  \"cameraCutCount\":%llu,\n")
				TEXT("  \"continuousHistoryFrames\":%llu,\n")
				TEXT("  \"lastCameraCut\":%s,\n")
				TEXT("  \"lastCameraCutReason\":\"%s\",\n")
				TEXT("  \"playerLocation\":[%.6f,%.6f,%.6f],\n")
				TEXT("  \"virtualLocation\":[%.6f,%.6f,%.6f],\n")
				TEXT("  \"projectedBounds\":[%.6f,%.6f,%.6f,%.6f],\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.10B fixed-fraction TSR, temporal-jitter and post-TSR linear-HDR extraction feasibility only; no dynamic resolution, recursion, main depth/stencil continuity or production performance acceptance claim\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				FramesSubmitted,
				FramesSkipped,
				GLastExtractionFrame.Load(),
				LastEndpointIndex,
				LastTargetSize.X, LastTargetSize.Y,
				PrimaryResolutionFraction,
				ExpectedPrimaryWidth, ExpectedPrimaryHeight,
				GExtractionInputWidth.Load(), GExtractionInputHeight.Load(),
				GObservedAAMethod.Load(),
				GTemporalJitterObserved.Load() ? TEXT("true") : TEXT("false"),
				GLastTemporalJitterX.Load(), GLastTemporalJitterY.Load(),
				GMeasuredSecondaryPreExposure.Load(),
				bTemporalHistoryValid ? TEXT("true") : TEXT("false"),
				CameraCutCount,
				ContinuousHistoryFrames,
				bLastCameraCut ? TEXT("true") : TEXT("false"),
				*LastCameraCutReason.ReplaceCharWithEscapedChar(),
				LastPlayerView.GetLocation().X, LastPlayerView.GetLocation().Y, LastPlayerView.GetLocation().Z,
				LastVirtualView.GetLocation().X, LastVirtualView.GetLocation().Y, LastVirtualView.GetLocation().Z,
				LastBounds.Min.X, LastBounds.Min.Y, LastBounds.Max.X, LastBounds.Max.Y);

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalFullViewFamilyTSRSpike.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		bool bTemporalHistoryValid = false;
		bool bLastCameraCut = true;
		float PrimaryResolutionFraction = 1.0f;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		FSceneViewStateReference SecondaryViewState;
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 FramesSubmitted = 0;
		uint64 FramesSkipped = 0;
		uint64 CameraCutCount = 0;
		uint64 ContinuousHistoryFrames = 0;
		FString Status = TEXT("STOPPED");
		FString LastCameraCutReason = TEXT("not started");
		FIntPoint LastTargetSize = FIntPoint::ZeroValue;
		int32 LastEndpointIndex = INDEX_NONE;
		FTransform LastPlayerView = FTransform::Identity;
		FTransform LastVirtualView = FTransform::Identity;
		InteriorPortalMath::FPortalScreenBounds LastBounds;
		int32 LastHistoryEndpointIndex = INDEX_NONE;
		FIntPoint LastHistoryTargetSize = FIntPoint::ZeroValue;
		FTransform LastHistoryEntryFrame = FTransform::Identity;
		FTransform LastHistoryExitFrame = FTransform::Identity;
	};

	TUniquePtr<FTSRPortalProducer> GTSRProducer;

	void StartTSRSpike()
	{
		if (GTSRProducer && GTSRProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalTSRSpike: already running."));
			return;
		}

		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalTSRSpike: PIE/Game world unavailable."));
			return;
		}

		GTSRProducer = MakeUnique<FTSRPortalProducer>();
		if (!GTSRProducer->Start(World))
		{
			GTSRProducer.Reset();
		}
	}

	void StopTSRSpike()
	{
		if (GTSRProducer)
		{
			GTSRProducer->Stop();
			GTSRProducer.Reset();
		}
		else
		{
			SetPreExposureRebaseCVars(false, 1.0f);
			UE_LOG(LogTemp, Display, TEXT("PortalTSRSpike: not running."));
		}
	}

	void DumpTSRSpike()
	{
		if (GTSRProducer)
		{
			GTSRProducer->DumpReport();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalTSRSpike: not running; no live report to dump."));
		}
	}

	FAutoConsoleCommand GStartTSRSpikeCommand(
		TEXT("portal.StartFullViewFamilyTSRSpike"),
		TEXT("Start STEP 1B.10B fixed-fraction TSR/post-temporal extraction spike. Requires RendererBackend=SceneCapture."),
		FConsoleCommandDelegate::CreateStatic(&StartTSRSpike));

	FAutoConsoleCommand GStopTSRSpikeCommand(
		TEXT("portal.StopFullViewFamilyTSRSpike"),
		TEXT("Stop STEP 1B.10B TSR spike and release persistent resources."),
		FConsoleCommandDelegate::CreateStatic(&StopTSRSpike));

	FAutoConsoleCommand GDumpTSRSpikeCommand(
		TEXT("portal.DumpFullViewFamilyTSRSpike"),
		TEXT("Write STEP 1B.10B TSR/screen-percentage/jitter telemetry to Saved/AutomationReports."),
		FConsoleCommandDelegate::CreateStatic(&DumpTSRSpike));
}
