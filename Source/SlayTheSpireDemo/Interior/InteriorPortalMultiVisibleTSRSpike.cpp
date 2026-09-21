#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalProjectedBounds.h"
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
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "LegacyScreenPercentageDriver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "RenderCommandFence.h"
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

	// Depth has not passed through TSR. Read the actual primary-view rectangle
	// from its view uniform rather than deriving it from pooled texture extents.
	class FPortalDepthExtractionPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FPortalDepthExtractionPS);
		SHADER_USE_PARAMETER_STRUCT(FPortalDepthExtractionPS, FGlobalShader);
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceDepthTexture)
			SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
	IMPLEMENT_GLOBAL_SHADER(FPortalDepthExtractionPS,
		"/Project/InteriorPortalDepthExtraction.usf", "MainPS", SF_Pixel);

	TAutoConsoleVariable<int32> CVarMultiVisibleDiagnostics(
		TEXT("portal.MultiVisibleDiagnostics"),
		0,
		TEXT("Endpoint/recursion-aware full-fidelity TSR diagnostics. 0=quiet, 1=periodic telemetry."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarFullFidelityPingPong(
		TEXT("portal.FullFidelityPingPong"),
		// Keep the full-view recovery default until continuous-motion acceptance.
		0,
		TEXT("FullFidelity recursion target policy. 1=two full-coordinate-domain color/depth buffers per visible endpoint with projected viewport/scissor; 0=legacy per-level full-view targets."),
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

	int32 ReadBoundedCompositionPadding()
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(
			TEXT("portal.BoundedMainPassPaddingPixels")))
		{
			return FMath::Clamp(Var->GetInt(), 0, 64);
		}
		return InteriorPortalProjectedBounds::DefaultOverscanPixels;
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
		// Legacy per-level depth ownership is retained behind portal.FullFidelityPingPong=0.
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
		int32 LastPingPongSlot = INDEX_NONE;
		FIntRect LastParentViewRect = FIntRect(0, 0, 0, 0);
		FIntRect LastRenderRect = FIntRect(0, 0, 0, 0);
		float LastProjectedCoverage = 0.0f;

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

	struct FRetiringLayer
	{
		TUniquePtr<FLayerState> Layer;
		// Legacy fallback color output follows the exact same retirement fence as
		// the lifetime that last submitted into it. It is rooted only while detached
		// from AInteriorPortal::RenderTargets and waiting for that fence.
		UTextureRenderTarget2D* LegacyColorTarget = nullptr;
		// Both the publisher of this layer and the consumer bound to its ViewState.
		TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> Publisher;
		TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> ChildConsumer;
		FRenderCommandFence Fence;
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
		UTextureRenderTarget2D* PingPongColor[InteriorPortalProjectedBounds::PingPongBufferCount] = { nullptr, nullptr };
		UTextureRenderTarget2D* PingPongDepth[InteriorPortalProjectedBounds::PingPongBufferCount] = { nullptr, nullptr };
		FIntPoint PingPongTargetSize = FIntPoint::ZeroValue;
		TArray<TUniquePtr<FRetiringLayer>> RetiringLayers;
		int32 LastVisibleDepth = 0;
		int32 LastEffectiveDepth = 0;
		int32 LastAttemptedLayerMask = 0;
		int32 LastSubmittedLayerMask = 0;
	};

	struct FLayerRenderPlan
	{
		FInteriorPortalRenderRequest Request;
		FMatrix ProjectionMatrix = FMatrix::Identity;
		FIntRect ParentViewRect = FIntRect(0, 0, 0, 0);
		FIntRect RenderRect = FIntRect(0, 0, 0, 0);
		float Coverage = 1.0f;
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
			const FIntRect& InExtractionDestinationRect,
			TSharedRef<InteriorPortalRendering::FColorSample, ESPMode::ThreadSafe> InColorSample,
			TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> InMainPublisher,
			TSharedPtr<FRecursiveCompositionExtension, ESPMode::ThreadSafe> InRecursivePublisher,
			const FInteriorPortalRenderRequest& InCompletedRequest)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, LayerState(InLayerState)
			, ExpectedViewState(InExpectedViewState)
			, ExtractionTarget(InExtractionTarget)
			, DepthExtractionTarget(InDepthExtractionTarget)
			, ExtractionDestinationRect(InExtractionDestinationRect)
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

					FIntRect DestinationRect = ExtractionDestinationRect;
					DestinationRect.Min.X = FMath::Clamp(DestinationRect.Min.X, 0, ExtractionTexture->Desc.Extent.X);
					DestinationRect.Min.Y = FMath::Clamp(DestinationRect.Min.Y, 0, ExtractionTexture->Desc.Extent.Y);
					DestinationRect.Max.X = FMath::Clamp(DestinationRect.Max.X, 0, ExtractionTexture->Desc.Extent.X);
					DestinationRect.Max.Y = FMath::Clamp(DestinationRect.Max.Y, 0, ExtractionTexture->Desc.Extent.Y);
					if (DestinationRect.Width() <= 0 || DestinationRect.Height() <= 0)
					{
						return SceneColor;
					}

					GraphBuilder.UseInternalAccessMode(ExtractionTexture);
					AddDrawTexturePass(
						GraphBuilder, View,
						SceneColor.Texture, ExtractionTexture,
						SceneColor.ViewRect.Min, SceneColor.ViewRect.Size(),
						DestinationRect.Min, DestinationRect.Size(),
						TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
					GraphBuilder.UseExternalAccessMode(ExtractionTexture, ERHIAccess::SRVMask);

					bool bDepthExtracted = false;
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
								if (DestinationRect.Max.X <= DepthExtractionTexture->Desc.Extent.X
									&& DestinationRect.Max.Y <= DepthExtractionTexture->Desc.Extent.Y)
								{
									LayerState->DepthTargetWidth.Store(DepthExtractionTexture->Desc.Extent.X);
									LayerState->DepthTargetHeight.Store(DepthExtractionTexture->Desc.Extent.Y);
									GraphBuilder.UseInternalAccessMode(DepthExtractionTexture);
									const FScreenPassRenderTarget DepthOutput(
										DepthExtractionTexture, DestinationRect, ERenderTargetLoadAction::ELoad);
									const FScreenPassTextureViewport DepthViewport(DepthOutput);
									auto* Parameters = GraphBuilder.AllocParameters<FPortalDepthExtractionPS::FParameters>();
									Parameters->View = View.ViewUniformBuffer;
									Parameters->SourceDepthTexture = SceneDepthTexture;
									Parameters->Output = GetScreenPassTextureViewportParameters(DepthViewport);
									Parameters->RenderTargets[0] = DepthOutput.GetRenderTargetBinding();
									TShaderMapRef<FPortalDepthExtractionPS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
									AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("InteriorPortal::ExtractViewDepth"),
										View, DepthViewport, DepthViewport, Shader, Parameters);
									GraphBuilder.UseExternalAccessMode(
										DepthExtractionTexture, ERHIAccess::SRVMask);
									LayerState->LastDepthExtractionFrame.Store(GFrameCounter);
									bDepthExtracted = true;
								}
							}
						}
					}

					if (!bDepthExtracted)
					{
						// Do not publish fresh color paired with stale ping-pong depth.
						return SceneColor;
					}
					if (CVarMultiVisibleDiagnostics.GetValueOnRenderThread() != 0
						&& (ColorSample->Submission == 1 || ColorSample->Submission % 120 == 0))
					{
						UE_LOG(LogTemp, Display, TEXT("PortalExtract E%dL%d ColorSource=%s Destination=%s DepthSource=View.ViewRectMinAndSize"),
							CompletedRequest.EndpointIndex, CompletedRequest.RecursionLevel,
							*SceneColor.ViewRect.ToString(), *DestinationRect.ToString());
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
		FIntRect ExtractionDestinationRect = FIntRect(0, 0, 0, 0);
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

			bPingPongEnabled = CVarFullFidelityPingPong.GetValueOnGameThread() != 0;
			if (bPingPongEnabled)
			{
				if (IConsoleVariable* BoundedComposition = IConsoleManager::Get().FindConsoleVariable(
					TEXT("portal.BoundedMainPassScissor")))
				{
					PreviousBoundedCompositionValue = BoundedComposition->GetInt();
					BoundedComposition->Set(1, ECVF_SetByCode);
					bRestoreBoundedComposition = true;
				}
			}

			// FullFidelity lifecycle ownership is established before this producer starts.
			// Per-level temporal history remains independent; only endpoint output
			// color/depth resources are shared between alternating recursion levels.
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
			Status = bPingPongEnabled
				? TEXT("RUNNING_PING_PONG_PROJECTED_VIEWPORT")
				: TEXT("RUNNING_RECURSIVE_MULTI_VISIBLE_TSR");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalMultiVisible: START. Endpoints=2 MaxRecursionDepth=4 PrimaryFraction=%.3f PingPong=%d SharedFinalScratch=1 PerLevelViewState=1."),
				PrimaryResolutionFraction, bPingPongEnabled ? 1 : 0);
			return true;
		}

		void Stop()
		{
			if (!bRunning && !WorldPostActorTickHandle.IsValid())
			{
				RestoreBoundedComposition();
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
				PollRetirements(Endpoint, true);
				ReleaseLegacyColorTargets(Endpoint);
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
				ReleaseEndpointPingPongTargets(Endpoint);
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
			RestoreBoundedComposition();
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
				TEXT("PortalMultiVisible PingPong=%d Requested=%d VisibleEndpointMask=0x%02x SubmittedEndpointMask=0x%02x PublishedEndpointMask=0x%02x Scratch=%dx%d"),
				bPingPongEnabled ? 1 : 0,
				LastRequestedRecursionDepth, LastVisibleMask, LastSubmittedMask, PublishedMask,
				FinalScratchSize.X, FinalScratchSize.Y);

			for (int32 EndpointIndex = 0; EndpointIndex < EndpointCount; ++EndpointIndex)
			{
				const FEndpointState& Endpoint = *Endpoints[EndpointIndex];
				const int32 PublishedLayerMask = BuildPublishedLayerMask(EndpointIndex);
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Endpoint=%d VisibleDepth=%d EffectiveDepth=%d Attempted=0x%02x Submitted=0x%02x SubmissionCount=%d Published=0x%02x PingPongSize=%dx%d"),
					EndpointIndex, Endpoint.LastVisibleDepth, Endpoint.LastEffectiveDepth,
					Endpoint.LastAttemptedLayerMask, Endpoint.LastSubmittedLayerMask,
					CountSetBits(Endpoint.LastSubmittedLayerMask), PublishedLayerMask,
					Endpoint.PingPongTargetSize.X, Endpoint.PingPongTargetSize.Y);

				AInteriorPortal* Portal = GetEndpointPortal(EndpointIndex);
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Endpoint=%d ColorTargets Active=%d Retiring=%d Owned=%d DepthTargets Active=%d Retiring=%d Owned=%d"),
					EndpointIndex,
					CountActiveColorTargets(Endpoint, Portal),
					CountRetiringColorTargets(Endpoint),
					CountActiveColorTargets(Endpoint, Portal) + CountRetiringColorTargets(Endpoint),
					CountActiveDepthTargets(Endpoint),
					CountRetiringDepthTargets(Endpoint),
					CountActiveDepthTargets(Endpoint) + CountRetiringDepthTargets(Endpoint));
				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const UTextureRenderTarget2D* ColorTarget = GetColorTarget(Endpoint, Portal, Level);
					const UTextureRenderTarget2D* DepthTarget = GetDepthTarget(Endpoint, Layer, Level);
					const bool bViewStateAllocated = Layer.ViewState.GetReference() != nullptr;
					const bool bDepthAllocated = IsValid(DepthTarget);
					const bool bColorAllocated = IsValid(ColorTarget);
					UE_LOG(LogTemp, Display,
						TEXT("PortalMultiVisible E%dL%d State=%s Lifetime=%llu Slot=%d Parent=(%d,%d)-(%d,%d) Render=(%d,%d)-(%d,%d) Coverage=%.5f ViewState=%d Color=%d[%dx%d] Depth=%d[%dx%d] SubmittedFrames=%llu Skipped=%llu Failure=%s"),
						EndpointIndex, Level,
						InteriorPortalRecursionLifetime::ToString(Layer.Lifetime.State),
						Layer.Lifetime.LifetimeId, Layer.LastPingPongSlot,
						Layer.LastParentViewRect.Min.X, Layer.LastParentViewRect.Min.Y,
						Layer.LastParentViewRect.Max.X, Layer.LastParentViewRect.Max.Y,
						Layer.LastRenderRect.Min.X, Layer.LastRenderRect.Min.Y,
						Layer.LastRenderRect.Max.X, Layer.LastRenderRect.Max.Y,
						Layer.LastProjectedCoverage,
						bViewStateAllocated ? 1 : 0,
						bColorAllocated ? 1 : 0,
						bColorAllocated ? ColorTarget->SizeX : 0,
						bColorAllocated ? ColorTarget->SizeY : 0,
						bDepthAllocated ? 1 : 0,
						bDepthAllocated ? DepthTarget->SizeX : 0,
						bDepthAllocated ? DepthTarget->SizeY : 0,
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

		void RestoreBoundedComposition()
		{
			if (!bRestoreBoundedComposition)
			{
				return;
			}
			if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(
				TEXT("portal.BoundedMainPassScissor")))
			{
				Var->Set(PreviousBoundedCompositionValue, ECVF_SetByCode);
			}
			bRestoreBoundedComposition = false;
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

		const UTextureRenderTarget2D* GetColorTarget(
			const FEndpointState& Endpoint,
			AInteriorPortal* Portal,
			const int32 Level) const
		{
			if (bPingPongEnabled)
			{
				const int32 Slot = InteriorPortalProjectedBounds::PingPongSlotForLevel(Level);
				return Slot >= 0 && Slot < InteriorPortalProjectedBounds::PingPongBufferCount
					? Endpoint.PingPongColor[Slot] : nullptr;
			}
			return IsValid(Portal) && Portal->RenderTargets.IsValidIndex(Level)
				? Portal->RenderTargets[Level] : nullptr;
		}

		const UTextureRenderTarget2D* GetDepthTarget(
			const FEndpointState& Endpoint,
			const FLayerState& Layer,
			const int32 Level) const
		{
			if (bPingPongEnabled)
			{
				const int32 Slot = InteriorPortalProjectedBounds::PingPongSlotForLevel(Level);
				return Slot >= 0 && Slot < InteriorPortalProjectedBounds::PingPongBufferCount
					? Endpoint.PingPongDepth[Slot] : nullptr;
			}
			return Layer.SecondaryDepthTarget;
		}

		int32 CountActiveColorTargets(
			const FEndpointState& Endpoint,
			const AInteriorPortal* Portal) const
		{
			int32 Count = 0;
			if (bPingPongEnabled)
			{
				for (int32 Slot = 0; Slot < InteriorPortalProjectedBounds::PingPongBufferCount; ++Slot)
				{
					Count += IsValid(Endpoint.PingPongColor[Slot]) ? 1 : 0;
				}
				return Count;
			}
			if (!IsValid(Portal))
			{
				return 0;
			}
			for (const UTextureRenderTarget2D* Target : Portal->RenderTargets)
			{
				Count += IsValid(Target) ? 1 : 0;
			}
			return Count;
		}

		int32 CountRetiringColorTargets(const FEndpointState& Endpoint) const
		{
			if (bPingPongEnabled)
			{
				return 0;
			}
			int32 Count = 0;
			for (const auto& Retiring : Endpoint.RetiringLayers)
			{
				Count += IsValid(Retiring->LegacyColorTarget) ? 1 : 0;
			}
			return Count;
		}

		int32 CountActiveDepthTargets(const FEndpointState& Endpoint) const
		{
			int32 Count = 0;
			if (bPingPongEnabled)
			{
				for (int32 Slot = 0; Slot < InteriorPortalProjectedBounds::PingPongBufferCount; ++Slot)
				{
					Count += IsValid(Endpoint.PingPongDepth[Slot]) ? 1 : 0;
				}
				return Count;
			}
			for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
			{
				Count += IsValid(Endpoint.Layers[Level]->SecondaryDepthTarget) ? 1 : 0;
			}
			return Count;
		}

		int32 CountRetiringDepthTargets(const FEndpointState& Endpoint) const
		{
			if (bPingPongEnabled)
			{
				return 0;
			}
			int32 Count = 0;
			for (const auto& Retiring : Endpoint.RetiringLayers)
			{
				Count += IsValid(Retiring->Layer->SecondaryDepthTarget) ? 1 : 0;
			}
			return Count;
		}

		void ReleaseLegacyColorTargets(FEndpointState& Endpoint)
		{
			AInteriorPortal* Portal = GetEndpointPortal(Endpoint.EndpointIndex);
			if (!IsValid(Portal))
			{
				return;
			}
			for (UTextureRenderTarget2D* Target : Portal->RenderTargets)
			{
				if (IsValid(Target))
				{
					Target->ReleaseResource();
				}
			}
			Portal->RenderTargets.Reset();
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

		void PollRetirements(FEndpointState& Endpoint, const bool bSynchronousStop = false)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_PollRetirements);
			for (int32 Index = Endpoint.RetiringLayers.Num() - 1; Index >= 0; --Index)
			{
				FRetiringLayer& Retiring = *Endpoint.RetiringLayers[Index];
				if (!bSynchronousStop && !Retiring.Fence.IsFenceComplete()) { continue; }
				FLayerState& Layer = *Retiring.Layer;
				Layer.Lifetime.MarkReclaimable();
				Retiring.Publisher.Reset();
				Retiring.ChildConsumer.Reset();
				Layer.ViewState.Destroy();
				ReleaseDepthTarget(Layer);
				if (IsValid(Retiring.LegacyColorTarget))
				{
					Retiring.LegacyColorTarget->ReleaseResource();
					Retiring.LegacyColorTarget->RemoveFromRoot();
					Retiring.LegacyColorTarget = nullptr;
				}
				Layer.Lifetime.ResetUnallocated();
				Endpoint.RetiringLayers.RemoveAt(Index);
			}
		}

		int32 CountRetiring(const FEndpointState& Endpoint, const int32 Level) const
		{
			int32 Count = 0;
			for (const auto& Retiring : Endpoint.RetiringLayers)
			{
				Count += Retiring->Layer->Level == Level ? 1 : 0;
			}
			return Count;
		}

		void UpdateCapacity(const int32 RequestedDepth)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_UpdateCapacity);
			for (auto& EndpointPtr : Endpoints)
			{
				FEndpointState& Endpoint = *EndpointPtr;
				PollRetirements(Endpoint);
				for (int32 Level = MaxRecursionDepth - 1; Level >= RequestedDepth; --Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					InteriorPortalRecursionLifetime::FPublicationToken Token;
					if (!Layer.Lifetime.BeginRetirement(Token)) { continue; }
					Layer.ActivePublicationGeneration.Store(0);
					Layer.bVisibleLastTick = false;
					Layer.bHistoryValid = false;
					auto Retiring = MakeUnique<FRetiringLayer>();
					Retiring->Layer = MoveTemp(Endpoint.Layers[Level]);
					if (!bPingPongEnabled)
					{
						AInteriorPortal* Portal = GetEndpointPortal(Endpoint.EndpointIndex);
						if (IsValid(Portal) && Portal->RenderTargets.IsValidIndex(Level))
						{
							UTextureRenderTarget2D* ColorTarget = Portal->RenderTargets[Level];
							if (IsValid(ColorTarget))
							{
								TRACE_CPUPROFILER_EVENT_SCOPE(Portal_RetireLegacyColorTarget);
								ColorTarget->AddToRoot();
								Retiring->LegacyColorTarget = ColorTarget;
							}
							// Capacity is indexed by recursion level, so shrinking in descending
							// order keeps all retained lower levels at their original indices.
							Portal->RenderTargets.RemoveAt(Level, 1, EAllowShrinking::No);
						}
					}
					Retiring->Publisher = MoveTemp(Endpoint.RecursiveCompositionExtensions[Level]);
					if (Level + 1 < MaxRecursionDepth)
					{
						Retiring->ChildConsumer = MoveTemp(Endpoint.RecursiveCompositionExtensions[Level + 1]);
					}
					// Queue-local publishers are detached before any replacement is installed.
					// Clear only these old objects after their previously queued consumers.
					ENQUEUE_RENDER_COMMAND(RetirePortalCapacity)(
						[Publisher = Retiring->Publisher, Child = Retiring->ChildConsumer](FRHICommandListImmediate&)
						{
							if (Publisher) { Publisher->ClearRequest(); }
							if (Child) { Child->ClearRequest(); }
						});
					Retiring->Fence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
					Endpoint.RetiringLayers.Add(MoveTemp(Retiring));
					Endpoint.Layers[Level] = MakeUnique<FLayerState>(Level);
				}
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
			Layer.LastPingPongSlot = INDEX_NONE;
			Layer.LastParentViewRect = FIntRect(0, 0, 0, 0);
			Layer.LastRenderRect = FIntRect(0, 0, 0, 0);
			Layer.LastProjectedCoverage = 0.0f;
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
			Layer.DepthTargetWidth.Store(0);
			Layer.DepthTargetHeight.Store(0);
		}

		bool EnsureLayerViewState(
			UWorld* World,
			FEndpointState& Endpoint,
			const int32 Level,
			const int32 RequiredVisibleDepth)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_EnsureLayerViewState);
			if (!World || !World->Scene || Level < 0 || Level >= MaxRecursionDepth)
			{
				return false;
			}

			FLayerState& Layer = *Endpoint.Layers[Level];
			if (Layer.Lifetime.State == InteriorPortalRecursionLifetime::EResourceState::Unallocated)
			{
				if (!InteriorPortalRecursionLifetime::CanAllocateReplacement(CountRetiring(Endpoint, Level)))
				{
					return false;
				}
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Portal_ViewStateAllocate);
					Layer.ViewState.Allocate(World->GetFeatureLevel());
				}
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
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Portal_CreateRecursiveExtension);
					Endpoint.RecursiveCompositionExtensions[ChildLevel] =
						FSceneViewExtensions::NewExtension<FRecursiveCompositionExtension>(
							World, Layer.ViewState.GetReference());
				}
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
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_EnsureFinalScratch);
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

		UTextureRenderTarget2D* CreatePingPongTarget(
			const FIntPoint TargetSize,
			const ETextureRenderTargetFormat Format,
			const EPixelFormat PixelFormat,
			const bool bForceLinearGamma)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_CreatePingPongTarget);
			UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Target)
			{
				return nullptr;
			}
			Target->AddToRoot();
			Target->RenderTargetFormat = Format;
			Target->ClearColor = FLinearColor::Black;
			Target->bForceLinearGamma = bForceLinearGamma;
			Target->bAutoGenerateMips = false;
			Target->InitCustomFormat(TargetSize.X, TargetSize.Y, PixelFormat, true);
			Target->UpdateResourceImmediate(true);
			return Target;
		}

		bool EnsureEndpointPingPongTargets(
			FEndpointState& Endpoint,
			UWorld* World,
			const FIntPoint TargetSize,
			const int32 RequiredSlots)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_EnsureEndpointPingPongTargets);
			if (!World || TargetSize.X <= 0 || TargetSize.Y <= 0
				|| RequiredSlots <= 0 || RequiredSlots > InteriorPortalProjectedBounds::PingPongBufferCount)
			{
				return false;
			}

			if (Endpoint.PingPongTargetSize != FIntPoint::ZeroValue
				&& Endpoint.PingPongTargetSize != TargetSize)
			{
				FlushRenderingCommands();
				ReleaseEndpointPingPongTargets(Endpoint);
			}

			for (int32 Slot = 0; Slot < RequiredSlots; ++Slot)
			{
				if (!Endpoint.PingPongColor[Slot])
				{
					Endpoint.PingPongColor[Slot] = CreatePingPongTarget(
						TargetSize, RTF_RGBA16f, PF_FloatRGBA, true);
				}
				if (!Endpoint.PingPongDepth[Slot])
				{
					Endpoint.PingPongDepth[Slot] = CreatePingPongTarget(
						TargetSize, RTF_R32f, PF_R32_FLOAT, true);
				}
				if (!Endpoint.PingPongColor[Slot] || !Endpoint.PingPongDepth[Slot])
				{
					ReleaseEndpointPingPongTargets(Endpoint);
					return false;
				}
			}
			Endpoint.PingPongTargetSize = TargetSize;
			return true;
		}

		void ReleaseEndpointPingPongTargets(FEndpointState& Endpoint)
		{
			for (int32 Slot = 0; Slot < InteriorPortalProjectedBounds::PingPongBufferCount; ++Slot)
			{
				if (Endpoint.PingPongColor[Slot])
				{
					Endpoint.PingPongColor[Slot]->RemoveFromRoot();
					Endpoint.PingPongColor[Slot] = nullptr;
				}
				if (Endpoint.PingPongDepth[Slot])
				{
					Endpoint.PingPongDepth[Slot]->RemoveFromRoot();
					Endpoint.PingPongDepth[Slot] = nullptr;
				}
			}
			Endpoint.PingPongTargetSize = FIntPoint::ZeroValue;
		}

		bool EnsureDepthTarget(FLayerState& Layer, UWorld* World, const FIntPoint TargetSize)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_EnsureDepthTarget);
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
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_ReleaseDepthTarget);
			if (Layer.SecondaryDepthTarget)
			{
				// Runtime capacity retirement reaches here only after the retiring
				// lifetime's RHI-thread-depth fence has completed. Explicitly release
				// the render resource instead of waiting for a later UObject GC pass.
				Layer.SecondaryDepthTarget->ReleaseResource();
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
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_SubmitVisibleEndpoints);
			ResetFrameSubmissionDiagnostics();
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			if (PortalSystem)
			{
				LastRequestedRecursionDepth = FMath::Clamp(PortalSystem->RecursionDepth, 1, MaxRecursionDepth);
			}
			// Capacity retirement must run even without a linked/visible pair or viewport.
			UpdateCapacity(LastRequestedRecursionDepth);
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
			const FIntRect TargetRect(0, 0, TargetSize.X, TargetSize.Y);

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

				TArray<FLayerRenderPlan, TInlineAllocator<MaxRecursionDepth>> Plans;
				FTransform ParentView = PlayerView;
				FMatrix ParentProjection = ProjectionData.ProjectionMatrix;
				FIntRect ParentViewRect = bPingPongEnabled ? TargetRect : PlayerRect;
				for (int32 Level = 0; Level < LastRequestedRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const uint64 GeometryGeneration = Layer.HistoryGeneration != 0
						? Layer.HistoryGeneration : 1;
					const FMatrix ParentViewProjection = BuildViewProjection(
						ParentView, ParentProjection);
					FInteriorPortalRenderRequest Request;
					if (!FInteriorPortalRenderRequest::Build(
						EndpointIndex, EndpointIndex, Level,
						ParentView, Entry->GetLogicalFrame(), Exit->GetLogicalFrame(),
						Entry->HalfWidth, Entry->HalfHeight,
						ParentViewProjection, ParentViewRect,
						ParentProjection,
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

					FLayerRenderPlan Plan;
					Plan.Request = Request;
					Plan.ParentViewRect = ParentViewRect;
					if (bPingPongEnabled)
					{
						if (!InteriorPortalProjectedBounds::ExpandAndClampRect(
							Request.ScissorRect, ParentViewRect,
							ReadBoundedCompositionPadding(), Plan.RenderRect))
						{
							break;
						}
						if (!InteriorPortalProjectedBounds::BuildCroppedProjection(
							ParentProjection, ParentViewRect, Plan.RenderRect, Plan.ProjectionMatrix))
						{
							break;
						}
						const int64 ParentPixels = int64(ParentViewRect.Width()) * int64(ParentViewRect.Height());
						const int64 RenderPixels = int64(Plan.RenderRect.Width()) * int64(Plan.RenderRect.Height());
						Plan.Coverage = ParentPixels > 0
							? float(double(RenderPixels) / double(ParentPixels)) : 1.0f;
						Plan.Request.ProjectionMatrix = Plan.ProjectionMatrix;
					}
					else
					{
						Plan.RenderRect = TargetRect;
						Plan.ProjectionMatrix = ProjectionData.ProjectionMatrix;
						Plan.Coverage = 1.0f;
					}

					Plans.Add(Plan);
					ParentView = Request.VirtualView;
					if (bPingPongEnabled)
					{
						ParentProjection = Plan.ProjectionMatrix;
						ParentViewRect = Plan.RenderRect;
					}
				}

				int32 VisibleDepth = Plans.Num();
				Endpoint.LastVisibleDepth = VisibleDepth;
				Endpoint.LastEffectiveDepth = VisibleDepth;
				if (VisibleDepth <= 0)
				{
					for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
					{
						HideLayer(Endpoint, Level, TEXT("endpoint left visible recursion set"));
					}
					continue;
				}

				for (int32 Level = 0; Level < VisibleDepth; ++Level)
				{
					if (!EnsureLayerViewState(World, Endpoint, Level, VisibleDepth))
					{
						Endpoint.Layers[Level]->LastSubmissionFailureReason =
							TEXT("VIEWSTATE_OR_LIFETIME_UNAVAILABLE");
						VisibleDepth = Level;
						Endpoint.LastEffectiveDepth = VisibleDepth;
						break;
					}
					FLayerState& Layer = *Endpoint.Layers[Level];
					Plans[Level].Request.RendererHistoryGeneration = Layer.HistoryGeneration;
					Plans[Level].Request.HistoryIdentity = FInteriorPortalRenderRequest::MakeHistoryIdentity(
						EndpointIndex, Level, Layer.HistoryGeneration);
				}
				if (VisibleDepth == 0)
				{
					Endpoint.LastEffectiveDepth = 0;
					continue;
				}

				VisibleMask |= (1 << EndpointIndex);
				if (bPingPongEnabled)
				{
					const int32 RequiredSlots = VisibleDepth > 1 ? 2 : 1;
					if (!EnsureEndpointPingPongTargets(Endpoint, World, TargetSize, RequiredSlots))
					{
						Endpoint.LastEffectiveDepth = 0;
						for (int32 Level = 0; Level < VisibleDepth; ++Level)
						{
							Endpoint.Layers[Level]->LastSubmissionFailureReason = TEXT("PING_PONG_TARGET_UNAVAILABLE");
						}
						continue;
					}
				}
				else
				{
					Entry->EnsureTargets(TargetSize.X, TargetSize.Y, VisibleDepth);
				}

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
						TargetSize,
						EndpointIndex, Level, VisibleDepth,
						Entry, Exit, Endpoint, Plans[Level]))
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
					TEXT("PortalMultiVisible Tick=%llu PingPong=%d RequestedRecursion=%d VisibleCount=%d VisibleMask=0x%02x SubmittedMask=0x%02x PublishedMask=0x%02x E0Visible=%d E0Effective=%d E0Attempted=0x%02x E0Submitted=0x%02x E1Visible=%d E1Effective=%d E1Attempted=0x%02x E1Submitted=0x%02x"),
					ProducerTicks, bPingPongEnabled ? 1 : 0, LastRequestedRecursionDepth,
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
			const int32 EndpointIndex,
			const int32 Level,
			const int32 VisibleDepth,
			AInteriorPortal* Entry,
			AInteriorPortal* Exit,
			FEndpointState& Endpoint,
			const FLayerRenderPlan& Plan)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Portal_SubmitLayer);
			FLayerState& Layer = *Endpoint.Layers[Level];
			FInteriorPortalRenderRequest Request = Plan.Request;
			Layer.LastSubmissionFailureReason = TEXT("NONE");
			if (!Layer.Lifetime.CanSubmit(Level, LastRequestedRecursionDepth))
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("LIFETIME_NOT_SUBMITTABLE");
				return false;
			}
			if (!IsValid(Entry) || !IsValid(Exit) || !World->Scene
				|| !Endpoint.MainCompositionExtension)
			{
				++Layer.FramesSkipped;
				Layer.LastSubmissionFailureReason = TEXT("PRECONDITION_UNAVAILABLE");
				return false;
			}

			UTextureRenderTarget2D* PortalTarget = nullptr;
			UTextureRenderTarget2D* DepthTarget = nullptr;
			int32 PingPongSlot = INDEX_NONE;
			if (bPingPongEnabled)
			{
				PingPongSlot = InteriorPortalProjectedBounds::PingPongSlotForLevel(Level);
				if (PingPongSlot < 0 || PingPongSlot >= InteriorPortalProjectedBounds::PingPongBufferCount)
				{
					++Layer.FramesSkipped;
					Layer.LastSubmissionFailureReason = TEXT("PING_PONG_SLOT_INVALID");
					return false;
				}
				PortalTarget = Endpoint.PingPongColor[PingPongSlot];
				DepthTarget = Endpoint.PingPongDepth[PingPongSlot];
			}
			else
			{
				if (!Entry->RenderTargets.IsValidIndex(Level) || !EnsureDepthTarget(Layer, World, TargetSize))
				{
					++Layer.FramesSkipped;
					Layer.LastSubmissionFailureReason = TEXT("LEGACY_TARGET_UNAVAILABLE");
					return false;
				}
				PortalTarget = Entry->RenderTargets[Level];
				DepthTarget = Layer.SecondaryDepthTarget;
			}

			FRenderTarget* PortalTargetResource = PortalTarget
				? PortalTarget->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* FinalScratchResource = FinalScratch
				? FinalScratch->GameThread_GetRenderTargetResource() : nullptr;
			FRenderTarget* DepthTargetResource = DepthTarget
				? DepthTarget->GameThread_GetRenderTargetResource() : nullptr;
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

			const FIntRect ExtractionRect = bPingPongEnabled
				? Plan.RenderRect
				: FIntRect(0, 0, TargetSize.X, TargetSize.Y);
			FSceneViewStateInterface* ExpectedViewState = Layer.ViewState.GetReference();
			TSharedRef<FLayerExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
				FSceneViewExtensions::NewExtension<FLayerExtractionExtension>(
					World, &Layer, ExpectedViewState,
					PortalTargetResource, DepthTargetResource,
					ExtractionRect,
					Request.ColorSample.ToSharedRef(),
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
			ViewInitOptions.SetViewRectangle(ExtractionRect);
			ViewInitOptions.ViewOrigin = Request.ViewLocation;
			ViewInitOptions.ViewLocation = Request.ViewLocation;
			ViewInitOptions.ViewRotation = Request.VirtualView.Rotator();
			ViewInitOptions.ViewRotationMatrix = Request.ViewRotationMatrix;
			ViewInitOptions.ProjectionMatrix = bPingPongEnabled
				? Plan.ProjectionMatrix
				: ProjectionData.ProjectionMatrix;
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

			Layer.LastPingPongSlot = PingPongSlot;
			Layer.LastParentViewRect = Plan.ParentViewRect;
			Layer.LastRenderRect = ExtractionRect;
			Layer.LastProjectedCoverage = Plan.Coverage;
			Layer.Lifetime.MarkActive();
			++Layer.FramesSubmitted;
			CommitHistory(
				Layer, TargetSize, Request.EntryFrame, Request.ExitFrame,
				bCameraCut, CameraCutReason);

			if (CVarMultiVisibleDiagnostics.GetValueOnGameThread() != 0
				&& (Layer.FramesSubmitted == 1 || (Layer.FramesSubmitted % 120) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalMultiVisible Endpoint=%d Level=%d Slot=%d Rect=(%d,%d)-(%d,%d) Coverage=%.5f Submitted=%llu CameraCut=%d Cuts=%llu Continuous=%llu Lifetime=%llu PublicationGeneration=%llu PackedIdentity=%llu Pre=%.9g ExtractFrame=%llu Completed=%llu"),
					EndpointIndex, Level, PingPongSlot,
					ExtractionRect.Min.X, ExtractionRect.Min.Y,
					ExtractionRect.Max.X, ExtractionRect.Max.Y,
					Plan.Coverage,
					Layer.FramesSubmitted,
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
				int32 ActiveColorTargetCount = 0;
				int32 RetiringColorTargetCount = 0;
				int32 DepthTargetCount = 0;
				int32 ActiveDepthTargetCount = 0;
				int32 RetiringDepthTargetCount = 0;
				uint64 EndpointExplicitTargetBytes = 0;
				FString LayersJson;

				if (bPingPongEnabled)
				{
					for (int32 Slot = 0; Slot < InteriorPortalProjectedBounds::PingPongBufferCount; ++Slot)
					{
						if (IsValid(Endpoint.PingPongColor[Slot]))
						{
							++ColorTargetCount;
							++ActiveColorTargetCount;
							EndpointExplicitTargetBytes += EstimateTargetBytes(Endpoint.PingPongColor[Slot]);
						}
						if (IsValid(Endpoint.PingPongDepth[Slot]))
						{
							++DepthTargetCount;
							++ActiveDepthTargetCount;
							EndpointExplicitTargetBytes += EstimateTargetBytes(Endpoint.PingPongDepth[Slot]);
						}
					}
				}

				for (int32 Level = 0; Level < MaxRecursionDepth; ++Level)
				{
					FLayerState& Layer = *Endpoint.Layers[Level];
					const UTextureRenderTarget2D* ColorTarget = GetColorTarget(Endpoint, Portal, Level);
					const UTextureRenderTarget2D* DepthTarget = GetDepthTarget(Endpoint, Layer, Level);
					const bool bViewStateAllocated = Layer.ViewState.GetReference() != nullptr;
					const bool bColorAllocated = IsValid(ColorTarget);
					const bool bDepthAllocated = IsValid(DepthTarget);
					const bool bOwned = bViewStateAllocated
						|| (!bPingPongEnabled && (bColorAllocated || bDepthAllocated));
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
					const uint64 ColorBytes = EstimateTargetBytes(ColorTarget);
					const uint64 DepthBytes = EstimateTargetBytes(DepthTarget);
					if (!bPingPongEnabled)
					{
						ColorTargetCount += bColorAllocated ? 1 : 0;
						ActiveColorTargetCount += bColorAllocated ? 1 : 0;
						DepthTargetCount += bDepthAllocated ? 1 : 0;
						ActiveDepthTargetCount += bDepthAllocated ? 1 : 0;
						EndpointExplicitTargetBytes += ColorBytes + DepthBytes;
					}

					const FString EscapedFailure = Layer.LastSubmissionFailureReason.ReplaceCharWithEscapedChar();
					const FString EscapedCutReason = Layer.LastCameraCutReason.ReplaceCharWithEscapedChar();
					LayersJson += FString::Printf(
						TEXT("      {\"level\":%d,\"lifetimeId\":%llu,\"publicationGeneration\":%llu,\"packedPublicationIdentity\":%llu,\"resourceState\":\"%s\",\"attemptedThisFrame\":%s,\"submittedThisFrame\":%s,\"published\":%s,\"submissionFailureReason\":\"%s\",\"viewStateAllocated\":%s,\"historyValid\":%s,\"visibleLastTick\":%s,\"framesSubmitted\":%llu,\"framesSkipped\":%llu,\"cameraCutCount\":%llu,\"continuousHistoryFrames\":%llu,\"lastCameraCutReason\":\"%s\",\"lastExtractionFrame\":%llu,\"lastDepthExtractionFrame\":%llu,\"lastCompletedSubmission\":%llu,\"secondaryPreExposure\":%.9g,\"observedAAMethod\":%d,\"temporalJitterObserved\":%s,\"pingPongSlot\":%d,\"sharedPingPongTargets\":%s,\"parentViewRect\":{\"minX\":%d,\"minY\":%d,\"maxX\":%d,\"maxY\":%d},\"renderRect\":{\"minX\":%d,\"minY\":%d,\"maxX\":%d,\"maxY\":%d},\"projectedCoverage\":%.9g,\"colorTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\"depthTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\"depthSourceRectAuthority\":\"View.ViewRectMinAndSize\"}%s\n"),
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
						Layer.bHistoryValid ? TEXT("true") : TEXT("false"),
						Layer.bVisibleLastTick ? TEXT("true") : TEXT("false"),
						Layer.FramesSubmitted, Layer.FramesSkipped,
						Layer.CameraCutCount, Layer.ContinuousHistoryFrames,
						*EscapedCutReason,
						Layer.LastExtractionFrame.Load(), Layer.LastDepthExtractionFrame.Load(),
						Layer.LastCompletedSubmission.Load(), Layer.SecondaryPreExposure.Load(),
						Layer.ObservedAAMethod.Load(),
						Layer.bTemporalJitterObserved.Load() ? TEXT("true") : TEXT("false"),
						Layer.LastPingPongSlot,
						bPingPongEnabled ? TEXT("true") : TEXT("false"),
						Layer.LastParentViewRect.Min.X, Layer.LastParentViewRect.Min.Y,
						Layer.LastParentViewRect.Max.X, Layer.LastParentViewRect.Max.Y,
						Layer.LastRenderRect.Min.X, Layer.LastRenderRect.Min.Y,
						Layer.LastRenderRect.Max.X, Layer.LastRenderRect.Max.Y,
						Layer.LastProjectedCoverage,
						bColorAllocated ? TEXT("true") : TEXT("false"),
						bColorAllocated ? ColorTarget->SizeX : 0,
						bColorAllocated ? ColorTarget->SizeY : 0,
						bColorAllocated ? static_cast<int32>(ColorTarget->RenderTargetFormat.GetValue()) : -1,
						ColorBytes,
						bDepthAllocated ? TEXT("true") : TEXT("false"),
						bDepthAllocated ? DepthTarget->SizeX : 0,
						bDepthAllocated ? DepthTarget->SizeY : 0,
						bDepthAllocated ? static_cast<int32>(DepthTarget->RenderTargetFormat.GetValue()) : -1,
						DepthBytes,
						Level + 1 < MaxRecursionDepth ? TEXT(",") : TEXT(""));
				}

				FString RetirementsJson;
				for (const auto& Retiring : Endpoint.RetiringLayers)
				{
					FLayerState& Old = *Retiring->Layer;
					OwnedLayerMask |= 1 << Old.Level;
					RetiringLayerMask |= 1 << Old.Level;
					ViewStateCount += Old.ViewState.GetReference() ? 1 : 0;
					const bool bRetiringColorAllocated = IsValid(Retiring->LegacyColorTarget);
					const uint64 RetiringColorBytes = EstimateTargetBytes(Retiring->LegacyColorTarget);
					ColorTargetCount += bRetiringColorAllocated ? 1 : 0;
					RetiringColorTargetCount += bRetiringColorAllocated ? 1 : 0;
					const bool bRetiringDepthAllocated = IsValid(Old.SecondaryDepthTarget);
					const uint64 RetiringDepthBytes = EstimateTargetBytes(Old.SecondaryDepthTarget);
					DepthTargetCount += bRetiringDepthAllocated ? 1 : 0;
					RetiringDepthTargetCount += bRetiringDepthAllocated ? 1 : 0;
					EndpointExplicitTargetBytes += RetiringColorBytes + RetiringDepthBytes;
					if (!RetirementsJson.IsEmpty()) { RetirementsJson += TEXT(","); }
					RetirementsJson += FString::Printf(
						TEXT("{\"level\":%d,\"lifetimeId\":%llu,\"state\":\"RETIRING\",\"fenceComplete\":%s,\"legacyColorTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\"secondaryDepthTarget\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu}}"),
						Old.Level, Old.Lifetime.LifetimeId,
						Retiring->Fence.IsFenceComplete() ? TEXT("true") : TEXT("false"),
						bRetiringColorAllocated ? TEXT("true") : TEXT("false"),
						bRetiringColorAllocated ? Retiring->LegacyColorTarget->SizeX : 0,
						bRetiringColorAllocated ? Retiring->LegacyColorTarget->SizeY : 0,
						bRetiringColorAllocated
							? static_cast<int32>(Retiring->LegacyColorTarget->RenderTargetFormat.GetValue()) : -1,
						RetiringColorBytes,
						bRetiringDepthAllocated ? TEXT("true") : TEXT("false"),
						bRetiringDepthAllocated ? Old.SecondaryDepthTarget->SizeX : 0,
						bRetiringDepthAllocated ? Old.SecondaryDepthTarget->SizeY : 0,
						bRetiringDepthAllocated
							? static_cast<int32>(Old.SecondaryDepthTarget->RenderTargetFormat.GetValue()) : -1,
						RetiringDepthBytes);
				}
				TotalViewStates += ViewStateCount;
				TotalColorTargets += ColorTargetCount;
				TotalDepthTargets += DepthTargetCount;
				TotalExplicitTargetBytes += EndpointExplicitTargetBytes;

				EndpointJson += FString::Printf(
					TEXT("    {\"endpoint\":%d,\"visibleDepth\":%d,\"effectiveDepth\":%d,\"attemptedLayerMask\":%d,\"submittedLayerMask\":%d,\"submissionCount\":%d,\"publishedLayerMask\":%d,\"ownedLayerMask\":%d,\"ownedCount\":%d,\"activeLayerMask\":%d,\"activeCount\":%d,\"retiringLayerMask\":%d,\"retiringCount\":%d,\"reclaimableLayerMask\":%d,\"reclaimableCount\":%d,\"viewStateCount\":%d,\"colorTargetCount\":%d,\"activeColorTargetCount\":%d,\"retiringColorTargetCount\":%d,\"depthTargetCount\":%d,\"activeDepthTargetCount\":%d,\"retiringDepthTargetCount\":%d,\"explicitTargetEstimatedBytes\":%llu,\"pingPong\":{\"enabled\":%s,\"targetWidth\":%d,\"targetHeight\":%d},\"retiringLifetimes\":[%s],\"layers\":[\n%s    ]}%s\n"),
					EndpointIndex, Endpoint.LastVisibleDepth, Endpoint.LastEffectiveDepth,
					Endpoint.LastAttemptedLayerMask, Endpoint.LastSubmittedLayerMask,
					CountSetBits(Endpoint.LastSubmittedLayerMask), PublishedLayerMask,
					OwnedLayerMask, CountSetBits(OwnedLayerMask),
					ActiveLayerMask, CountSetBits(ActiveLayerMask),
					RetiringLayerMask, CountSetBits(RetiringLayerMask),
					ReclaimableLayerMask, CountSetBits(ReclaimableLayerMask),
					ViewStateCount, ColorTargetCount, ActiveColorTargetCount, RetiringColorTargetCount,
					DepthTargetCount, ActiveDepthTargetCount, RetiringDepthTargetCount,
					EndpointExplicitTargetBytes,
					bPingPongEnabled ? TEXT("true") : TEXT("false"),
					Endpoint.PingPongTargetSize.X, Endpoint.PingPongTargetSize.Y,
					*RetirementsJson, *LayersJson,
					EndpointIndex + 1 < EndpointCount ? TEXT(",") : TEXT(""));
			}

			const bool bScratchAllocated = IsValid(FinalScratch);
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"schema\":\"PortalFullFidelityPingPongViewport.Prototype.v5\",\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"diagnosticScope\":\"Two-buffer-per-endpoint FullFidelity recursion outputs with projected secondary ViewRect/cropped projection; per-level ViewState/TSR/Lumen remain unchanged\",\n")
				TEXT("  \"pingPongEnabled\":%s,\n")
				TEXT("  \"requestedDepth\":%d,\n")
				TEXT("  \"primaryResolutionFraction\":%.6f,\n")
				TEXT("  \"visibleEndpointCount\":%d,\n")
				TEXT("  \"visibleEndpointMask\":%d,\n")
				TEXT("  \"submittedEndpointMask\":%d,\n")
				TEXT("  \"publishedEndpointMask\":%d,\n")
				TEXT("  \"totals\":{\"viewStates\":%d,\"colorTargets\":%d,\"depthTargets\":%d,\"explicitTargetEstimatedBytes\":%llu},\n")
				TEXT("  \"sharedScratch\":{\"allocated\":%s,\"width\":%d,\"height\":%d,\"renderTargetFormat\":%d,\"estimatedBytes\":%llu},\n")
				TEXT("  \"endpoints\":[\n%s  ],\n")
				TEXT("  \"claimBoundary\":\"Legacy per-level color and secondary depth targets above RequestedDepth retire with the exact recursion lifetime and RHI-thread fence that last used them; depth creation remains lazy on submission, and short invisibility/lower EffectiveDepth do not shrink configured capacity. Ping-pong endpoint targets remain shared. Shared-scratch audit remains a separate gate, and logical release does not prove immediate global GPU allocator residency return.\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				bPingPongEnabled ? TEXT("true") : TEXT("false"),
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
		bool bPingPongEnabled = false;
		bool bRestoreBoundedComposition = false;
		int32 PreviousBoundedCompositionValue = 0;
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
		TEXT("Stop full-fidelity producer and release endpoint x recursion-level histories/color/depth targets."),
		FConsoleCommandDelegate::CreateStatic(&StopMultiVisible));

	FAutoConsoleCommand GDumpMultiVisibleCommand(
		TEXT("portal.DumpMultiVisibleTSRSpike"),
		TEXT("Dump endpoint/recursion-aware full-fidelity telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpMultiVisible));
}
