#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalRecursionLifetime.h"

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
	constexpr int32 MaxRecursionDepth = 4;

	TAutoConsoleVariable<int32> CVarMultiVisibleDiagnostics(
		TEXT("portal.MultiVisibleDiagnostics"),
		0,
		TEXT("Endpoint/recursion-aware full-fidelity TSR diagnostics. 0=quiet, 1=periodic telemetry."),
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

	FMatrix BuildViewProjection(const FTransform& View, const FMatrix& ProjectionMatrix)
	{
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		return FTranslationMatrix(-View.GetLocation())
			* FInverseRotationMatrix(View.Rotator())
			* PortalViewPlanes
			* ProjectionMatrix;
	}

	struct FLayerState
	{
		explicit FLayerState(const int32 InLevel) : Level(InLevel) {}

		int32 Level = 0;
		FSceneViewStateReference ViewState;
		UTextureRenderTarget2D* SecondaryDepthTarget = nullptr;
		FIntPoint SecondaryDepthTargetSize = FIntPoint::ZeroValue;
		InteriorPortalRecursionLifetime::FLifetime Lifetime;

		bool bHistoryValid = false;
		bool bVisibleLastTick = false;
		bool bLastCameraCut = true;
		uint64 HistoryGeneration = 0;
		TAtomic<uint64> ActivePublicationGeneration { 0 };
		FIntPoint LastTargetSize = FIntPoint::ZeroValue;
		FTransform LastEntryFrame = FTransform::Identity;
		FTransform LastExitFrame = FTransform::Identity;
		FString LastCameraCutReason = TEXT("not started");
		FString LastSubmissionFailureReason = TEXT("NOT_ATTEMPTED_THIS_FRAME");

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

	class FRecursiveCompositionExtension final : public FWorldSceneViewExtension
	{
	public:
		FRecursiveCompositionExtension(
			const FAutoRegister& AutoRegister,
			UWorld* InWorld,
			FSceneViewStateInterface* InExpectedParentViewState)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, ExpectedParentViewState(InExpectedParentViewState)
		{
		}

		void SetEnabled(const bool bInEnabled)
		{
			bEnabled = bInEnabled;
			if (!bEnabled)
			{
				ClearRequest();
			}
		}

		void PublishRequest(const FInteriorPortalRenderRequest& Request)
		{
			FScopeLock Lock(&RequestMutex);
			PublishedRequest = Request;
		}

		void ClearRequest()
		{
			FScopeLock Lock(&RequestMutex);
			PublishedRequest.Reset();
		}

		bool HasPublishedRequest() const
		{
			FScopeLock Lock(&RequestMutex);
			return PublishedRequest.IsSet();
		}

		FInteriorPortalRenderRequest GetPublishedRequest() const
		{
			FScopeLock Lock(&RequestMutex);
			return PublishedRequest.IsSet()
				? PublishedRequest.GetValue()
				: FInteriorPortalRenderRequest();
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
			if (!bEnabled
				|| Pass != ISceneViewExtension::EPostProcessingPass::BeforeDOF
				|| !InView.Family
				|| !InView.Family->bAdditionalViewFamily
				|| InView.State != ExpectedParentViewState)
			{
				return;
			}

			const FInteriorPortalRenderRequest Request = GetPublishedRequest();
			if (!Request.IsValid() || !Request.PortalRenderTarget)
			{
				return;
			}

			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[Request](FRDGBuilder& GraphBuilder, const FSceneView& View,
					const FPostProcessMaterialInputs& Inputs)
				{
					return FInteriorPortalViewExtension::ComposePortalIntoSceneColor(
						GraphBuilder, View, Inputs, Request);
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return bEnabled
				&& ExpectedParentViewState != nullptr
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		FSceneViewStateInterface* ExpectedParentViewState = nullptr;
		bool bEnabled = true;
		mutable FCriticalSection RequestMutex;
		TOptional<FInteriorPortalRenderRequest> PublishedRequest;
	};

	struct FEndpointState
	{
		explicit FEndpointState(const int32 InEndpointIndex)
			: EndpointIndex(InEndpointIndex)
		{
			for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
			{
				Layers[Level] = MakeUnique<FLayerState>(Level);
			}
		}

		int32 EndpointIndex = INDEX_NONE;
		TUniquePtr<FLayerState> Layers[MaxRecursionDepth];
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> MainCompositionExtension;
		TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> RecursiveCompositionExtensions[MaxRecursionDepth];
		int32 LastVisibleDepth = 0;
		int32 LastEffectiveDepth = 0;
		int32 LastAttemptedLayerMask = 0;
		int32 LastSubmittedLayerMask = 0;
	};

	class FLayerExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FLayerExtractionExtension(
			const FAutoRegister& AutoRegister,
			UWorld* InWorld,
			FLayerState* InLayerState,
			FSceneViewStateInterface* InExpectedViewState,
			FRenderTarget* InExtractionTarget,
			FRenderTarget* InDepthExtractionTarget,
			const FIntPoint& InExpectedDepthSourceSize,
			TSharedRef<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> InColorSample,
			TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> InMainPublisher,
			TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> InRecursivePublisher,
			const FInteriorPortalRenderRequest& InCompletedRequest)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, LayerState(InLayerState)
			, ExpectedViewState(InExpectedViewState)
			, ExtractionTarget(InExtractionTarget)
			, DepthExtractionTarget(InDepthExtractionTarget)
			, ExpectedDepthSourceSize(InExpectedDepthSourceSize)
			, ColorSample(InColorSample)
			, MainPublisher(MoveTemp(InMainPublisher))
			, RecursivePublisher(MoveTemp(InRecursivePublisher))
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
				|| !LayerState
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
					if (!SceneColor.IsValid() || !LayerState || !ExtractionTarget)
					{
						return SceneColor;
					}

					const float MeasuredPreExposure = View.State
						? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					LayerState->SecondaryPreExposure.Store(MeasuredPreExposure);
					LayerState->ObservedAAMethod.Store(static_cast<int32>(View.AntiAliasingMethod));
					LayerState->ExtractionInputWidth.Store(SceneColor.ViewRect.Width());
					LayerState->ExtractionInputHeight.Store(SceneColor.ViewRect.Height());

					const FVector2D TemporalJitter = View.ViewMatrices.GetTemporalAAJitter();
					LayerState->LastTemporalJitterX.Store(static_cast<float>(TemporalJitter.X));
					LayerState->LastTemporalJitterY.Store(static_cast<float>(TemporalJitter.Y));
					if (FMath::Abs(TemporalJitter.X) > 1.0e-8
						|| FMath::Abs(TemporalJitter.Y) > 1.0e-8)
					{
						LayerState->bTemporalJitterObserved.Store(true);
					}

					FRDGTextureRef ExtractionTexture =
						ExtractionTarget->GetRenderTargetTexture(GraphBuilder);
					if (!ExtractionTexture)
					{
						return SceneColor;
					}
					GraphBuilder.UseInternalAccessMode(ExtractionTexture);
					AddDrawTexturePass(
						GraphBuilder, View,
						SceneColor.Texture, ExtractionTexture,
						SceneColor.ViewRect.Min, SceneColor.ViewRect.Size(),
						FIntPoint::ZeroValue, ExtractionTexture->Desc.Extent,
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
								LayerState->DepthSourceWidth.Store(SourceSize.X);
								LayerState->DepthSourceHeight.Store(SourceSize.Y);
								LayerState->DepthTargetWidth.Store(DepthExtractionTexture->Desc.Extent.X);
								LayerState->DepthTargetHeight.Store(DepthExtractionTexture->Desc.Extent.Y);
								GraphBuilder.UseInternalAccessMode(DepthExtractionTexture);
								AddDrawTexturePass(
									GraphBuilder, View,
									SceneDepthTexture, DepthExtractionTexture,
									FIntPoint::ZeroValue, SourceSize,
									FIntPoint::ZeroValue, DepthExtractionTexture->Desc.Extent,
									TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
								GraphBuilder.UseExternalAccessMode(
									DepthExtractionTexture, ERHIAccess::SRVMask);
								LayerState->LastDepthExtractionFrame.Store(GFrameCounter);
							}
						}
					}

					ColorSample->PreExposure = MeasuredPreExposure;
					LayerState->LastExtractionFrame.Store(GFrameCounter);
					LayerState->LastCompletedSubmission.Store(ColorSample->Submission);

					if (LayerState->Lifetime.CanPublishPacked(
						CompletedRequest.RendererHistoryGeneration))
					{
						if (MainPublisher)
						{
							MainPublisher->PublishRequest(CompletedRequest);
						}
						else if (RecursivePublisher)
						{
							RecursivePublisher->PublishRequest(CompletedRequest);
						}
					}
					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return LayerState != nullptr
				&& ExtractionTarget != nullptr
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		FLayerState* LayerState = nullptr;
		FSceneViewStateInterface* ExpectedViewState = nullptr;
		FRenderTarget* ExtractionTarget = nullptr;
		FRenderTarget* DepthExtractionTarget = nullptr;
		FIntPoint ExpectedDepthSourceSize = FIntPoint::ZeroValue;
		TSharedRef<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> ColorSample;
		TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> MainPublisher;
		TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> RecursivePublisher;
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

			// FullFidelity lifecycle ownership is established before this producer starts.
			// P1A-3 deliberately leaves every recursion ViewState/lifetime unallocated
			// until that exact endpoint/level enters a visible recursion chain.
			ActiveWorld = World;
			PrimaryResolutionFraction = ReadPrimaryFraction();
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					ResetLayer(*Endpoint.Layers[Level]);
				}
				Endpoint.MainCompositionExtension =
					FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
				Endpoint.MainCompositionExtension->SetEnabled(true);
				Endpoint.LastVisibleDepth = 0;
				Endpoint.LastEffectiveDepth = 0;
				Endpoint.LastAttemptedLayerMask = 0;
				Endpoint.LastSubmittedLayerMask = 0;
			}

			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FMultiVisibleProducer::OnWorldPostActorTick);
			bRunning = true;
			Status = TEXT("RUNNING_RECURSIVE_MULTI_VISIBLE_TSR");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible: START. Endpoints=2 MaxRecursionDepth=4 PrimaryFraction=%.3f SharedFinalScratch=1 LazyPerEndpointPerLevelViewState=1 LazyPerEndpointPerLevelDepth=1."),
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
				FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				if (Endpoint.MainCompositionExtension)
				{
					Endpoint.MainCompositionExtension->SetEnabled(false);
					Endpoint.MainCompositionExtension->ClearRequest();
				}
				for (int32 Level = 1; Level < MaxRecursionDepth; ++Level)
				{
					if (Endpoint.RecursiveCompositionExtensions[Level])
					{
						Endpoint.RecursiveCompositionExtensions[Level]->SetEnabled(false);
						Endpoint.RecursiveCompositionExtensions[Level]->ClearRequest();
					}
				}
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					AdvancePublicationGeneration(*Endpoint.Layers[Level]);
				}
			}

			FlushRenderingCommands();
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				Endpoint.MainCompositionExtension.Reset();
				for (int32 Level = 1; Level < MaxRecursionDepth; ++Level)
				{
					Endpoint.RecursiveCompositionExtensions[Level].Reset();
				}
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					Layer.ViewState.Destroy();
					ReleaseDepthTarget(Layer);
					ResetLifetimeAfterSynchronousTeardown(Layer);
				}
				Endpoint.LastVisibleDepth = 0;
				Endpoint.LastEffectiveDepth = 0;
				Endpoint.LastAttemptedLayerMask = 0;
				Endpoint.LastSubmittedLayerMask = 0;
			}
			ReleaseFinalScratch();
			ActiveWorld.Reset();
			bRunning = false;
			Status = TEXT("STOPPED");
			LastVisibleMask = 0;
			LastSubmittedMask = 0;
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible: STOP. VisibleMask=0x%02x SubmittedMask=0x%02x."),
				LastVisibleMask, LastSubmittedMask);
		}

		bool IsRunning() const { return bRunning; }

		void DumpReport() const
		{
			WriteReport();
			const int32 PublishedMask = BuildPublishedMask();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible P1A3 Requested=%d VisibleEndpointMask=0x%02x SubmittedEndpointMask=0x%02x PublishedEndpointMask=0x%02x Scratch=%dx%d"),
				LastRequestedRecursionDepth, LastVisibleMask, LastSubmittedMask, PublishedMask,
				FinalScratchSize.X, FinalScratchSize.Y);

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				const FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				const int32 PublishedLayerMask = BuildPublishedLayerMask(EndpointIndex);
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible P1A3 Endpoint=%d VisibleDepth=%d EffectiveDepth=%d Attempted=0x%02x Submitted=0x%02x SubmissionCount=%d Published=0x%02x"),
					EndpointIndex, Endpoint.LastVisibleDepth, Endpoint.LastEffectiveDepth,
					Endpoint.LastAttemptedLayerMask, Endpoint.LastSubmittedLayerMask,
					CountSetBits(Endpoint.LastSubmittedLayerMask), PublishedLayerMask);

				AInteriorPortal* Portal = GetEndpointPortal(EndpointIndex);
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const UTextureRenderTarget2D* ColorTarget = GetColorTarget(Portal, Level);
					const bool bViewStateAllocated = Layer.ViewState.GetReference() != nullptr;
					const bool bDepthAllocated = IsValid(Layer.SecondaryDepthTarget);
					const bool bColorAllocated = IsValid(ColorTarget);
					UE_LOG(LogTemp, Display,
						TEXT("PortalMultiVisible P1A3 E%dL%d State=%s Lifetime=%llu PublicationGeneration=%llu PackedIdentity=%llu ViewState=%d Color=%d[%dx%d RTF=%d] Depth=%d[%dx%d RTF=%d] SubmittedFrames=%llu Skipped=%llu Failure=%s"),
						EndpointIndex, Level,
						InteriorPortalRecursionLifetime::ToString(Layer.Lifetime.State),
						Layer.Lifetime.LifetimeId, Layer.Lifetime.PublicationGeneration,
						Layer.HistoryGeneration,
						bViewStateAllocated ? 1 : 0,
						bColorAllocated ? 1 : 0,
						bColorAllocated ? ColorTarget->SizeX : 0,
						bColorAllocated ? ColorTarget->SizeY : 0,
						bColorAllocated ? static_cast<int32>(ColorTarget->RenderTargetFormat.GetValue()) : -1,
						bDepthAllocated ? 1 : 0,
						bDepthAllocated ? Layer.SecondaryDepthTargetSize.X : 0,
						bDepthAllocated ? Layer.SecondaryDepthTargetSize.Y : 0,
						bDepthAllocated ? static_cast<int32>(Layer.SecondaryDepthTarget->RenderTargetFormat.GetValue()) : -1,
						Layer.FramesSubmitted, Layer.FramesSkipped,
						*Layer.LastSubmissionFailureReason);
				}
			}
		}

	private:
		static int32 CountBits(const int32 Mask)
		{
			return ((Mask & 1) ? 1 : 0) + ((Mask & 2) ? 1 : 0);
		}

		static int32 CountSetBits(int32 Mask)
		{
			int32 Count = 0;
			while (Mask != 0)
			{
				Count += Mask & 1;
				Mask >>= 1;
			}
			return Count;
		}

		static uint64 EstimateTargetBytes(const UTextureRenderTarget2D* Target)
		{
			if (!IsValid(Target) || Target->SizeX <= 0 || Target->SizeY <= 0)
			{
				return 0;
			}

			uint64 BytesPerPixel = 0;
			switch (Target->RenderTargetFormat)
			{
			case RTF_R8:
				BytesPerPixel = 1;
				break;
			case RTF_RG8:
			case RTF_R16f:
				BytesPerPixel = 2;
				break;
			case RTF_RGBA8:
			case RTF_RGBA8_SRGB:
			case RTF_RG16f:
			case RTF_R32f:
			case RTF_RGB10A2:
				BytesPerPixel = 4;
				break;
			case RTF_RGBA16f:
			case RTF_RG32f:
				BytesPerPixel = 8;
				break;
			case RTF_RGBA32f:
				BytesPerPixel = 16;
				break;
			default:
				break;
			}
			return static_cast<uint64>(Target->SizeX)
				* static_cast<uint64>(Target->SizeY)
				* BytesPerPixel;
		}

		AInteriorPortal* GetEndpointPortal(const int32 EndpointIndex) const
		{
			AInteriorPortalSystem* PortalSystem = ActiveWorld.IsValid()
				? FindPortalSystem(ActiveWorld.Get()) : nullptr;
			if (!PortalSystem)
			{
				return nullptr;
			}
			return EndpointIndex == 0 ? PortalSystem->BluePortal.Get() : PortalSystem->OrangePortal.Get();
		}

		static UTextureRenderTarget2D* GetColorTarget(AInteriorPortal* Portal, const int32 Level)
		{
			return IsValid(Portal) && Portal->RenderTargets.IsValidIndex(Level)
				? Portal->RenderTargets[Level] : nullptr;
		}

		int32 BuildPublishedLayerMask(const int32 EndpointIndex) const
		{
			if (EndpointIndex < 0 || EndpointIndex >= EndpointCount)
			{
				return 0;
			}
			const FEndpointState& Endpoint = *Endpoints[EndpointIndex];
			int32 Mask = 0;
			if (Endpoint.MainCompositionExtension
				&& Endpoint.MainCompositionExtension->HasPublishedRequest())
			{
				Mask |= 1;
			}
			for (int32 Level = 1; Level < MaxRecursionDepth; ++Level)
			{
				if (Endpoint.RecursiveCompositionExtensions[Level]
					&& Endpoint.RecursiveCompositionExtensions[Level]->HasPublishedRequest())
				{
					Mask |= (1 << Level);
				}
			}
			return Mask;
		}

		void ResetFrameSubmissionDiagnostics()
		{
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				Endpoint.LastEffectiveDepth = 0;
				Endpoint.LastAttemptedLayerMask = 0;
				Endpoint.LastSubmittedLayerMask = 0;
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					Endpoint.Layers[Level]->LastSubmissionFailureReason =
						TEXT("NOT_ATTEMPTED_THIS_FRAME");
				}
			}
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
			if (World == ActiveWorld.Get())
			{
				SubmitVisibleEndpoints(World);
			}
		}

		void SyncPublicationIdentity(FLayerState& Layer)
		{
			const uint64 PackedIdentity = Layer.Lifetime.GetPackedPublicationIdentity();
			Layer.HistoryGeneration = PackedIdentity;
			Layer.ActivePublicationGeneration.Store(PackedIdentity);
		}

		void ResetLayer(FLayerState& Layer)
		{
			Layer.bHistoryValid = false;
			Layer.bVisibleLastTick = false;
			Layer.bLastCameraCut = true;
			Layer.HistoryGeneration = 0;
			Layer.ActivePublicationGeneration.Store(0);
			Layer.LastTargetSize = FIntPoint::ZeroValue;
			Layer.LastEntryFrame = FTransform::Identity;
			Layer.LastExitFrame = FTransform::Identity;
			Layer.LastCameraCutReason = TEXT("producer start");
			Layer.LastSubmissionFailureReason = TEXT("NOT_ATTEMPTED_THIS_FRAME");
			Layer.FramesSubmitted = 0;
			Layer.FramesSkipped = 0;
			Layer.CameraCutCount = 0;
			Layer.ContinuousHistoryFrames = 0;
			Layer.LastExtractionFrame.Store(0);
			Layer.LastDepthExtractionFrame.Store(0);
			Layer.LastCompletedSubmission.Store(0);
			Layer.SecondaryPreExposure.Store(1.0f);
			Layer.ObservedAAMethod.Store(-1);
			Layer.LastTemporalJitterX.Store(0.0f);
			Layer.LastTemporalJitterY.Store(0.0f);
			Layer.bTemporalJitterObserved.Store(false);
			Layer.ExtractionInputWidth.Store(0);
			Layer.ExtractionInputHeight.Store(0);
			Layer.DepthSourceWidth.Store(0);
			Layer.DepthSourceHeight.Store(0);
			Layer.DepthTargetWidth.Store(0);
			Layer.DepthTargetHeight.Store(0);
		}

		bool EnsureLayerViewState(
			UWorld* World,
			FEndpointState& Endpoint,
			const int32 Level,
			const int32 RequiredVisibleDepth)
		{
			if (!World || !World->Scene || Level < 0 || Level >= MaxRecursionDepth)
			{
				return false;
			}

			FLayerState& Layer = *Endpoint.Layers[Level];
			if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Unallocated)
			{
				Layer.ViewState.Allocate(World->GetFeatureLevel());
				if (!Layer.ViewState.GetReference())
				{
					return false;
				}

				const uint64 NewLifetimeId = LifetimeIdSource.Allocate();
				if (!Layer.Lifetime.BeginAllocated(NewLifetimeId))
				{
					Layer.ViewState.Destroy();
					return false;
				}
				SyncPublicationIdentity(Layer);
			}
			else if (!Layer.Lifetime.IsReusable() || !Layer.ViewState.GetReference())
			{
				return false;
			}

			const int32 ChildLevel = Level + 1;
			if (ChildLevel < RequiredVisibleDepth
				&& ChildLevel < MaxRecursionDepth
				&& !Endpoint.RecursiveCompositionExtensions[ChildLevel])
			{
				Endpoint.RecursiveCompositionExtensions[ChildLevel] =
					FSceneViewExtensions::NewExtension<FRecursiveCompositionExtension>(
						World, Layer.ViewState.GetReference());
				Endpoint.RecursiveCompositionExtensions[ChildLevel]->SetEnabled(true);
			}
			return true;
		}

		void AdvancePublicationGeneration(FLayerState& Layer)
		{
			const InteriorPortalRecursionLifetime::FPublicationToken Previous =
				Layer.Lifetime.AdvancePublicationGeneration();
			if (Previous.IsValid())
			{
				SyncPublicationIdentity(Layer);
			}
		}

		void ResetLifetimeAfterSynchronousTeardown(FLayerState& Layer)
		{
			if (Layer.Lifetime.IsReusable())
			{
				InteriorPortalRecursionLifetime::FPublicationToken RetiredPublication;
				Layer.Lifetime.BeginRetirement(RetiredPublication);
			}
			if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Retiring)
			{
				Layer.Lifetime.MarkReclaimable();
			}
			if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Reclaimable)
			{
				Layer.Lifetime.ResetUnallocated();
			}
			SyncPublicationIdentity(Layer);
		}

		void HideLayer(FEndpointState& Endpoint, const int32 Level, const TCHAR* Reason)
		{
			FLayerState& Layer = *Endpoint.Layers[Level];
			if (!Layer.bVisibleLastTick)
			{
				return;
			}

			Layer.bVisibleLastTick = false;
			Layer.bHistoryValid = false;
			Layer.LastCameraCutReason = Reason;
			AdvancePublicationGeneration(Layer);
			const uint64 ActiveGeneration = Layer.ActivePublicationGeneration.Load();

			// Visibility is decided on the game thread, while an older player/parent
			// view family may already be queued on the render thread. Clearing the
			// publication synchronously here creates a one-frame ownership hole: that
			// already-queued view still rasterizes the portal surface but no longer has
			// a BeforeDOF portal request, exposing the spiral fallback. Retire the old
			// publication identity in render-queue order instead. A newly visible
			// identity is never cleared because its packed identity is >= ActiveGeneration.
			if (Level == 0)
			{
				const TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> Publisher =
					Endpoint.MainCompositionExtension;
				if (Publisher)
				{
					ENQUEUE_RENDER_COMMAND(RetirePortalMainPublication)(
						[Publisher, ActiveGeneration](FRHICommandListImmediate& RHICmdList)
						{
							(void)RHICmdList;
							if (!Publisher->HasPublishedRequest())
							{
								return;
							}
							const FInteriorPortalRenderRequest Published = Publisher->GetPublishedRequest();
							if (Published.RendererHistoryGeneration < ActiveGeneration)
							{
								Publisher->ClearRequest();
							}
						});
				}
			}
			else
			{
				const TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> Publisher =
					Endpoint.RecursiveCompositionExtensions[Level];
				if (Publisher)
				{
					ENQUEUE_RENDER_COMMAND(RetirePortalRecursivePublication)(
						[Publisher, ActiveGeneration](FRHICommandListImmediate& RHICmdList)
						{
							(void)RHICmdList;
							if (!Publisher->HasPublishedRequest())
							{
								return;
							}
							const FInteriorPortalRenderRequest Published = Publisher->GetPublishedRequest();
							if (Published.RendererHistoryGeneration < ActiveGeneration)
							{
								Publisher->ClearRequest();
							}
						});
				}
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

		bool EnsureDepthTarget(FLayerState& Layer, UWorld* World, const FIntPoint TargetSize)
		{
			if (!World || TargetSize.X <= 0 || TargetSize.Y <= 0)
			{
				return false;
			}
			if (Layer.SecondaryDepthTarget && Layer.SecondaryDepthTargetSize == TargetSize)
			{
				return true;
			}
			if (Layer.SecondaryDepthTarget)
			{
				FlushRenderingCommands();
				ReleaseDepthTarget(Layer);
			}
			Layer.SecondaryDepthTarget = NewObject<UTextureRenderTarget2D>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Layer.SecondaryDepthTarget)
			{
				return false;
			}
			Layer.SecondaryDepthTarget->AddToRoot();
			Layer.SecondaryDepthTarget->RenderTargetFormat = RTF_R32f;
			Layer.SecondaryDepthTarget->ClearColor = FLinearColor::Black;
			Layer.SecondaryDepthTarget->bForceLinearGamma = true;
			Layer.SecondaryDepthTarget->bAutoGenerateMips = false;
			Layer.SecondaryDepthTarget->InitCustomFormat(
				TargetSize.X, TargetSize.Y, PF_R32_FLOAT, true);
			Layer.SecondaryDepthTarget->UpdateResourceImmediate(true);
			Layer.SecondaryDepthTargetSize = TargetSize;
			return Layer.SecondaryDepthTarget->GameThread_GetRenderTargetResource() != nullptr;
		}

		void ReleaseDepthTarget(FLayerState& Layer)
		{
			if (Layer.SecondaryDepthTarget)
			{
				Layer.SecondaryDepthTarget->RemoveFromRoot();
				Layer.SecondaryDepthTarget = nullptr;
			}
			Layer.SecondaryDepthTargetSize = FIntPoint::ZeroValue;
		}

		bool DetermineCameraCut(
			const FLayerState& Layer,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			FString& OutReason) const
		{
			if (!Layer.bHistoryValid)
			{
				OutReason = TEXT("history invalid / first visible frame");
				return true;
			}
			if (Layer.LastTargetSize != TargetSize)
			{
				OutReason = TEXT("render target size changed");
				return true;
			}
			if (PortalFrameChanged(Layer.LastEntryFrame, EntryFrame)
				|| PortalFrameChanged(Layer.LastExitFrame, ExitFrame))
			{
				OutReason = TEXT("portal logical frame changed");
				return true;
			}
			OutReason = TEXT("continuous endpoint recursion history");
			return false;
		}

		void CommitHistory(
			FLayerState& Layer,
			const FIntPoint TargetSize,
			const FTransform& EntryFrame,
			const FTransform& ExitFrame,
			const bool bCameraCut,
			const FString& CameraCutReason)
		{
			Layer.bLastCameraCut = bCameraCut;
			Layer.LastCameraCutReason = CameraCutReason;
			if (bCameraCut)
			{
				++Layer.CameraCutCount;
			}
			else
			{
				++Layer.ContinuousHistoryFrames;
			}
			Layer.bHistoryValid = true;
			Layer.bVisibleLastTick = true;
			Layer.LastTargetSize = TargetSize;
			Layer.LastEntryFrame = EntryFrame;
			Layer.LastExitFrame = ExitFrame;
		}

		void SubmitVisibleEndpoints(UWorld* World)
		{
			ResetFrameSubmissionDiagnostics();
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager
				|| PortalSystem->RendererBackend != EInteriorPortalRendererBackend::SceneCapture
				|| !PortalSystem->IsLinked())
			{
				for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
				{
					FEndpointState& Endpoint = *Endpoints[EndpointIndex];
					for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
					{
						HideLayer(Endpoint, Level, TEXT("portal pair/player/backend unavailable"));
					}
					Endpoint.LastVisibleDepth = 0;
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

			LastRequestedRecursionDepth = FMath::Clamp(
				PortalSystem->RecursionDepth, 1, MaxRecursionDepth);
			int32 VisibleMask = 0;
			int32 SubmittedMask = 0;
			AInteriorPortal* Candidates[EndpointCount] = {
				PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()
			};

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				AInteriorPortal* Entry = Candidates[EndpointIndex];
				AInteriorPortal* Exit = Candidates[1 - EndpointIndex];
				if (!IsValid(Entry) || !IsValid(Exit))
				{
					for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
					{
						HideLayer(Endpoint, Level, TEXT("endpoint unavailable"));
					}
					Endpoint.LastVisibleDepth = 0;
					continue;
				}

				TArray<FInteriorPortalRenderRequest, TInlineAllocator<MaxRecursionDepth>> Requests;
				FTransform ParentView = PlayerView;
				for (int32 Level = 0; Level < LastRequestedRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const uint64 GeometryGeneration = Layer.HistoryGeneration != 0
						? Layer.HistoryGeneration : 1;
					const FMatrix ParentViewProjection = BuildViewProjection(
						ParentView, ProjectionData.ProjectionMatrix);
					FInteriorPortalRenderRequest Request;
					if (!FInteriorPortalRenderRequest::Build(
						EndpointIndex, EndpointIndex, Level,
						ParentView, Entry->GetLogicalFrame(), Exit->GetLogicalFrame(),
						Entry->HalfWidth, Entry->HalfHeight,
						ParentViewProjection, PlayerRect,
						ProjectionData.ProjectionMatrix,
						ProjectionData.IsPerspectiveProjection(),
						ProjectionData.GetNearPlaneFromProjectionMatrix(),
						PortalSystem->ClipPlaneBias,
						GeometryGeneration, Request)
						|| !Request.IsValid() || !IsFiniteTransform(Request.VirtualView))
					{
						break;
					}

					if (Request.ForegroundDepthReference.bValid)
					{
						Request.ForegroundDepthReference.Row2.W = Entry->SurfaceVisualBias;
					}

					Requests.Add(Request);
					ParentView = Request.VirtualView;
				}

				const int32 VisibleDepth = Requests.Num();
				Endpoint.LastVisibleDepth = VisibleDepth;
				Endpoint.LastEffectiveDepth = VisibleDepth; // P1B is not implemented in P1A-3.
				if (VisibleDepth <= 0)
				{
					for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
					{
						HideLayer(Endpoint, Level, TEXT("endpoint left visible recursion set"));
					}
					continue;
				}

				bool bViewStatesReady = true;
				for (int32 Level = 0; Level < VisibleDepth; ++Level)
				{
					if (!EnsureLayerViewState(World, Endpoint, Level, VisibleDepth))
					{
						Endpoint.Layers[Level]->LastSubmissionFailureReason =
							TEXT("VIEWSTATE_OR_LIFETIME_UNAVAILABLE");
						bViewStatesReady = false;
						break;
					}
					FLayerState& Layer = *Endpoint.Layers[Level];
					Requests[Level].RendererHistoryGeneration = Layer.HistoryGeneration;
					Requests[Level].HistoryIdentity = FInteriorPortalRenderRequest::MakeHistoryIdentity(
						EndpointIndex, Level, Layer.HistoryGeneration);
				}
				if (!bViewStatesReady)
				{
					Endpoint.LastEffectiveDepth = 0;
					continue;
				}

				VisibleMask |= (1 << EndpointIndex);
				Entry->EnsureTargets(TargetSize.X, TargetSize.Y, VisibleDepth);
				for (int32 Level = VisibleDepth; Level < MaxRecursionDepth; ++Level)
				{
					HideLayer(Endpoint, Level, TEXT("recursion level not visible/requested"));
				}

				bool bTopSubmitted = false;
				for (int32 Level = VisibleDepth - 1; Level >= 0; --Level)
				{
					Endpoint.LastAttemptedLayerMask |= (1 << Level);
					if (SubmitLayer(
						World, *PortalSystem, ProjectionData, POV,
						TargetSize, ExpectedPrimarySize,
						EndpointIndex, Level, VisibleDepth,
						Entry, Exit, Endpoint, Requests[Level]))
					{
						Endpoint.LastSubmittedLayerMask |= (1 << Level);
						bTopSubmitted |= Level == 0;
					}
				}
				if (bTopSubmitted)
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
					TEXT("PortalMultiVisible Tick=%llu RequestedRecursion=%d VisibleCount=%d VisibleMask=0x%02x SubmittedMask=0x%02x PublishedMask=0x%02x E0Visible=%d E0Effective=%d E0Attempted=0x%02x E0Submitted=0x%02x E1Visible=%d E1Effective=%d E1Attempted=0x%02x E1Submitted=0x%02x"),
					ProducerTicks, LastRequestedRecursionDepth,
					CountBits(VisibleMask), VisibleMask, SubmittedMask, BuildPublishedMask(),
					Endpoints[0]->LastVisibleDepth, Endpoints[0]->LastEffectiveDepth,
					Endpoints[0]->LastAttemptedLayerMask, Endpoints[0]->LastSubmittedLayerMask,
					Endpoints[1]->LastVisibleDepth, Endpoints[1]->LastEffectiveDepth,
					Endpoints[1]->LastAttemptedLayerMask, Endpoints[1]->LastSubmittedLayerMask);
			}
		}

		bool SubmitLayer(
			UWorld* World,
			AInteriorPortalSystem& PortalSystem,
			const FSceneViewProjectionData& ProjectionData,
			const FMinimalViewInfo& POV,
			const FIntPoint TargetSize,
			const FIntPoint ExpectedPrimarySize,
			const int32 EndpointIndex,
			const int32 Level,
			const int32 VisibleDepth,
			AInteriorPortal* Entry,
			AInteriorPortal* Exit,
			FEndpointState& Endpoint,
			FInteriorPortalRenderRequest Request)
		{
			FLayerState& Layer = *Endpoint.Layers[Level];
			Layer.LastSubmissionFailureReason = TEXT("NONE");
			if (!Layer.Lifetime.CanSubmit(Level, LastRequestedRecursionDepth))
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("LIFETIME_NOT_SUBMITTABLE");
				return false;
			}
			if (!IsValid(Entry) || !IsValid(Exit) || !World->Scene
				|| !Endpoint.MainCompositionExtension
				|| !Entry->RenderTargets.IsValidIndex(Level))
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("PRECONDITION_OR_COLOR_TARGET_UNAVAILABLE");
				return false;
			}
			if (!EnsureDepthTarget(Layer, World, TargetSize))
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("DEPTH_TARGET_UNAVAILABLE");
				return false;
			}

			UTextureRenderTarget2D* PortalTarget = Entry->RenderTargets[Level];
			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* FinalScratchResource = FinalScratch
				? FinalScratch->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* DepthTargetResource = Layer.SecondaryDepthTarget
				? Layer.SecondaryDepthTarget->GameThread_GetRenderTargetResource() : nullptr;
			if (!PortalTargetResource || !FinalScratchResource || !DepthTargetResource)
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("RENDER_RESOURCE_UNAVAILABLE");
				return false;
			}

			Request.PortalRenderTarget = PortalTargetResource;
			Request.PortalDepthRenderTarget = DepthTargetResource;
			Request.ColorSample = MakeShared<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe>(
				Layer.FramesSubmitted + 1);

			TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> MainPublisher;
			TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> RecursivePublisher;
			if (Level == 0)
			{
				MainPublisher = Endpoint.MainCompositionExtension;
			}
			else
			{
				RecursivePublisher = Endpoint.RecursiveCompositionExtensions[Level];
			}

			FSceneViewStateInterface* ExpectedViewState = Layer.ViewState.GetReference();
			TSharedRef<FLayerExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FLayerExtractionExtension>(
					World, &Layer, ExpectedViewState,
					PortalTargetResource, DepthTargetResource,
					ExpectedPrimarySize, Request.ColorSample.ToSharedRef(),
					MainPublisher, RecursivePublisher, Request);

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

			if (Level + 1 < VisibleDepth
				&& Endpoint.RecursiveCompositionExtensions[Level + 1])
			{
				ViewFamily.ViewExtensions.Add(
					Endpoint.RecursiveCompositionExtensions[Level + 1].ToSharedRef());
			}

			FString CameraCutReason;
			const bool bCameraCut = DetermineCameraCut(
				Layer, TargetSize, Request.EntryFrame, Request.ExitFrame, CameraCutReason);

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

			HidePortalPrimitives(Exit, ViewInitOptions);

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

			Layer.Lifetime.MarkActive();
			++Layer.FramesSubmitted;
			CommitHistory(
				Layer, TargetSize, Request.EntryFrame, Request.ExitFrame,
				bCameraCut, CameraCutReason);

			if (CVarMultiVisibleDiagnostics.GetValueOnGameThread() != 0
				&& (Layer.FramesSubmitted == 1 || (Layer.FramesSubmitted % 120) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Endpoint=%d Level=%d Submitted=%llu CameraCut=%d Cuts=%llu Continuous=%llu Lifetime=%llu PublicationGeneration=%llu PackedIdentity=%llu Pre=%.9g ExtractFrame=%llu Completed=%llu"),
					EndpointIndex, Level, Layer.FramesSubmitted,
					bCameraCut ? 1 : 0, Layer.CameraCutCount,
					Layer.ContinuousHistoryFrames,
					Layer.Lifetime.LifetimeId, Layer.Lifetime.PublicationGeneration,
					Layer.HistoryGeneration,
					Layer.SecondaryPreExposure.Load(),
					Layer.LastExtractionFrame.Load(),
					Layer.LastCompletedSubmission.Load());
			}
			return true;
		}

		int32 BuildPublishedMask() const
		{
			int32 Mask = 0;
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				const FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				if (Endpoint.MainCompositionExtension
					&& Endpoint.MainCompositionExtension->HasPublishedRequest())
				{
					Mask |= (1 << EndpointIndex);
				}
			}
			return Mask;
		}

		void WriteReport() const
		{
			const int32 PublishedMask = BuildPublishedMask();
			int32 TotalViewStates = 0;
			int32 TotalColorTargets = 0;
			int32 TotalDepthTargets = 0;
			uint64 TotalExplicitTargetBytes = EstimateTargetBytes(FinalScratch);

			FString EndpointJson;
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				const FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				AInteriorPortal* Portal = GetEndpointPortal(EndpointIndex);
				const int32 PublishedLayerMask = BuildPublishedLayerMask(EndpointIndex);
				int32 OwnedLayerMask = 0;
				int32 ActiveLayerMask = 0;
				int32 RetiringLayerMask = 0;
				int32 ReclaimableLayerMask = 0;
				int32 ViewStateCount = 0;
				int32 ColorTargetCount = 0;
				int32 DepthTargetCount = 0;
				uint64 EndpointExplicitTargetBytes = 0;
				FString LayersJson;

				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const UTextureRenderTarget2D* ColorTarget = GetColorTarget(Portal, Level);
					const bool bViewStateAllocated = Layer.ViewState.GetReference() != nullptr;
					const bool bColorAllocated = IsValid(ColorTarget);
					const bool bDepthAllocated = IsValid(Layer.SecondaryDepthTarget);
					const bool bOwned = bViewStateAllocated || bColorAllocated || bDepthAllocated;
					const bool bAttempted = (Endpoint.LastAttemptedLayerMask & (1 << Level)) != 0;
					const bool bSubmitted = (Endpoint.LastSubmittedLayerMask & (1 << Level)) != 0;
					const bool bPublished = (PublishedLayerMask & (1 << Level)) != 0;
					if (bOwned)
					{
						OwnedLayerMask |= (1 << Level);
					}
					if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Active)
					{
						ActiveLayerMask |= (1 << Level);
					}
					else if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Retiring)
					{
						RetiringLayerMask |= (1 << Level);
					}
					else if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Reclaimable)
					{
						ReclaimableLayerMask |= (1 << Level);
					}
					ViewStateCount += bViewStateAllocated ? 1 : 0;
					ColorTargetCount += bColorAllocated ? 1 : 0;
					DepthTargetCount += bDepthAllocated ? 1 : 0;
					const uint64 ColorBytes = EstimateTargetBytes(ColorTarget);
					const uint64 DepthBytes = EstimateTargetBytes(Layer.SecondaryDepthTarget);
					EndpointExplicitTargetBytes += ColorBytes + DepthBytes;

					const FString EscapedFailure = Layer.LastSubmissionFailureReason.ReplaceCharWithEscapedChar();
					const FString EscapedCutReason = Layer.LastCameraCutReason.ReplaceCharWithEscapedChar();
					LayersJson += FString::Printf(
						TEXT("      {\"level\":%d,\"lifetimeId\":%llu,\"lifetimeIdStatus\":\"IMPLEMENTED\",\"publicationGeneration\":%llu,\"packedPublicationIdentity\":%llu,\"resourceState\":\"%s\",\"resourceStateAuthority\":\"IMPLEMENTED_P1A3\",\"retirementStateStatus\":\"IMPLEMENTED_MODEL\",\"attemptedThisFrame\":%s,\"submittedThisFrame\":%s,\"published\":%s,\"submissionFailureReason\":\"%s\",\"viewStateAllocated\":%s,\"historyGenerationCompat\":%llu,\"activePublicationGenerationCompat\":%llu,\"historyValid\":%s,\"visibleLastTick\":%s,\"framesSubmitted\":%llu,\"framesSkipped\":%llu,\"cameraCutCount\":%llu,\"continuousHistoryFrames\":%llu,\"lastCameraCutReason\":\"%s\",\"lastExtractionFrame\":%llu,\"lastDepthExtractionFrame\":%llu,\"lastCompletedSubmission\":%llu,\"secondaryPreExposure\":%.9g,\"observedAAMethod\":%d,\"temporalJitterObserved\":%s,\"colorTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\"depthTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\"depthSourceWidth\":%d,\"depthSourceHeight\":%d}%s\n"),
						Level,
						Layer.Lifetime.LifetimeId,
						Layer.Lifetime.PublicationGeneration,
						Layer.Lifetime.GetPackedPublicationIdentity(),
						InteriorPortalRecursionLifetime::ToString(Layer.Lifetime.State),
						bAttempted ? TEXT("true") : TEXT("false"),
						bSubmitted ? TEXT("true") : TEXT("false"),
						bPublished ? TEXT("true") : TEXT("false"),
						*EscapedFailure,
						bViewStateAllocated ? TEXT("true") : TEXT("false"),
						Layer.HistoryGeneration,
						Layer.ActivePublicationGeneration.Load(),
						Layer.bHistoryValid ? TEXT("true") : TEXT("false"),
						Layer.bVisibleLastTick ? TEXT("true") : TEXT("false"),
						Layer.FramesSubmitted, Layer.FramesSkipped,
						Layer.CameraCutCount, Layer.ContinuousHistoryFrames,
						*EscapedCutReason,
						Layer.LastExtractionFrame.Load(), Layer.LastDepthExtractionFrame.Load(),
						Layer.LastCompletedSubmission.Load(), Layer.SecondaryPreExposure.Load(),
						Layer.ObservedAAMethod.Load(),
						Layer.bTemporalJitterObserved.Load() ? TEXT("true") : TEXT("false"),
						bColorAllocated ? TEXT("true") : TEXT("false"),
						bColorAllocated ? ColorTarget->SizeX : 0,
						bColorAllocated ? ColorTarget->SizeY : 0,
						bColorAllocated ? static_cast<int32>(ColorTarget->RenderTargetFormat.GetValue()) : -1,
						ColorBytes,
						bDepthAllocated ? TEXT("true") : TEXT("false"),
						bDepthAllocated ? Layer.SecondaryDepthTargetSize.X : 0,
						bDepthAllocated ? Layer.SecondaryDepthTargetSize.Y : 0,
						bDepthAllocated ? static_cast<int32>(Layer.SecondaryDepthTarget->RenderTargetFormat.GetValue()) : -1,
						DepthBytes,
						Layer.DepthSourceWidth.Load(), Layer.DepthSourceHeight.Load(),
						Level + 1 < MaxRecursionDepth ? TEXT(",") : TEXT(""));
				}

				TotalViewStates += ViewStateCount;
				TotalColorTargets += ColorTargetCount;
				TotalDepthTargets += DepthTargetCount;
				TotalExplicitTargetBytes += EndpointExplicitTargetBytes;

				EndpointJson += FString::Printf(
					TEXT("    {\"endpoint\":%d,\"visibleDepth\":%d,\"effectiveDepth\":%d,\"attemptedLayerMask\":%d,\"submittedLayerMask\":%d,\"submissionCount\":%d,\"publishedLayerMask\":%d,\"ownedLayerMask\":%d,\"ownedCount\":%d,\"activeLayerMask\":%d,\"activeCount\":%d,\"retiringLayerMask\":%d,\"retiringCount\":%d,\"reclaimableLayerMask\":%d,\"reclaimableCount\":%d,\"viewStateCount\":%d,\"colorTargetCount\":%d,\"depthTargetCount\":%d,\"explicitTargetEstimatedBytes\":%llu,\"layers\":[\n%s    ]}%s\n"),
					EndpointIndex, Endpoint.LastVisibleDepth, Endpoint.LastEffectiveDepth,
					Endpoint.LastAttemptedLayerMask, Endpoint.LastSubmittedLayerMask,
					CountSetBits(Endpoint.LastSubmittedLayerMask), PublishedLayerMask,
					OwnedLayerMask, CountSetBits(OwnedLayerMask),
					ActiveLayerMask, CountSetBits(ActiveLayerMask),
					RetiringLayerMask, CountSetBits(RetiringLayerMask),
					ReclaimableLayerMask, CountSetBits(ReclaimableLayerMask),
					ViewStateCount, ColorTargetCount, DepthTargetCount,
					EndpointExplicitTargetBytes, *LayersJson,
					EndpointIndex + 1 < EndpointCount ? TEXT(",") : TEXT(""));
			}

			const bool bScratchAllocated = IsValid(FinalScratch);
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"schema\":\"PortalFullFidelityLazyViewState.P1A3.v1\",\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"diagnosticScope\":\"P1A-3 visible-demand ViewState/lifetime allocation; runtime capacity reclaim remains deferred to P1A-4\",\n")
				TEXT("  \"lifetimeIdStatus\":\"IMPLEMENTED\",\n")
				TEXT("  \"retirementStateStatus\":\"IMPLEMENTED_MODEL_NOT_YET_DRIVING_RUNTIME_RECLAIM\",\n")
				TEXT("  \"resourceStateAuthority\":\"IMPLEMENTED_P1A3\",\n")
				TEXT("  \"requestedDepth\":%d,\n")
				TEXT("  \"primaryResolutionFraction\":%.6f,\n")
				TEXT("  \"visibleEndpointCount\":%d,\n")
				TEXT("  \"visibleEndpointMask\":%d,\n")
				TEXT("  \"submittedEndpointMask\":%d,\n")
				TEXT("  \"publishedEndpointMask\":%d,\n")
				TEXT("  \"totals\":{\"viewStates\":%d,\"colorTargets\":%d,\"depthTargets\":%d,\"explicitTargetEstimatedBytes\":%llu},\n")
				TEXT("  \"sharedScratch\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\n")
				TEXT("  \"endpoints\":[\n%s  ],\n")
				TEXT("  \"claimBoundary\":\"P1A-3 allocates ViewState/lifetime only after a level enters a visible recursion chain. Short visibility loss keeps allocated in-budget resources; configured-depth retirement/reclaim is still deferred to P1A-4.\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				LastRequestedRecursionDepth,
				PrimaryResolutionFraction,
				CountBits(LastVisibleMask), LastVisibleMask, LastSubmittedMask, PublishedMask,
				TotalViewStates, TotalColorTargets, TotalDepthTargets, TotalExplicitTargetBytes,
				bScratchAllocated ? TEXT("true") : TEXT("false"),
				bScratchAllocated ? FinalScratchSize.X : 0,
				bScratchAllocated ? FinalScratchSize.Y : 0,
				bScratchAllocated ? static_cast<int32>(FinalScratch->RenderTargetFormat.GetValue()) : -1,
				EstimateTargetBytes(FinalScratch),
				*EndpointJson);

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
		InteriorPortalRecursionLifetime::FLifetimeIdSource LifetimeIdSource;
		TUniquePtr<FEndpointState> Endpoints[EndpointCount];
		UTextureRenderTarget2D* FinalScratch = nullptr;
		FIntPoint FinalScratchSize = FIntPoint::ZeroValue;
		uint64 ProducerTicks = 0;
		int32 LastVisibleMask = 0;
		int32 LastSubmittedMask = 0;
		int32 LastRequestedRecursionDepth = 1;
		FString Status = TEXT("STOPPED");
	};

	TUniquePtr<FMultiVisibleProducer> GMultiVisibleProducer;

	bool StartMultiVisible(UWorld* World)
	{
		if (GMultiVisibleProducer && GMultiVisibleProducer->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalMultiVisible: already running."));
			return true;
		}
		if (!IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("PortalMultiVisible: PIE/Game world unavailable."));
			return false;
		}
		GMultiVisibleProducer = MakeUnique<FMultiVisibleProducer>();
		if (!GMultiVisibleProducer->Start(World))
		{
			GMultiVisibleProducer.Reset();
			UE_LOG(LogTemp, Error, TEXT("PortalMultiVisible: producer startup failed."));
			return false;
		}
		return true;
	}

	void StartMultiVisibleFromConsole()
	{
		StartMultiVisible(FindPlayableWorld());
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
		TEXT("Start endpoint x recursion-level full-fidelity TSR producer."),
		FConsoleCommandDelegate::CreateStatic(&StartMultiVisibleFromConsole));

	FAutoConsoleCommand GStopMultiVisibleCommand(
		TEXT("portal.StopMultiVisibleTSRSpike"),
		TEXT("Stop full-fidelity producer and release endpoint x recursion-level histories/depth targets."),
		FConsoleCommandDelegate::CreateStatic(&StopMultiVisible));

	FAutoConsoleCommand GDumpMultiVisibleCommand(
		TEXT("portal.DumpMultiVisibleTSRSpike"),
		TEXT("Dump endpoint/recursion-aware full-fidelity telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpMultiVisible));
}
