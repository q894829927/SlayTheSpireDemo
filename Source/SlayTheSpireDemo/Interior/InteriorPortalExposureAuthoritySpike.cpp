#include "InteriorPortalRenderer.h"
#include "InteriorPortalFullFidelityBackend.h"
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
#include "SceneManagement.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalExposureAuthoritySpikePrivate
{
	TAutoConsoleVariable<int32> CVarSecondaryExposureAuthority(
		TEXT("portal.SecondaryExposureAuthority"),
		1,
		TEXT("STEP 1B.14D exposure-authority A/B. 0=secondary temporal ViewState owns exposure; 1=use the captured player-main ViewState through FSceneViewInitOptions::ExposureSceneViewStateInterface."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarExposureAuthorityDiagnostics(
		TEXT("portal.SecondaryExposureAuthorityDiagnostics"),
		1,
		TEXT("STEP 1B.14D diagnostics. 0=quiet, 1=periodic main/secondary exposure ownership and camera-cut telemetry."),
		ECVF_Default);

	TAtomic<float> GSceneColorPreExposure { 1.0f };
	TAtomic<float> GSecondaryTemporalPreExposure { 1.0f };
	TAtomic<float> GMainExposurePreExposure { 1.0f };
	TAtomic<bool> GMainExposureAuthorityActive { false };
	TAtomic<uint64> GExtractionFrames { 0 };
	TAtomic<uint64> GLastExtractionFrame { 0 };
	TAtomic<int32> GObservedAA { -1 };

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

	float ReadPrimaryFraction()
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(
			TEXT("portal.FullViewFamilyTSRPrimaryFraction")))
		{
			return FMath::Clamp(Var->GetFloat(), 0.5f, 1.0f);
		}
		return 0.67f;
	}

	class FMainExposureCaptureExtension final : public FWorldSceneViewExtension
	{
	public:
		FMainExposureCaptureExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
		{
		}

		void SetEnabled(const bool bInEnabled)
		{
			FScopeLock Lock(&Mutex);
			bEnabled = bInEnabled;
			if (!bEnabled)
			{
				MainState = nullptr;
				MainPreExposure = 1.0f;
				CaptureFrame = 0;
			}
		}

		bool GetSnapshot(FSceneViewStateInterface*& OutState, float& OutPreExposure, uint64& OutFrame) const
		{
			FScopeLock Lock(&Mutex);
			OutState = MainState;
			OutPreExposure = MainPreExposure;
			OutFrame = CaptureFrame;
			return bEnabled && MainState != nullptr;
		}

		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
		{
			if (!InteriorPortalRendering::IsPlayerMainView(InViewFamily, InView) || !InView.State)
			{
				return;
			}

			FScopeLock Lock(&Mutex);
			if (!bEnabled)
			{
				return;
			}
			MainState = InView.State;
			MainPreExposure = FMath::Max(InView.State->GetPreExposure(), UE_SMALL_NUMBER);
			CaptureFrame = GFrameCounter;
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

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			FScopeLock Lock(&Mutex);
			return bEnabled && FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		mutable FCriticalSection Mutex;
		bool bEnabled = true;
		FSceneViewStateInterface* MainState = nullptr;
		float MainPreExposure = 1.0f;
		uint64 CaptureFrame = 0;
	};

	class FExposureAuthorityExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FExposureAuthorityExtractionExtension(
			const FAutoRegister& AutoRegister,
			UWorld* InWorld,
			FRenderTarget* InExtractionTarget)
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

					const FSceneViewStateInterface* TemporalState = View.State;
					const float TemporalPreExposure = TemporalState
						? FMath::Max(TemporalState->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					// UpdatePreExposure writes the actual color domain into View.State,
					// even when eye adaptation history is owned by a different state.
					const float SceneColorPreExposure = TemporalPreExposure;

					GSecondaryTemporalPreExposure.Store(TemporalPreExposure);
					GSceneColorPreExposure.Store(SceneColorPreExposure);
					GObservedAA.Store(static_cast<int32>(View.AntiAliasingMethod));
					const uint64 Count = GExtractionFrames.Load() + 1;
					GExtractionFrames.Store(Count);
					GLastExtractionFrame.Store(GFrameCounter);

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

	class FExposureAuthorityProducer
	{
	public:
		bool Start(UWorld* World)
		{
			if (bRunning || !World || !World->Scene)
			{
				return false;
			}

			if (InteriorPortalFullFidelityBackend::IsRunning())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("PortalExposureAuthority: refusing to start while accepted FullFidelity backend owns rendering."));
				return false;
			}
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			if (!PortalSystem || PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture)
			{
				UE_LOG(LogTemp, Error,
					TEXT("PortalExposureAuthority: requires PIE/Game with RendererBackend=SceneCapture."));
				return false;
			}

			if (GEngine)
			{
				GEngine->Exec(World, TEXT("portal.StopFullViewFamilyTSRSpike"));
				GEngine->Exec(World, TEXT("portal.StopSecondaryEyeAdaptationSpike"));
				GEngine->Exec(World, TEXT("portal.StopFullViewFamilyRealtimeSpike"));
				GEngine->Exec(World, TEXT("portal.ClearFullViewFamilyMainCompositionSpike"));
			}

			ActiveWorld = World;
			PrimaryResolutionFraction = ReadPrimaryFraction();
			SecondaryViewState.Allocate(World->GetFeatureLevel());
			MainExposureCapture = FSceneViewExtensions::NewExtension<FMainExposureCaptureExtension>(World);
			MainExposureCapture->SetEnabled(true);
			CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
			CompositionExtension->SetEnabled(true);

			GSceneColorPreExposure.Store(1.0f);
			GSecondaryTemporalPreExposure.Store(1.0f);
			GMainExposurePreExposure.Store(1.0f);
			GMainExposureAuthorityActive.Store(false);
			GExtractionFrames.Store(0);
			GLastExtractionFrame.Store(0);
			GObservedAA.Store(-1);
			SetPreExposureRebaseCVars(true, 1.0f);
			bTemporalHistoryValid = false;
			LastCameraCutReason = TEXT("producer start");

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FExposureAuthorityProducer::OnWorldPostActorTick);
			bRunning = true;
			UE_LOG(LogTemp, Display,
				TEXT("PortalExposureAuthority: START Policy=%d PrimaryFraction=%.3f. Policy 1 keeps TSR history on the secondary ViewState while sourcing exposure from the captured player-main ViewState."),
				CVarSecondaryExposureAuthority.GetValueOnGameThread() != 0 ? 1 : 0,
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
			if (MainExposureCapture)
			{
				MainExposureCapture->SetEnabled(false);
			}
			FlushRenderingCommands();
			CompositionExtension.Reset();
			MainExposureCapture.Reset();
			SecondaryViewState.Destroy();
			ReleaseScratch();
			SetPreExposureRebaseCVars(false, 1.0f);
			ActiveWorld.Reset();
			bRunning = false;
			WriteReport(TEXT("STOPPED"));
			UE_LOG(LogTemp, Display,
				TEXT("PortalExposureAuthority: STOP Submitted=%llu Skipped=%llu CameraCuts=%llu SceneColorPreExposure=%.9g MainPreExposure=%.9g SecondaryTemporalPreExposure=%.9g"),
				FramesSubmitted, FramesSkipped, CameraCutCount,
				GSceneColorPreExposure.Load(), GMainExposurePreExposure.Load(),
				GSecondaryTemporalPreExposure.Load());
		}

		bool IsRunning() const { return bRunning; }

		void DumpReport() const
		{
			WriteReport(TEXT("RUNNING"));
			UE_LOG(LogTemp, Display,
				TEXT("PortalExposureAuthority Report Policy=%d MainAuthorityActive=%d Submitted=%llu Skipped=%llu CameraCuts=%llu LastCut=%d CutReason=%s ExtractionFrames=%llu LastFrame=%llu SceneColorPreExposure=%.9g MainPreExposure=%.9g SecondaryTemporalPreExposure=%.9g AA=%d Endpoint=%d"),
				CVarSecondaryExposureAuthority.GetValueOnGameThread() != 0 ? 1 : 0,
				GMainExposureAuthorityActive.Load() ? 1 : 0,
				FramesSubmitted, FramesSkipped, CameraCutCount,
				bLastCameraCut ? 1 : 0, *LastCameraCutReason,
				GExtractionFrames.Load(), GLastExtractionFrame.Load(),
				GSceneColorPreExposure.Load(), GMainExposurePreExposure.Load(),
				GSecondaryTemporalPreExposure.Load(), GObservedAA.Load(), LastEndpointIndex);
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
			if (InteriorPortalFullFidelityBackend::IsRunning())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("PortalExposureAuthority: stopping because accepted FullFidelity backend acquired rendering ownership."));
				Stop();
				return;
			}
			if (!ActiveWorld.IsValid())
			{
				Stop();
				return;
			}
			if (World == ActiveWorld.Get())
			{
				SubmitFrame(World);
			}
		}

		void SkipFrame(const TCHAR* Reason)
		{
			++FramesSkipped;
			if (CompositionExtension)
			{
				CompositionExtension->ClearRequest();
			}
			bTemporalHistoryValid = false;
			LastCameraCutReason = Reason;
		}

		bool EnsureScratch(const FIntPoint TargetSize)
		{
			if (TargetSize.X <= 0 || TargetSize.Y <= 0)
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
				ReleaseScratch();
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

		void ReleaseScratch()
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
				OutReason = TEXT("history invalid / re-entry");
				return true;
			}
			if (LastHistoryEndpointIndex != EndpointIndex)
			{
				OutReason = TEXT("visible endpoint changed");
				return true;
			}
			if (LastHistoryTargetSize != TargetSize)
			{
				OutReason = TEXT("target size changed");
				return true;
			}
			if (!LastHistoryEntryFrame.GetLocation().Equals(EntryFrame.GetLocation(), 0.01)
				|| !LastHistoryEntryFrame.GetRotation().Equals(EntryFrame.GetRotation(), 1.0e-5)
				|| !LastHistoryExitFrame.GetLocation().Equals(ExitFrame.GetLocation(), 0.01)
				|| !LastHistoryExitFrame.GetRotation().Equals(ExitFrame.GetRotation(), 1.0e-5))
			{
				OutReason = TEXT("portal logical frame changed");
				return true;
			}
			OutReason = TEXT("continuous history");
			return false;
		}

		void CommitHistory(
			const int32 EndpointIndex,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			const bool bCameraCut,
			const FString& Reason)
		{
			bLastCameraCut = bCameraCut;
			LastCameraCutReason = Reason;
			if (bCameraCut)
			{
				++CameraCutCount;
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
				SkipFrame(TEXT("portal pair/player/backend unavailable"));
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				SkipFrame(TEXT("projection data unavailable"));
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
				SkipFrame(TEXT("view rect unavailable"));
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
				InteriorPortalMath::FPortalScreenBounds Bounds;
				if (IsValid(Candidate)
					&& InteriorPortalMath::ProjectPortalApertureToScreenBounds(
						Candidate->GetLogicalFrame(), Candidate->HalfWidth, Candidate->HalfHeight,
						PlayerViewProjection, PlayerRect, Bounds,
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
				SkipFrame(TEXT("portal left visible set"));
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
				SkipFrame(TEXT("portal request build failed"));
				return;
			}

			const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
			const int32 Height = FMath::Max(144,
				FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
			const FIntPoint TargetSize(Width, Height);
			if (!EnsureScratch(TargetSize))
			{
				SkipFrame(TEXT("scratch unavailable"));
				return;
			}

			Entry->EnsureTargets(Width, Height, 1);
			UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets.IsValidIndex(0)
				? Entry->RenderTargets[0] : nullptr;
			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* ScratchResource = FinalScratch
				? FinalScratch->GameThread_GetRenderTargetResource() : nullptr;
			if (!PortalTargetResource || !ScratchResource || !World->Scene)
			{
				SkipFrame(TEXT("render target unavailable"));
				return;
			}

			FSceneViewStateInterface* MainExposureState = nullptr;
			float MainPreExposure = 1.0f;
			uint64 MainCaptureFrame = 0;
			const bool bMainCaptureValid = MainExposureCapture
				&& MainExposureCapture->GetSnapshot(MainExposureState, MainPreExposure, MainCaptureFrame);
			const bool bUseMainExposureAuthority =
				CVarSecondaryExposureAuthority.GetValueOnGameThread() != 0
				&& bMainCaptureValid
				&& MainExposureState != nullptr;
			GMainExposureAuthorityActive.Store(bUseMainExposureAuthority);
			GMainExposurePreExposure.Store(FMath::Max(MainPreExposure, UE_SMALL_NUMBER));

			TSharedRef<FExposureAuthorityExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FExposureAuthorityExtractionExtension>(
					World, PortalTargetResource);

			FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->EngineShowFlags
				: FEngineShowFlags(ESFIM_Game);
			ShowFlags.SetMotionBlur(false);
			ShowFlags.SetDepthOfField(false);
			ShowFlags.SetTemporalAA(true);
			ShowFlags.SetScreenPercentage(true);

			FSceneViewFamilyContext ViewFamily(
				FSceneViewFamily::ConstructionValues(ScratchResource, World->Scene, ShowFlags)
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
			if (bUseMainExposureAuthority)
			{
				ViewInitOptions.ExposureSceneViewStateInterface = MainExposureState;
			}
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
			SceneView->FinalPostProcessSettings.DynamicGlobalIlluminationMethod =
				EDynamicGlobalIlluminationMethod::Lumen;
			SceneView->FinalPostProcessSettings.bOverride_ReflectionMethod = true;
			SceneView->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
			SceneView->EndFinalPostprocessSettings(ViewInitOptions);

			const float RebaseSourcePreExposure = bUseMainExposureAuthority
				? FMath::Max(MainPreExposure, UE_SMALL_NUMBER)
				: FMath::Max(GSceneColorPreExposure.Load(), UE_SMALL_NUMBER);
			SetPreExposureRebaseCVars(true, RebaseSourcePreExposure);

			FCanvas Canvas(ScratchResource, nullptr, World, World->GetFeatureLevel(),
				FCanvas::CDM_DeferDrawing, 1.0f);
			IRendererModule& RendererModule =
				FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
			RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

			Request.PortalRenderTarget = PortalTargetResource;
			Request.PortalDepthRenderTarget = nullptr;
			CompositionExtension->PublishRequest(Request);

			++FramesSubmitted;
			LastEndpointIndex = EndpointIndex;
			CommitHistory(
				EndpointIndex, TargetSize, EntryFrame, ExitFrame,
				bCameraCut, CameraCutReason);

			if (CVarExposureAuthorityDiagnostics.GetValueOnGameThread() != 0
				&& (FramesSubmitted == 1 || (FramesSubmitted % 60) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalExposureAuthority Frame=%llu Submitted=%llu Endpoint=%d Policy=%d MainAuthorityActive=%d MainCaptureFrame=%llu MainPreExposure=%.9g SceneColorPreExposure=%.9g SecondaryTemporalPreExposure=%.9g CameraCut=%d CameraCuts=%llu CutReason=%s"),
					GFrameCounter, FramesSubmitted, EndpointIndex,
					CVarSecondaryExposureAuthority.GetValueOnGameThread() != 0 ? 1 : 0,
					bUseMainExposureAuthority ? 1 : 0,
					MainCaptureFrame,
					MainPreExposure,
					GSceneColorPreExposure.Load(),
					GSecondaryTemporalPreExposure.Load(),
					bCameraCut ? 1 : 0,
					CameraCutCount,
					*CameraCutReason);
			}
		}

		void WriteReport(const TCHAR* Status) const
		{
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"policy\":%d,\n")
				TEXT("  \"mainExposureAuthorityActive\":%s,\n")
				TEXT("  \"framesSubmitted\":%llu,\n")
				TEXT("  \"framesSkipped\":%llu,\n")
				TEXT("  \"cameraCutCount\":%llu,\n")
				TEXT("  \"lastCameraCutReason\":\"%s\",\n")
				TEXT("  \"lastEndpointIndex\":%d,\n")
				TEXT("  \"extractionFrames\":%llu,\n")
				TEXT("  \"lastExtractionFrame\":%llu,\n")
				TEXT("  \"sceneColorPreExposure\":%.9g,\n")
				TEXT("  \"mainPreExposure\":%.9g,\n")
				TEXT("  \"secondaryTemporalPreExposure\":%.9g,\n")
				TEXT("  \"aaMethod\":%d,\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.14D tests exposure authority decoupling only. Secondary SceneViewState still owns TSR/Lumen temporal history and retains the accepted camera-cut policy.\"\n")
				TEXT("}\n"),
				Status,
				CVarSecondaryExposureAuthority.GetValueOnAnyThread() != 0 ? 1 : 0,
				GMainExposureAuthorityActive.Load() ? TEXT("true") : TEXT("false"),
				FramesSubmitted, FramesSkipped, CameraCutCount,
				*LastCameraCutReason.ReplaceCharWithEscapedChar(),
				LastEndpointIndex,
				GExtractionFrames.Load(), GLastExtractionFrame.Load(),
				GSceneColorPreExposure.Load(), GMainExposurePreExposure.Load(),
				GSecondaryTemporalPreExposure.Load(), GObservedAA.Load());

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalExposureAuthoritySpike.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		bool bTemporalHistoryValid = false;
		bool bLastCameraCut = true;
		float PrimaryResolutionFraction = 0.67f;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		TSharedPtr<FMainExposureCaptureExtension, ESPMode::ThreadSafe> MainExposureCapture;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		FSceneViewStateReference SecondaryViewState;
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 FramesSubmitted = 0;
		uint64 FramesSkipped = 0;
		uint64 CameraCutCount = 0;
		int32 LastEndpointIndex = INDEX_NONE;
		int32 LastHistoryEndpointIndex = INDEX_NONE;
		FIntPoint LastHistoryTargetSize = FIntPoint::ZeroValue;
		FTransform LastHistoryEntryFrame = FTransform::Identity;
		FTransform LastHistoryExitFrame = FTransform::Identity;
		FString LastCameraCutReason = TEXT("not started");
	};

	TUniquePtr<FExposureAuthorityProducer> GProducer;

	void StartSpike()
	{
		if (GProducer && GProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalExposureAuthority: already running."));
			return;
		}
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("PortalExposureAuthority: enter PIE first."));
			return;
		}
		GProducer = MakeUnique<FExposureAuthorityProducer>();
		if (!GProducer->Start(World))
		{
			GProducer.Reset();
		}
	}

	void StopSpike()
	{
		if (GProducer)
		{
			GProducer->Stop();
			GProducer.Reset();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalExposureAuthority: not running."));
		}
	}

	void DumpSpike()
	{
		if (GProducer)
		{
			GProducer->DumpReport();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalExposureAuthority: not running."));
		}
	}

	FAutoConsoleCommand GStartCommand(
		TEXT("portal.StartExposureAuthoritySpike"),
		TEXT("Start STEP 1B.14D exposure-authority decoupling spike."),
		FConsoleCommandDelegate::CreateStatic(&StartSpike));

	FAutoConsoleCommand GStopCommand(
		TEXT("portal.StopExposureAuthoritySpike"),
		TEXT("Stop STEP 1B.14D exposure-authority decoupling spike."),
		FConsoleCommandDelegate::CreateStatic(&StopSpike));

	FAutoConsoleCommand GDumpCommand(
		TEXT("portal.DumpExposureAuthoritySpike"),
		TEXT("Write STEP 1B.14D exposure-authority telemetry to Saved/AutomationReports."),
		FConsoleCommandDelegate::CreateStatic(&DumpSpike));
}
