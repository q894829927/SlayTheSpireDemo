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

namespace InteriorPortalSecondaryEyeAdaptationSpikePrivate
{
	TAutoConsoleVariable<int32> CVarSecondaryEyeAdaptation(
		TEXT("portal.SecondaryEyeAdaptation"),
		0,
		TEXT("STEP 1B.14C controlled secondary exposure-policy A/B. 0=legacy secondary EyeAdaptation off, 1=secondary EyeAdaptation on. Restart the spike after changing for a clean comparison."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSecondaryEyeAdaptationDiagnostics(
		TEXT("portal.SecondaryEyeAdaptationDiagnostics"),
		1,
		TEXT("STEP 1B.14C diagnostics. 0=quiet, 1=periodic secondary Tonemap exposure/post-process telemetry."),
		ECVF_Default);

	TAtomic<float> GMeasuredSecondaryPreExposure { 1.0f };
	TAtomic<int32> GMeasuredSecondaryAutoExposureMethod { -1 };
	TAtomic<int32> GMeasuredSecondaryDynamicGI { -1 };
	TAtomic<int32> GMeasuredSecondaryReflection { -1 };
	TAtomic<float> GMeasuredSecondaryIndirectLightingIntensity { 1.0f };
	TAtomic<int32> GMeasuredSecondaryAA { -1 };
	TAtomic<int32> GExtractionWidth { 0 };
	TAtomic<int32> GExtractionHeight { 0 };
	TAtomic<uint64> GExtractionFrames { 0 };
	TAtomic<uint64> GLastExtractionFrame { 0 };

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

	class FSecondaryEyeAdaptationExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FSecondaryEyeAdaptationExtractionExtension(
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

					const float PreExposure = View.State
						? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					GMeasuredSecondaryPreExposure.Store(PreExposure);
					GMeasuredSecondaryAutoExposureMethod.Store(
						static_cast<int32>(View.FinalPostProcessSettings.AutoExposureMethod));
					GMeasuredSecondaryDynamicGI.Store(
						static_cast<int32>(View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod));
					GMeasuredSecondaryReflection.Store(
						static_cast<int32>(View.FinalPostProcessSettings.ReflectionMethod));
					GMeasuredSecondaryIndirectLightingIntensity.Store(
						View.FinalPostProcessSettings.IndirectLightingIntensity);
					GMeasuredSecondaryAA.Store(static_cast<int32>(View.AntiAliasingMethod));
					GExtractionWidth.Store(SceneColor.ViewRect.Width());
					GExtractionHeight.Store(SceneColor.ViewRect.Height());
					const uint64 Count = GExtractionFrames.Load() + 1;
					GExtractionFrames.Store(Count);
					GLastExtractionFrame.Store(GFrameCounter);

					if (CVarSecondaryEyeAdaptationDiagnostics.GetValueOnRenderThread() != 0
						&& (Count == 1 || (Count % 60) == 0))
					{
						UE_LOG(LogTemp, Display,
							TEXT("PortalSecondaryEyeAdaptation Tonemap Frame=%llu Count=%llu Policy=%d PreExposure=%.9g AutoExposureMethod=%d AA=%d SceneRect=%dx%d DynamicGI=%d Reflection=%d IndirectLightingIntensity=%.6f"),
							GFrameCounter,
							Count,
							CVarSecondaryEyeAdaptation.GetValueOnRenderThread() != 0 ? 1 : 0,
							PreExposure,
							GMeasuredSecondaryAutoExposureMethod.Load(),
							GMeasuredSecondaryAA.Load(),
							SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
							GMeasuredSecondaryDynamicGI.Load(),
							GMeasuredSecondaryReflection.Load(),
							GMeasuredSecondaryIndirectLightingIntensity.Load());
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

	class FSecondaryEyeAdaptationProducer
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
					TEXT("PortalSecondaryEyeAdaptation: refusing to start while accepted FullFidelity backend owns rendering."));
				return false;
			}

			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			if (!PortalSystem || PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture)
			{
				UE_LOG(LogTemp, Error,
					TEXT("PortalSecondaryEyeAdaptation: requires PIE/Game with RendererBackend=SceneCapture."));
				return false;
			}

			if (GEngine)
			{
				GEngine->Exec(World, TEXT("portal.StopFullViewFamilyTSRSpike"));
				GEngine->Exec(World, TEXT("portal.StopFullViewFamilyRealtimeSpike"));
				GEngine->Exec(World, TEXT("portal.ClearFullViewFamilyMainCompositionSpike"));
			}

			ActiveWorld = World;
			PrimaryResolutionFraction = ReadPrimaryFraction();
			SecondaryViewState.Allocate(World->GetFeatureLevel());
			CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
			CompositionExtension->SetEnabled(true);

			GMeasuredSecondaryPreExposure.Store(1.0f);
			GMeasuredSecondaryAutoExposureMethod.Store(-1);
			GMeasuredSecondaryDynamicGI.Store(-1);
			GMeasuredSecondaryReflection.Store(-1);
			GMeasuredSecondaryIndirectLightingIntensity.Store(1.0f);
			GMeasuredSecondaryAA.Store(-1);
			GExtractionWidth.Store(0);
			GExtractionHeight.Store(0);
			GExtractionFrames.Store(0);
			GLastExtractionFrame.Store(0);
			SetPreExposureRebaseCVars(true, 1.0f);
			bTemporalHistoryValid = false;
			LastCameraCutReason = TEXT("producer start");

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FSecondaryEyeAdaptationProducer::OnWorldPostActorTick);
			bRunning = true;
			UE_LOG(LogTemp, Display,
				TEXT("PortalSecondaryEyeAdaptation: START Policy=%d PrimaryFraction=%.3f. Use portal.StartVisualParityReference for the full-screen reference."),
				CVarSecondaryEyeAdaptation.GetValueOnGameThread() != 0 ? 1 : 0,
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
			ReleaseScratch();
			SetPreExposureRebaseCVars(false, 1.0f);
			ActiveWorld.Reset();
			bRunning = false;
			WriteReport(TEXT("STOPPED"));
			UE_LOG(LogTemp, Display,
				TEXT("PortalSecondaryEyeAdaptation: STOP Submitted=%llu Skipped=%llu ExtractionFrames=%llu LastPreExposure=%.9g"),
				FramesSubmitted,
				FramesSkipped,
				GExtractionFrames.Load(),
				GMeasuredSecondaryPreExposure.Load());
		}

		bool IsRunning() const
		{
			return bRunning;
		}

		void DumpReport() const
		{
			WriteReport(TEXT("RUNNING"));
			UE_LOG(LogTemp, Display,
				TEXT("PortalSecondaryEyeAdaptation Report Policy=%d Submitted=%llu Skipped=%llu ExtractionFrames=%llu LastFrame=%llu PreExposure=%.9g AutoExposureMethod=%d AA=%d SceneRect=%dx%d DynamicGI=%d Reflection=%d IndirectLightingIntensity=%.6f"),
				CVarSecondaryEyeAdaptation.GetValueOnGameThread() != 0 ? 1 : 0,
				FramesSubmitted,
				FramesSkipped,
				GExtractionFrames.Load(),
				GLastExtractionFrame.Load(),
				GMeasuredSecondaryPreExposure.Load(),
				GMeasuredSecondaryAutoExposureMethod.Load(),
				GMeasuredSecondaryAA.Load(),
				GExtractionWidth.Load(), GExtractionHeight.Load(),
				GMeasuredSecondaryDynamicGI.Load(),
				GMeasuredSecondaryReflection.Load(),
				GMeasuredSecondaryIndirectLightingIntensity.Load());
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
					TEXT("PortalSecondaryEyeAdaptation: stopping because accepted FullFidelity backend acquired rendering ownership."));
				Stop();
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

		void SkipFrame()
		{
			++FramesSkipped;
			if (CompositionExtension)
			{
				CompositionExtension->ClearRequest();
			}
			bTemporalHistoryValid = false;
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
			bTemporalHistoryValid = true;
			LastHistoryEndpointIndex = EndpointIndex;
			LastHistoryTargetSize = TargetSize;
			LastHistoryEntryFrame = EntryFrame;
			LastHistoryExitFrame = ExitFrame;
			bLastCameraCut = bCameraCut;
			LastCameraCutReason = Reason;
		}

		void SubmitFrame(UWorld* World)
		{
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager
				|| PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture
				|| !PortalSystem->IsLinked())
			{
				SkipFrame();
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				SkipFrame();
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
				SkipFrame();
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
				SkipFrame();
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
				SkipFrame();
				return;
			}

			const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
			const int32 Height = FMath::Max(144,
				FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
			const FIntPoint TargetSize(Width, Height);
			if (!EnsureScratch(TargetSize))
			{
				SkipFrame();
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
				SkipFrame();
				return;
			}

			TSharedRef<FSecondaryEyeAdaptationExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FSecondaryEyeAdaptationExtractionExtension>(
					World, PortalTargetResource);

			const bool bEnableEyeAdaptation =
				CVarSecondaryEyeAdaptation.GetValueOnGameThread() != 0;
			FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->EngineShowFlags
				: FEngineShowFlags(ESFIM_Game);
			ShowFlags.SetEyeAdaptation(bEnableEyeAdaptation);
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

			SetPreExposureRebaseCVars(true,
				FMath::Max(GMeasuredSecondaryPreExposure.Load(), UE_SMALL_NUMBER));

			FCanvas Canvas(ScratchResource, nullptr, World, World->GetFeatureLevel(),
				FCanvas::CDM_DeferDrawing, 1.0f);
			IRendererModule& RendererModule =
				FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
			RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

			Request.PortalRenderTarget = PortalTargetResource;
			Request.PortalDepthRenderTarget = nullptr;
			CompositionExtension->PublishRequest(Request);

			++FramesSubmitted;
			CommitHistory(
				EndpointIndex, TargetSize, EntryFrame, ExitFrame,
				bCameraCut, CameraCutReason);
		}

		void WriteReport(const TCHAR* Status) const
		{
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"secondaryEyeAdaptation\":%s,\n")
				TEXT("  \"primaryResolutionFraction\":%.6f,\n")
				TEXT("  \"framesSubmitted\":%llu,\n")
				TEXT("  \"framesSkipped\":%llu,\n")
				TEXT("  \"extractionFrames\":%llu,\n")
				TEXT("  \"lastExtractionFrame\":%llu,\n")
				TEXT("  \"secondaryPreExposure\":%.9g,\n")
				TEXT("  \"autoExposureMethod\":%d,\n")
				TEXT("  \"aaMethod\":%d,\n")
				TEXT("  \"sceneRect\":[%d,%d],\n")
				TEXT("  \"dynamicGI\":%d,\n")
				TEXT("  \"reflection\":%d,\n")
				TEXT("  \"indirectLightingIntensity\":%.6f,\n")
				TEXT("  \"lastCameraCut\":%s,\n")
				TEXT("  \"lastCameraCutReason\":\"%s\",\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.14C is a controlled secondary EyeAdaptation ownership A/B only. It does not yet promote either policy to production or claim final visual parity.\"\n")
				TEXT("}\n"),
				Status,
				CVarSecondaryEyeAdaptation.GetValueOnAnyThread() != 0 ? TEXT("true") : TEXT("false"),
				PrimaryResolutionFraction,
				FramesSubmitted,
				FramesSkipped,
				GExtractionFrames.Load(),
				GLastExtractionFrame.Load(),
				GMeasuredSecondaryPreExposure.Load(),
				GMeasuredSecondaryAutoExposureMethod.Load(),
				GMeasuredSecondaryAA.Load(),
				GExtractionWidth.Load(), GExtractionHeight.Load(),
				GMeasuredSecondaryDynamicGI.Load(),
				GMeasuredSecondaryReflection.Load(),
				GMeasuredSecondaryIndirectLightingIntensity.Load(),
				bLastCameraCut ? TEXT("true") : TEXT("false"),
				*LastCameraCutReason.ReplaceCharWithEscapedChar());

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalSecondaryEyeAdaptationSpike.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		bool bTemporalHistoryValid = false;
		bool bLastCameraCut = true;
		float PrimaryResolutionFraction = 0.67f;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		FSceneViewStateReference SecondaryViewState;
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 FramesSubmitted = 0;
		uint64 FramesSkipped = 0;
		int32 LastHistoryEndpointIndex = INDEX_NONE;
		FIntPoint LastHistoryTargetSize = FIntPoint::ZeroValue;
		FTransform LastHistoryEntryFrame = FTransform::Identity;
		FTransform LastHistoryExitFrame = FTransform::Identity;
		FString LastCameraCutReason = TEXT("not started");
	};

	TUniquePtr<FSecondaryEyeAdaptationProducer> GProducer;

	void StartSpike()
	{
		if (GProducer && GProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalSecondaryEyeAdaptation: already running."));
			return;
		}
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("PortalSecondaryEyeAdaptation: enter PIE first."));
			return;
		}
		GProducer = MakeUnique<FSecondaryEyeAdaptationProducer>();
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
			UE_LOG(LogTemp, Display, TEXT("PortalSecondaryEyeAdaptation: not running."));
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
			UE_LOG(LogTemp, Display, TEXT("PortalSecondaryEyeAdaptation: not running."));
		}
	}

	FAutoConsoleCommand GStartCommand(
		TEXT("portal.StartSecondaryEyeAdaptationSpike"),
		TEXT("Start STEP 1B.14C transformed full-view TSR producer using portal.SecondaryEyeAdaptation 0/1."),
		FConsoleCommandDelegate::CreateStatic(&StartSpike));

	FAutoConsoleCommand GStopCommand(
		TEXT("portal.StopSecondaryEyeAdaptationSpike"),
		TEXT("Stop STEP 1B.14C secondary EyeAdaptation A/B producer."),
		FConsoleCommandDelegate::CreateStatic(&StopSpike));

	FAutoConsoleCommand GDumpCommand(
		TEXT("portal.DumpSecondaryEyeAdaptationSpike"),
		TEXT("Write STEP 1B.14C secondary EyeAdaptation telemetry to Saved/AutomationReports."),
		FConsoleCommandDelegate::CreateStatic(&DumpSpike));
}
