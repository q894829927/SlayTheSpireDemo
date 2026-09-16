#include "Engine/Engine.h"
#include "InteriorPortalRenderSample.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalLateLatchedPreExposureBridgePrivate
{
	TAutoConsoleVariable<int32> CVarLateLatchedPreExposureBridge(
		TEXT("portal.LateLatchedPreExposureBridge"),
		0,
		TEXT("STEP 1B.14D-B diagnostic bridge. 0=off, 1=late-latch real main Tonemap PreExposure and publish it as portal.SecondaryPreExposure on the following game-thread tick."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarLateLatchedPreExposureDiagnostics(
		TEXT("portal.LateLatchedPreExposureDiagnostics"),
		1,
		TEXT("STEP 1B.14D-B diagnostics. 0=quiet, 1=periodic Tonemap latch and bridge-write telemetry."),
		ECVF_Default);

	TAtomic<float> GLatchedMainPreExposure { 1.0f };
	TAtomic<uint64> GLatchedMainFrame { 0 };
	TAtomic<uint64> GMainTonemapFrames { 0 };
	TAtomic<uint64> GBridgeWrites { 0 };
	TAtomic<float> GLastPublishedPreExposure { 1.0f };
	TAtomic<float> GLastObservedSecondaryPreExposureBeforeWrite { 1.0f };

	class FMainTonemapPreExposureLatchExtension final : public FWorldSceneViewExtension
	{
	public:
		FMainTonemapPreExposureLatchExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
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
				|| !InView.Family
				|| !InteriorPortalRendering::IsPlayerMainView(*InView.Family, InView)
				|| CVarLateLatchedPreExposureBridge.GetValueOnAnyThread() == 0)
			{
				return;
			}

			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[](FRDGBuilder& GraphBuilder, const FSceneView& View,
					const FPostProcessMaterialInputs& Inputs)
				{
					const FScreenPassTextureSlice SceneColorSlice =
						Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
					FScreenPassTexture SceneColor =
						FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneColorSlice);
					if (!SceneColor.IsValid())
					{
						return SceneColor;
					}

					const float PreExposure = View.State
						? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
						: 1.0f;
					const uint64 Count = GMainTonemapFrames.Load() + 1;
					GMainTonemapFrames.Store(Count);
					GLatchedMainPreExposure.Store(PreExposure);
					GLatchedMainFrame.Store(GFrameCounter);

					if (CVarLateLatchedPreExposureDiagnostics.GetValueOnAnyThread() != 0
						&& (Count == 1 || (Count % 60) == 0))
					{
						UE_LOG(LogTemp, Display,
							TEXT("PortalLateLatch MainTonemap Frame=%llu Count=%llu PreExposure=%.9g AA=%d SceneRect=%dx%d"),
							GFrameCounter, Count, PreExposure,
							static_cast<int32>(View.AntiAliasingMethod),
							SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height());
					}
					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return CVarLateLatchedPreExposureBridge.GetValueOnAnyThread() != 0
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}
	};

	TSharedPtr<FMainTonemapPreExposureLatchExtension, ESPMode::ThreadSafe> GLatchExtension;
	FDelegateHandle GWorldPostActorTickHandle;
	TWeakObjectPtr<UWorld> GActiveWorld;
	bool GRunning = false;

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

	void SetBridgeEnabled(const bool bEnabled)
	{
		if (IConsoleVariable* Var =
			IConsoleManager::Get().FindConsoleVariable(TEXT("portal.LateLatchedPreExposureBridge")))
		{
			Var->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
		}
	}

	void OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		(void)TickType;
		(void)DeltaSeconds;
		if (!GRunning || !GActiveWorld.IsValid() || World != GActiveWorld.Get())
		{
			return;
		}

		const uint64 LatchedFrame = GLatchedMainFrame.Load();
		const float LatchedPreExposure = GLatchedMainPreExposure.Load();
		if (LatchedFrame == 0 || !FMath::IsFinite(LatchedPreExposure) || LatchedPreExposure <= UE_SMALL_NUMBER)
		{
			return;
		}

		IConsoleVariable* SecondaryPreExposureVar =
			IConsoleManager::Get().FindConsoleVariable(TEXT("portal.SecondaryPreExposure"));
		IConsoleVariable* RebaseVar =
			IConsoleManager::Get().FindConsoleVariable(TEXT("portal.PreExposureRebase"));
		if (!SecondaryPreExposureVar || !RebaseVar)
		{
			return;
		}

		const float BeforeWrite = SecondaryPreExposureVar->GetFloat();
		GLastObservedSecondaryPreExposureBeforeWrite.Store(BeforeWrite);
		SecondaryPreExposureVar->Set(LatchedPreExposure, ECVF_SetByCode);
		RebaseVar->Set(1, ECVF_SetByCode);
		GLastPublishedPreExposure.Store(LatchedPreExposure);
		const uint64 WriteCount = GBridgeWrites.Load() + 1;
		GBridgeWrites.Store(WriteCount);

		if (CVarLateLatchedPreExposureDiagnostics.GetValueOnGameThread() != 0
			&& (WriteCount == 1 || (WriteCount % 60) == 0))
		{
			const uint64 CurrentFrame = GFrameCounter;
			const uint64 FrameLag = CurrentFrame >= LatchedFrame ? CurrentFrame - LatchedFrame : 0;
			UE_LOG(LogTemp, Display,
				TEXT("PortalLateLatch Publish Frame=%llu Count=%llu LatchedMainFrame=%llu FrameLag=%llu Before=%.9g Published=%.9g Rebase=1"),
				CurrentFrame, WriteCount, LatchedFrame, FrameLag, BeforeWrite, LatchedPreExposure);
		}
	}

	void StartBridge()
	{
		if (GRunning)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalLateLatch: already running."));
			return;
		}
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalLateLatch: no PIE/Game world."));
			return;
		}

		GLatchedMainPreExposure.Store(1.0f);
		GLatchedMainFrame.Store(0);
		GMainTonemapFrames.Store(0);
		GBridgeWrites.Store(0);
		GLastPublishedPreExposure.Store(1.0f);
		GLastObservedSecondaryPreExposureBeforeWrite.Store(1.0f);
		GActiveWorld = World;
		SetBridgeEnabled(true);
		GLatchExtension = FSceneViewExtensions::NewExtension<FMainTonemapPreExposureLatchExtension>(World);
		GWorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddStatic(&OnWorldPostActorTick);
		GRunning = true;

		UE_LOG(LogTemp, Display,
			TEXT("PortalLateLatch: START. Start the producer first, then this bridge. It late-latches the real main Tonemap PreExposure and republishes it as portal.SecondaryPreExposure on the next game-thread tick."));
	}

	void DumpBridge()
	{
		const uint64 CurrentFrame = GFrameCounter;
		const uint64 LatchedFrame = GLatchedMainFrame.Load();
		const uint64 FrameLag = CurrentFrame >= LatchedFrame ? CurrentFrame - LatchedFrame : 0;
		UE_LOG(LogTemp, Display,
			TEXT("PortalLateLatch Report Running=%d MainTonemapFrames=%llu LatchedMainFrame=%llu FrameLag=%llu LatchedMainPreExposure=%.9g BridgeWrites=%llu BeforeLastWrite=%.9g PublishedPreExposure=%.9g"),
			GRunning ? 1 : 0,
			GMainTonemapFrames.Load(), LatchedFrame, FrameLag,
			GLatchedMainPreExposure.Load(), GBridgeWrites.Load(),
			GLastObservedSecondaryPreExposureBeforeWrite.Load(),
			GLastPublishedPreExposure.Load());
	}

	void StopBridge()
	{
		if (GWorldPostActorTickHandle.IsValid())
		{
			FWorldDelegates::OnWorldPostActorTick.Remove(GWorldPostActorTickHandle);
			GWorldPostActorTickHandle.Reset();
		}
		SetBridgeEnabled(false);
		GLatchExtension.Reset();
		GActiveWorld.Reset();
		GRunning = false;
		UE_LOG(LogTemp, Display, TEXT("PortalLateLatch: STOP."));
	}

	FAutoConsoleCommand GStartLateLatchedPreExposureBridgeCommand(
		TEXT("portal.StartLateLatchedPreExposureBridge"),
		TEXT("Start STEP 1B.14D-B real-main-Tonemap late-latched PreExposure bridge. Start the portal producer first."),
		FConsoleCommandDelegate::CreateStatic(&StartBridge));

	FAutoConsoleCommand GDumpLateLatchedPreExposureBridgeCommand(
		TEXT("portal.DumpLateLatchedPreExposureBridge"),
		TEXT("Dump STEP 1B.14D-B late-latched PreExposure bridge telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpBridge));

	FAutoConsoleCommand GStopLateLatchedPreExposureBridgeCommand(
		TEXT("portal.StopLateLatchedPreExposureBridge"),
		TEXT("Stop STEP 1B.14D-B late-latched PreExposure bridge."),
		FConsoleCommandDelegate::CreateStatic(&StopBridge));
}
