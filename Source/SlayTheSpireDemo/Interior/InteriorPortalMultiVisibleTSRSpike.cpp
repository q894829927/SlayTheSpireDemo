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
#include "HAL/FileManager.h"
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
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalMultiVisibleTSRPrivate
{
	constexpr int32 EndpointCount = 2;

	TAutoConsoleVariable<int32> CVarMultiVisibleDiagnostics(
		TEXT("portal.MultiVisibleDiagnostics"),
		0,
		TEXT("STEP 1B.14D-MV endpoint-aware multi-visible TSR diagnostics. 0=quiet, 1=periodic telemetry."),
		ECVF_Default);

	float ReadPrimaryFraction()
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(
			TEXT("portal.FullViewFamilyTSRPrimaryFraction")))
		{
			return FMath::Clamp(Var->GetFloat(), 0.5f, 1.0f);
		}
		return 0.67f;
	}

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

	struct FEndpointState
	{
		explicit FEndpointState(const int32 InEndpointIndex)
			: EndpointIndex(InEndpointIndex)
		{
		}

		int32 EndpointIndex = INDEX_NONE;
		FSceneViewStateReference ViewState;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		UTextureRenderTarget2D* SecondaryDepthTarget = nullptr;
		FIntPoint SecondaryDepthTargetSize = FIntPoint::ZeroValue;

		bool bHistoryValid = false;
		bool bVisibleLastTick = false;
		bool bLastCameraCut = true;
		uint64 HistoryGeneration = 1;
		TAtomic<uint64> ActivePublicationGeneration { 1 };
		FIntPoint LastTargetSize = FIntPoint::ZeroValue;
		FTransform LastEntryFrame = FTransform::Identity;
		FTransform LastExitFrame = FTransform::Identity;
		FString LastCameraCutReason = TEXT("not started");

		uint64 FramesSubmitted = 0;
		uint64 FramesSkipped = 0;
		uint64 CameraCutCount = 0;
		uint64 ContinuousHistoryFrames = 0;

		TAtomic<uint64> LastExtractionFrame { 0 };
		TAtomic<uint64> LastDepthExtractionFrame { 0 };
		TAtomic<uint64> LastCompletedSubmission { 0 };
		TAtomic<float> SecondaryPreExposure { 1.0f };
		TAtomic<int32> ObservedAAMethod { -1 };
		TAtomic<float> LastTemporalJitterX { 0.0f };
		TAtomic<float> LastTemporalJitterY { 0.0f };
		TAtomic<bool> bTemporalJitterObserved { false };
		TAtomic<int32> ExtractionInputWidth { 0 };
		TAtomic<int32> ExtractionInputHeight { 0 };
		TAtomic<int32> DepthSourceWidth { 0 };
		TAtomic<int32> DepthSourceHeight { 0 };
		TAtomic<int32> DepthTargetWidth { 0 };
		TAtomic<int32> DepthTargetHeight { 0 };
	};

	class FEndpointExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FEndpointExtractionExtension(
			const FAutoRegister& AutoRegister,
			UWorld* InWorld,
			FEndpointState* InEndpointState,
			FSceneViewStateInterface* InExpectedViewState,
			FRenderTarget* InExtractionTarget,
			FRenderTarget* InDepthExtractionTarget,
			const FIntPoint& InExpectedDepthSourceSize,
			TSharedRef<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> InColorSample,
			TSharedRef<FInteriorPortalViewExtension, ESPMode::ThreadSafe> InCompositionExtension,
			const FInteriorPortalRenderRequest& InCompletedRequest)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, EndpointState(InEndpointState)
			, ExpectedViewState(InExpectedViewState)
			, ExtractionTarget(InExtractionTarget)
			, DepthExtractionTarget(InDepthExtractionTarget)
			, ExpectedDepthSourceSize(InExpectedDepthSourceSize)
			, ColorSample(InColorSample)
			, CompositionExtension(InCompositionExtension)
			, CompletedRequest(InCompletedRequest)
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
				|| !EndpointState
				|| !InView.Family
				|| !InView.Family->bAdditionalViewFamily
				|| InView.State != ExpectedViewState
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
					if (!SceneColor.IsValid() || !EndpointState || !ExtractionTarget)
					{
						return SceneColor;
					}

					const float MeasuredPreExposure = View.State
						? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					EndpointState->SecondaryPreExposure.Store(MeasuredPreExposure);
					EndpointState->ObservedAAMethod.Store(static_cast<int32>(View.AntiAliasingMethod));
					EndpointState->ExtractionInputWidth.Store(SceneColor.ViewRect.Width());
					EndpointState->ExtractionInputHeight.Store(SceneColor.ViewRect.Height());

					const FVector2D TemporalJitter = View.ViewMatrices.GetTemporalAAJitter();
					EndpointState->LastTemporalJitterX.Store(static_cast<float>(TemporalJitter.X));
					EndpointState->LastTemporalJitterY.Store(static_cast<float>(TemporalJitter.Y));
					if (FMath::Abs(TemporalJitter.X) > 1.0e-8
						|| FMath::Abs(TemporalJitter.Y) > 1.0e-8)
					{
						EndpointState->bTemporalJitterObserved.Store(true);
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

					if (DepthExtractionTarget)
					{
						const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextureUniformBuffer =
							CreateSceneTextureUniformBuffer(
								GraphBuilder, View, ESceneTextureSetupMode::SceneDepth);
						if (SceneTextureUniformBuffer)
						{
							const FSceneTextureUniformParameters* SceneTextureContents =
								SceneTextureUniformBuffer->GetContents();
							FRDGTextureRef SceneDepthTexture = SceneTextureContents
								? SceneTextureContents->SceneDepthTexture : nullptr;
							FRDGTextureRef DepthExtractionTexture =
								DepthExtractionTarget->GetRenderTargetTexture(GraphBuilder);
							if (SceneDepthTexture && DepthExtractionTexture)
							{
								const FIntPoint AvailableDepthExtent = SceneDepthTexture->Desc.Extent;
								const FIntPoint SourceSize(
									FMath::Clamp(ExpectedDepthSourceSize.X, 1, AvailableDepthExtent.X),
									FMath::Clamp(ExpectedDepthSourceSize.Y, 1, AvailableDepthExtent.Y));
								EndpointState->DepthSourceWidth.Store(SourceSize.X);
								EndpointState->DepthSourceHeight.Store(SourceSize.Y);
								EndpointState->DepthTargetWidth.Store(DepthExtractionTexture->Desc.Extent.X);
								EndpointState->DepthTargetHeight.Store(DepthExtractionTexture->Desc.Extent.Y);

								GraphBuilder.UseInternalAccessMode(DepthExtractionTexture);
								AddDrawTexturePass(
									GraphBuilder,
									View,
									SceneDepthTexture,
									DepthExtractionTexture,
									FIntPoint::ZeroValue,
									SourceSize,
									FIntPoint::ZeroValue,
									DepthExtractionTexture->Desc.Extent,
									TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
								GraphBuilder.UseExternalAccessMode(
									DepthExtractionTexture, ERHIAccess::SRVMask);
								EndpointState->LastDepthExtractionFrame.Store(GFrameCounter);
							}
						}
					}

					ColorSample->PreExposure = MeasuredPreExposure;
					EndpointState->LastExtractionFrame.Store(GFrameCounter);
					EndpointState->LastCompletedSubmission.Store(ColorSample->Submission);

					// Visibility changes advance this generation on the game thread. An old
					// in-flight secondary render is therefore not allowed to republish a stale
					// request after its endpoint has left the visible set.
					if (EndpointState->ActivePublicationGeneration.Load()
						== CompletedRequest.RendererHistoryGeneration)
					{
						CompositionExtension->PublishRequest(CompletedRequest);
					}

					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return EndpointState != nullptr
				&& ExtractionTarget != nullptr
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		FEndpointState* EndpointState = nullptr;
		FSceneViewStateInterface* ExpectedViewState = nullptr;
		FRenderTarget* ExtractionTarget = nullptr;
		FRenderTarget* DepthExtractionTarget = nullptr;
		FIntPoint ExpectedDepthSourceSize = FIntPoint::ZeroValue;
		TSharedRef<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> ColorSample;
		TSharedRef<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
		FInteriorPortalRenderRequest CompletedRequest;
	};

	class FMultiVisibleProducer
	{
	public:
		FMultiVisibleProducer()
		{
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				Endpoints[EndpointIndex] = MakeUnique<FEndpointState>(EndpointIndex);
			}
		}

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
					TEXT("PortalMultiVisible: requires PIE/Game with RendererBackend=SceneCapture."));
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
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& State = *Endpoints[EndpointIndex];
				State.ViewState.Allocate(World->GetFeatureLevel());
				State.CompositionExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
				State.CompositionExtension->SetEnabled(true);
				ResetEndpoint(State);
			}

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FMultiVisibleProducer::OnWorldPostActorTick);
			bRunning = true;
			Status = TEXT("RUNNING_MULTI_VISIBLE_TSR");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible: START. Endpoints=2 PrimaryFraction=%.3f SharedFinalScratch=1 PerEndpointViewState=1 PerEndpointDepth=1."),
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

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& State = *Endpoints[EndpointIndex];
				AdvancePublicationGeneration(State);
				if (State.CompositionExtension)
				{
					State.CompositionExtension->SetEnabled(false);
					State.CompositionExtension->ClearRequest();
				}
			}

			FlushRenderingCommands();

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& State = *Endpoints[EndpointIndex];
				State.CompositionExtension.Reset();
				State.ViewState.Destroy();
				ReleaseDepthTarget(State);
			}
			ReleaseFinalScratch();

			ActiveWorld.Reset();
			bRunning = false;
			Status = TEXT("STOPPED");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible: STOP. VisibleMask=0x%02x SubmittedMask=0x%02x."),
				LastVisibleMask, LastSubmittedMask);
		}

		bool IsRunning() const
		{
			return bRunning;
		}

		void DumpReport() const
		{
			WriteReport();
			const int32 PublishedMask = BuildPublishedMask();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible Report VisibleCount=%d VisibleMask=0x%02x SubmittedMask=0x%02x PublishedMask=0x%02x E0[Submitted=%llu ExtractFrame=%llu Cuts=%llu Continuous=%llu Pre=%.9g Completed=%llu] E1[Submitted=%llu ExtractFrame=%llu Cuts=%llu Continuous=%llu Pre=%.9g Completed=%llu]"),
				CountBits(LastVisibleMask), LastVisibleMask, LastSubmittedMask, PublishedMask,
				Endpoints[0]->FramesSubmitted, Endpoints[0]->LastExtractionFrame.Load(),
				Endpoints[0]->CameraCutCount, Endpoints[0]->ContinuousHistoryFrames,
				Endpoints[0]->SecondaryPreExposure.Load(), Endpoints[0]->LastCompletedSubmission.Load(),
				Endpoints[1]->FramesSubmitted, Endpoints[1]->LastExtractionFrame.Load(),
				Endpoints[1]->CameraCutCount, Endpoints[1]->ContinuousHistoryFrames,
				Endpoints[1]->SecondaryPreExposure.Load(), Endpoints[1]->LastCompletedSubmission.Load());
		}

	private:
		static int32 CountBits(const int32 Mask)
		{
			return ((Mask & 1) ? 1 : 0) + ((Mask & 2) ? 1 : 0);
		}

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
			SubmitVisibleEndpoints(World);
		}

		void ResetEndpoint(FEndpointState& State)
		{
			State.bHistoryValid = false;
			State.bVisibleLastTick = false;
			State.bLastCameraCut = true;
			State.HistoryGeneration = 1;
			State.ActivePublicationGeneration.Store(1);
			State.LastTargetSize = FIntPoint::ZeroValue;
			State.LastEntryFrame = FTransform::Identity;
			State.LastExitFrame = FTransform::Identity;
			State.LastCameraCutReason = TEXT("producer start");
			State.FramesSubmitted = 0;
			State.FramesSkipped = 0;
			State.CameraCutCount = 0;
			State.ContinuousHistoryFrames = 0;
			State.LastExtractionFrame.Store(0);
			State.LastDepthExtractionFrame.Store(0);
			State.LastCompletedSubmission.Store(0);
			State.SecondaryPreExposure.Store(1.0f);
			State.ObservedAAMethod.Store(-1);
			State.LastTemporalJitterX.Store(0.0f);
			State.LastTemporalJitterY.Store(0.0f);
			State.bTemporalJitterObserved.Store(false);
			State.ExtractionInputWidth.Store(0);
			State.ExtractionInputHeight.Store(0);
			State.DepthSourceWidth.Store(0);
			State.DepthSourceHeight.Store(0);
			State.DepthTargetWidth.Store(0);
			State.DepthTargetHeight.Store(0);
		}

		void AdvancePublicationGeneration(FEndpointState& State)
		{
			++State.HistoryGeneration;
			if (State.HistoryGeneration == 0)
			{
				State.HistoryGeneration = 1;
			}
			State.ActivePublicationGeneration.Store(State.HistoryGeneration);
		}

		void HideEndpoint(FEndpointState& State, const TCHAR* Reason)
		{
			if (!State.bVisibleLastTick)
			{
				return;
			}
			State.bVisibleLastTick = false;
			State.bHistoryValid = false;
			State.LastCameraCutReason = Reason;
			AdvancePublicationGeneration(State);
			if (State.CompositionExtension)
			{
				State.CompositionExtension->ClearRequest();
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

		bool EnsureDepthTarget(FEndpointState& State, UWorld* World, const FIntPoint TargetSize)
		{
			if (!World || TargetSize.X <= 0 || TargetSize.Y <= 0)
			{
				return false;
			}
			if (State.SecondaryDepthTarget && State.SecondaryDepthTargetSize == TargetSize)
			{
				return true;
			}
			if (State.SecondaryDepthTarget)
			{
				FlushRenderingCommands();
				ReleaseDepthTarget(State);
			}
			State.SecondaryDepthTarget = NewObject<UTextureRenderTarget2D>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!State.SecondaryDepthTarget)
			{
				return false;
			}
			State.SecondaryDepthTarget->AddToRoot();
			State.SecondaryDepthTarget->RenderTargetFormat = RTF_R32f;
			State.SecondaryDepthTarget->ClearColor = FLinearColor::Black;
			State.SecondaryDepthTarget->bForceLinearGamma = true;
			State.SecondaryDepthTarget->bAutoGenerateMips = false;
			State.SecondaryDepthTarget->InitCustomFormat(
				TargetSize.X, TargetSize.Y, PF_R32_FLOAT, true);
			State.SecondaryDepthTarget->UpdateResourceImmediate(true);
			State.SecondaryDepthTargetSize = TargetSize;
			return State.SecondaryDepthTarget->GameThread_GetRenderTargetResource() != nullptr;
		}

		void ReleaseDepthTarget(FEndpointState& State)
		{
			if (State.SecondaryDepthTarget)
			{
				State.SecondaryDepthTarget->RemoveFromRoot();
				State.SecondaryDepthTarget = nullptr;
			}
			State.SecondaryDepthTargetSize = FIntPoint::ZeroValue;
		}

		bool DetermineCameraCut(
			const FEndpointState& State,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			FString& OutReason) const
		{
			if (!State.bHistoryValid)
			{
				OutReason = TEXT("history invalid / first visible frame");
				return true;
			}
			if (State.LastTargetSize != TargetSize)
			{
				OutReason = TEXT("render target size changed");
				return true;
			}
			if (PortalFrameChanged(State.LastEntryFrame, EntryFrame)
				|| PortalFrameChanged(State.LastExitFrame, ExitFrame))
			{
				OutReason = TEXT("portal logical frame changed");
				return true;
			}
			OutReason = TEXT("continuous endpoint history");
			return false;
		}

		void CommitHistory(
			FEndpointState& State,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			const bool bCameraCut,
			const FString& CameraCutReason)
		{
			State.bLastCameraCut = bCameraCut;
			State.LastCameraCutReason = CameraCutReason;
			if (bCameraCut)
			{
				++State.CameraCutCount;
			}
			else
			{
				++State.ContinuousHistoryFrames;
			}
			State.bHistoryValid = true;
			State.bVisibleLastTick = true;
			State.LastTargetSize = TargetSize;
			State.LastEntryFrame = EntryFrame;
			State.LastExitFrame = ExitFrame;
		}

		void SubmitVisibleEndpoints(UWorld* World)
		{
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager
				|| PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture
				|| !PortalSystem->IsLinked())
			{
				for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
				{
					HideEndpoint(*Endpoints[EndpointIndex], TEXT("portal pair/player/backend unavailable"));
				}
				LastVisibleMask = 0;
				LastSubmittedMask = 0;
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
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

			const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
			const int32 Height = FMath::Max(144,
				FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
			const FIntPoint TargetSize(Width, Height);
			const FIntPoint ExpectedPrimarySize(
				FMath::Max(1, FMath::RoundToInt(TargetSize.X * PrimaryResolutionFraction)),
				FMath::Max(1, FMath::RoundToInt(TargetSize.Y * PrimaryResolutionFraction)));

			if (!EnsureFinalScratch(World, TargetSize))
			{
				return;
			}

			int32 VisibleMask = 0;
			int32 SubmittedMask = 0;
			AInteriorPortal* Candidates[EndpointCount] = {
				PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()
			};

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& State = *Endpoints[EndpointIndex];
				AInteriorPortal* Entry = Candidates[EndpointIndex];
				AInteriorPortal* Exit = Candidates[1 - EndpointIndex];
				InteriorPortalMath::FPortalScreenBounds CandidateBounds;
				const bool bVisible = IsValid(Entry)
					&& IsValid(Exit)
					&& InteriorPortalMath::ProjectPortalApertureToScreenBounds(
						Entry->GetLogicalFrame(), Entry->HalfWidth, Entry->HalfHeight,
						PlayerViewProjection, PlayerRect, CandidateBounds,
						ProjectionData.IsPerspectiveProjection(),
						ProjectionData.GetNearPlaneFromProjectionMatrix());

				if (!bVisible)
				{
					HideEndpoint(State, TEXT("endpoint left visible set"));
					continue;
				}

				VisibleMask |= (1 << EndpointIndex);
				if (SubmitEndpoint(
					World, *PortalSystem, *Player, ProjectionData, PlayerRect,
					PlayerView, PlayerViewProjection, POV, TargetSize, ExpectedPrimarySize,
					EndpointIndex, Entry, Exit, State))
				{
					SubmittedMask |= (1 << EndpointIndex);
				}
			}

			LastVisibleMask = VisibleMask;
			LastSubmittedMask = SubmittedMask;
			++ProducerTicks;

			if (CVarMultiVisibleDiagnostics.GetValueOnGameThread() != 0
				&& (ProducerTicks == 1 || (ProducerTicks % 120) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Tick=%llu VisibleCount=%d VisibleMask=0x%02x SubmittedMask=0x%02x PublishedMask=0x%02x"),
					ProducerTicks, CountBits(VisibleMask), VisibleMask, SubmittedMask, BuildPublishedMask());
			}
		}

		bool SubmitEndpoint(
			UWorld* World,
			AInteriorPortalSystem& PortalSystem,
			APlayerController& Player,
			const FSceneViewProjectionData& ProjectionData,
			const FIntRect& PlayerRect,
			const FTransform& PlayerView,
			const FMatrix& PlayerViewProjection,
			const FMinimalViewInfo& POV,
			const FIntPoint TargetSize,
			const FIntPoint ExpectedPrimarySize,
			const int32 EndpointIndex,
			AInteriorPortal* Entry,
			AInteriorPortal* Exit,
			FEndpointState& State)
		{
			if (!IsValid(Entry) || !IsValid(Exit) || !State.CompositionExtension)
			{
				++State.FramesSkipped;
				return false;
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
				PortalSystem.ClipPlaneBias,
				State.HistoryGeneration, Request)
				|| !Request.IsValid() || !IsFiniteTransform(Request.VirtualView))
			{
				++State.FramesSkipped;
				return false;
			}

			// The visible portal surface is deliberately biased toward the entry side
			// to avoid host-surface Z fighting. Main SceneDepth therefore sees that
			// surface slightly before the logical traversal plane. At grazing angles
			// the line-of-sight depth gap is amplified and the accepted depth-aware
			// compositor can otherwise classify the portal's own spiral surface as a
			// foreground occluder. Keep logical aperture/remote-depth math unchanged,
			// but use the actual cosmetic surface plane for the foreground reference.
			FTransform ForegroundDepthFrame = EntryFrame;
			ForegroundDepthFrame.AddToTranslation(
				EntryFrame.GetUnitAxis(EAxis::X) * Entry->SurfaceVisualBias);
			InteriorPortalProjectiveAperture::BuildScreenToPortalMapping(
				ForegroundDepthFrame,
				Entry->HalfWidth,
				Entry->HalfHeight,
				PlayerViewProjection,
				Request.ForegroundDepthReference);

			if (!EnsureDepthTarget(State, World, TargetSize))
			{
				++State.FramesSkipped;
				return false;
			}

			Entry->EnsureTargets(TargetSize.X, TargetSize.Y, 1);
			UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets.IsValidIndex(0)
				? Entry->RenderTargets[0] : nullptr;
			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* FinalScratchResource = FinalScratch
				? FinalScratch->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* DepthTargetResource = State.SecondaryDepthTarget
				? State.SecondaryDepthTarget->GameThread_GetRenderTargetResource() : nullptr;
			if (!PortalTargetResource || !FinalScratchResource || !DepthTargetResource || !World->Scene)
			{
				++State.FramesSkipped;
				return false;
			}

			Request.PortalRenderTarget = PortalTargetResource;
			Request.PortalDepthRenderTarget = DepthTargetResource;
			Request.ColorSample = MakeShared<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe>(
				State.FramesSubmitted + 1);

			FSceneViewStateInterface* ExpectedViewState = State.ViewState.GetReference();
			TSharedRef<FEndpointExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FEndpointExtractionExtension>(
					World,
					&State,
					ExpectedViewState,
					PortalTargetResource,
					DepthTargetResource,
					ExpectedPrimarySize,
					Request.ColorSample.ToSharedRef(),
					State.CompositionExtension.ToSharedRef(),
					Request);

			FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->EngineShowFlags
				: FEngineShowFlags(ESFIM_Game);
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
				State, TargetSize, EntryFrame, ExitFrame, CameraCutReason);

			FSceneViewInitOptions ViewInitOptions;
			ViewInitOptions.ViewFamily = &ViewFamily;
			ViewInitOptions.SceneViewStateInterface = ExpectedViewState;
			ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TargetSize.X, TargetSize.Y));
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
			HidePortalPrimitives(PortalSystem.BluePortal.Get(), ViewInitOptions);
			HidePortalPrimitives(PortalSystem.OrangePortal.Get(), ViewInitOptions);

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

			FCanvas Canvas(
				FinalScratchResource, nullptr, World, World->GetFeatureLevel(),
				FCanvas::CDM_DeferDrawing, 1.0f);
			IRendererModule& RendererModule =
				FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
			RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);

			++State.FramesSubmitted;
			CommitHistory(State, TargetSize, EntryFrame, ExitFrame, bCameraCut, CameraCutReason);

			if (CVarMultiVisibleDiagnostics.GetValueOnGameThread() != 0
				&& (State.FramesSubmitted == 1 || (State.FramesSubmitted % 120) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Endpoint=%d Submitted=%llu CameraCut=%d Cuts=%llu Continuous=%llu Generation=%llu Pre=%.9g ExtractFrame=%llu Completed=%llu"),
					EndpointIndex,
					State.FramesSubmitted,
					bCameraCut ? 1 : 0,
					State.CameraCutCount,
					State.ContinuousHistoryFrames,
					State.HistoryGeneration,
					State.SecondaryPreExposure.Load(),
					State.LastExtractionFrame.Load(),
					State.LastCompletedSubmission.Load());
			}
			return true;
		}

		int32 BuildPublishedMask() const
		{
			int32 Mask = 0;
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				const FEndpointState& State = *Endpoints[EndpointIndex];
				if (State.CompositionExtension && State.CompositionExtension->HasPublishedRequest())
				{
					Mask |= (1 << EndpointIndex);
				}
			}
			return Mask;
		}

		void WriteReport() const
		{
			const int32 PublishedMask = BuildPublishedMask();
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"primaryResolutionFraction\":%.6f,\n")
				TEXT("  \"visibleEndpointCount\":%d,\n")
				TEXT("  \"visibleEndpointMask\":%d,\n")
				TEXT("  \"submittedEndpointMask\":%d,\n")
				TEXT("  \"publishedEndpointMask\":%d,\n")
				TEXT("  \"sharedFinalScratch\":true,\n")
				TEXT("  \"endpoint0\":{\"framesSubmitted\":%llu,\"framesSkipped\":%llu,\"lastExtractionFrame\":%llu,\"lastDepthExtractionFrame\":%llu,\"cameraCutCount\":%llu,\"continuousHistoryFrames\":%llu,\"secondaryPreExposure\":%.9g,\"observedAA\":%d,\"jitterObserved\":%s,\"completedSubmission\":%llu,\"historyGeneration\":%llu},\n")
				TEXT("  \"endpoint1\":{\"framesSubmitted\":%llu,\"framesSkipped\":%llu,\"lastExtractionFrame\":%llu,\"lastDepthExtractionFrame\":%llu,\"cameraCutCount\":%llu,\"continuousHistoryFrames\":%llu,\"secondaryPreExposure\":%.9g,\"observedAA\":%d,\"jitterObserved\":%s,\"completedSubmission\":%llu,\"historyGeneration\":%llu},\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.14D-MV top-level Blue/Orange simultaneous visibility spike. Each endpoint owns TSR ViewState/history/depth/composition; only the final renderer scratch is sequentially shared. Recursion >= 2 remains out of scope.\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				PrimaryResolutionFraction,
				CountBits(LastVisibleMask),
				LastVisibleMask,
				LastSubmittedMask,
				PublishedMask,
				Endpoints[0]->FramesSubmitted,
				Endpoints[0]->FramesSkipped,
				Endpoints[0]->LastExtractionFrame.Load(),
				Endpoints[0]->LastDepthExtractionFrame.Load(),
				Endpoints[0]->CameraCutCount,
				Endpoints[0]->ContinuousHistoryFrames,
				Endpoints[0]->SecondaryPreExposure.Load(),
				Endpoints[0]->ObservedAAMethod.Load(),
				Endpoints[0]->bTemporalJitterObserved.Load() ? TEXT("true") : TEXT("false"),
				Endpoints[0]->LastCompletedSubmission.Load(),
				Endpoints[0]->HistoryGeneration,
				Endpoints[1]->FramesSubmitted,
				Endpoints[1]->FramesSkipped,
				Endpoints[1]->LastExtractionFrame.Load(),
				Endpoints[1]->LastDepthExtractionFrame.Load(),
				Endpoints[1]->CameraCutCount,
				Endpoints[1]->ContinuousHistoryFrames,
				Endpoints[1]->SecondaryPreExposure.Load(),
				Endpoints[1]->ObservedAAMethod.Load(),
				Endpoints[1]->bTemporalJitterObserved.Load() ? TEXT("true") : TEXT("false"),
				Endpoints[1]->LastCompletedSubmission.Load(),
				Endpoints[1]->HistoryGeneration);

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalMultiVisibleTSRSpike.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		float PrimaryResolutionFraction = 0.67f;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		TUniquePtr<FEndpointState> Endpoints[EndpointCount];
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 ProducerTicks = 0;
		int32 LastVisibleMask = 0;
		int32 LastSubmittedMask = 0;
		FString Status = TEXT("STOPPED");
	};

	TUniquePtr<FMultiVisibleProducer> GMultiVisibleProducer;

	void StartMultiVisible()
	{
		if (GMultiVisibleProducer && GMultiVisibleProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalMultiVisible: already running."));
			return;
		}

		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalMultiVisible: PIE/Game world unavailable."));
			return;
		}

		GMultiVisibleProducer = MakeUnique<FMultiVisibleProducer>();
		if (!GMultiVisibleProducer->Start(World))
		{
			GMultiVisibleProducer.Reset();
		}
	}

	void StopMultiVisible()
	{
		if (GMultiVisibleProducer)
		{
			GMultiVisibleProducer->Stop();
			GMultiVisibleProducer.Reset();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalMultiVisible: not running."));
		}
	}

	void DumpMultiVisible()
	{
		if (GMultiVisibleProducer)
		{
			GMultiVisibleProducer->DumpReport();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalMultiVisible: not running; no live report to dump."));
		}
	}

	FAutoConsoleCommand GStartMultiVisibleCommand(
		TEXT("portal.StartMultiVisibleTSRSpike"),
		TEXT("Start STEP 1B.14D-MV two-endpoint TSR producer. Stops the single-visible TSR spike."),
		FConsoleCommandDelegate::CreateStatic(&StartMultiVisible));

	FAutoConsoleCommand GStopMultiVisibleCommand(
		TEXT("portal.StopMultiVisibleTSRSpike"),
		TEXT("Stop STEP 1B.14D-MV and release endpoint-owned histories/depth targets."),
		FConsoleCommandDelegate::CreateStatic(&StopMultiVisible));

	FAutoConsoleCommand GDumpMultiVisibleCommand(
		TEXT("portal.DumpMultiVisibleTSRSpike"),
		TEXT("Dump endpoint-aware STEP 1B.14D-MV telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpMultiVisible));
}
