#include "InteriorPortalRenderSample.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalProductionParityValidationPrivate
{
	TAutoConsoleVariable<int32> CVarProductionParityDiagnostics(
		TEXT("portal.ProductionParityDiagnostics"),
		1,
		TEXT("STEP 1B.14D-C production-parity telemetry. 0=quiet, 1=periodic main/secondary Tonemap exposure diagnostics."),
		ECVF_Default);

	TAtomic<uint64> GMainTonemapFrames { 0 };
	TAtomic<uint64> GSecondaryTonemapFrames { 0 };
	TAtomic<uint64> GInvalidMainExposureFrames { 0 };
	TAtomic<uint64> GInvalidSecondaryExposureFrames { 0 };
	TAtomic<uint64> GSecondaryEyeAdaptationOffFrames { 0 };
	TAtomic<uint64> GSecondaryCameraCutFrames { 0 };
	TAtomic<uint64> GMainCameraCutFrames { 0 };
	TAtomic<uint64> GLastMainFrame { 0 };
	TAtomic<uint64> GLastSecondaryFrame { 0 };
	TAtomic<float> GLastMainPreExposure { 0.0f };
	TAtomic<float> GLastSecondaryPreExposure { 0.0f };
	TAtomic<int32> GLastMainAA { -1 };
	TAtomic<int32> GLastSecondaryAA { -1 };
	TAtomic<int32> GLastMainEyeAdaptation { -1 };
	TAtomic<int32> GLastSecondaryEyeAdaptation { -1 };

	bool IsValidPreExposure(const float Value)
	{
		return FMath::IsFinite(Value) && Value > UE_SMALL_NUMBER;
	}

	bool IsPortalSecondaryView(const FSceneViewFamily& Family, const FSceneView& View)
	{
		// The accepted producer is a primary non-capture additional view family.
		// Run this validation in the controlled portal PIE setup so unrelated
		// additional families do not become ambiguous telemetry sources.
		return Family.bAdditionalViewFamily
			&& !Family.bIsMainViewFamily
			&& !View.bIsSceneCapture
			&& View.IsPrimarySceneView();
	}

	void ResetTelemetry()
	{
		GMainTonemapFrames.Store(0);
		GSecondaryTonemapFrames.Store(0);
		GInvalidMainExposureFrames.Store(0);
		GInvalidSecondaryExposureFrames.Store(0);
		GSecondaryEyeAdaptationOffFrames.Store(0);
		GSecondaryCameraCutFrames.Store(0);
		GMainCameraCutFrames.Store(0);
		GLastMainFrame.Store(0);
		GLastSecondaryFrame.Store(0);
		GLastMainPreExposure.Store(0.0f);
		GLastSecondaryPreExposure.Store(0.0f);
		GLastMainAA.Store(-1);
		GLastSecondaryAA.Store(-1);
		GLastMainEyeAdaptation.Store(-1);
		GLastSecondaryEyeAdaptation.Store(-1);
	}

	class FProductionParityExtension final : public FWorldSceneViewExtension
	{
	public:
		FProductionParityExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
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
			if (Pass != ISceneViewExtension::EPostProcessingPass::Tonemap || !InView.Family)
			{
				return;
			}

			const bool bMain = InteriorPortalRendering::IsPlayerMainView(*InView.Family, InView);
			const bool bSecondary = IsPortalSecondaryView(*InView.Family, InView);
			if (!bMain && !bSecondary)
			{
				return;
			}

			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[bMain, bSecondary](FRDGBuilder& GraphBuilder, const FSceneView& View,
					const FPostProcessMaterialInputs& Inputs)
				{
					const FScreenPassTextureSlice SceneColorSlice =
						Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
					FScreenPassTexture SceneColor =
						FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneColorSlice);
					if (!SceneColor.IsValid() || !View.Family)
					{
						return SceneColor;
					}

					const float PreExposure = View.State
						? View.State->GetPreExposure()
						: 0.0f;
					const bool bValidExposure = IsValidPreExposure(PreExposure);
					const int32 AAMethod = static_cast<int32>(View.AntiAliasingMethod);
					const int32 bEyeAdaptation = View.Family->EngineShowFlags.EyeAdaptation ? 1 : 0;

					if (bMain)
					{
						const uint64 Count = GMainTonemapFrames.Load() + 1;
						GMainTonemapFrames.Store(Count);
						GLastMainFrame.Store(GFrameCounter);
						GLastMainPreExposure.Store(PreExposure);
						GLastMainAA.Store(AAMethod);
						GLastMainEyeAdaptation.Store(bEyeAdaptation);
						if (!bValidExposure)
						{
							GInvalidMainExposureFrames.Store(GInvalidMainExposureFrames.Load() + 1);
						}
						if (View.bCameraCut)
						{
							GMainCameraCutFrames.Store(GMainCameraCutFrames.Load() + 1);
						}
					}

					if (bSecondary)
					{
						const uint64 Count = GSecondaryTonemapFrames.Load() + 1;
						GSecondaryTonemapFrames.Store(Count);
						GLastSecondaryFrame.Store(GFrameCounter);
						GLastSecondaryPreExposure.Store(PreExposure);
						GLastSecondaryAA.Store(AAMethod);
						GLastSecondaryEyeAdaptation.Store(bEyeAdaptation);
						if (!bValidExposure)
						{
							GInvalidSecondaryExposureFrames.Store(GInvalidSecondaryExposureFrames.Load() + 1);
						}
						if (!bEyeAdaptation)
						{
							GSecondaryEyeAdaptationOffFrames.Store(GSecondaryEyeAdaptationOffFrames.Load() + 1);
						}
						if (View.bCameraCut)
						{
							GSecondaryCameraCutFrames.Store(GSecondaryCameraCutFrames.Load() + 1);
						}

						if (CVarProductionParityDiagnostics.GetValueOnRenderThread() != 0
							&& (Count == 1 || (Count % 120) == 0))
						{
							const float MainPreExposure = GLastMainPreExposure.Load();
							const float Scale = IsValidPreExposure(MainPreExposure) && bValidExposure
								? MainPreExposure / PreExposure
								: 0.0f;
							UE_LOG(LogTemp, Display,
								TEXT("PortalProductionParity Secondary Frame=%llu Count=%llu PreExposure=%.9g MainPreExposure=%.9g MainOverSecondary=%.9g EyeAdaptation=%d AA=%d CameraCut=%d Rect=%dx%d"),
								GFrameCounter, Count, PreExposure, MainPreExposure, Scale,
								bEyeAdaptation, AAMethod, View.bCameraCut ? 1 : 0,
								SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height());
						}
					}

					return SceneColor;
				}));
		}
	};

	TSharedPtr<FProductionParityExtension, ESPMode::ThreadSafe> GExtension;
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

	FString ClassifyTelemetry()
	{
		const uint64 MainFrames = GMainTonemapFrames.Load();
		const uint64 SecondaryFrames = GSecondaryTonemapFrames.Load();
		if (MainFrames < 30 || SecondaryFrames < 30)
		{
			return TEXT("INSUFFICIENT_DATA");
		}
		if (GInvalidMainExposureFrames.Load() > 0 || GInvalidSecondaryExposureFrames.Load() > 0)
		{
			return TEXT("INVALID_PREEXPOSURE_OBSERVED");
		}
		if (GSecondaryEyeAdaptationOffFrames.Load() > 0
			|| GLastSecondaryEyeAdaptation.Load() != 1)
		{
			return TEXT("SECONDARY_EYE_ADAPTATION_DISABLED");
		}
		if (GLastSecondaryAA.Load() != static_cast<int32>(EAntiAliasingMethod::AAM_TSR))
		{
			return TEXT("SECONDARY_NOT_TSR");
		}
		return TEXT("TELEMETRY_READY_MANUAL_VISUAL_GATE_REQUIRED");
	}

	void WriteReport(const TCHAR* LifecycleStatus)
	{
		const float MainPreExposure = GLastMainPreExposure.Load();
		const float SecondaryPreExposure = GLastSecondaryPreExposure.Load();
		const float ExposureScale = IsValidPreExposure(MainPreExposure)
			&& IsValidPreExposure(SecondaryPreExposure)
			? MainPreExposure / SecondaryPreExposure
			: 0.0f;
		const FString Classification = ClassifyTelemetry();

		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"classification\":\"%s\",\n")
			TEXT("  \"mainTonemapFrames\":%llu,\n")
			TEXT("  \"secondaryTonemapFrames\":%llu,\n")
			TEXT("  \"lastMainFrame\":%llu,\n")
			TEXT("  \"lastSecondaryFrame\":%llu,\n")
			TEXT("  \"mainPreExposure\":%.9g,\n")
			TEXT("  \"secondaryPreExposure\":%.9g,\n")
			TEXT("  \"mainOverSecondaryExposureScale\":%.9g,\n")
			TEXT("  \"mainEyeAdaptation\":%d,\n")
			TEXT("  \"secondaryEyeAdaptation\":%d,\n")
			TEXT("  \"mainAAMethod\":%d,\n")
			TEXT("  \"secondaryAAMethod\":%d,\n")
			TEXT("  \"invalidMainExposureFrames\":%llu,\n")
			TEXT("  \"invalidSecondaryExposureFrames\":%llu,\n")
			TEXT("  \"secondaryEyeAdaptationOffFrames\":%llu,\n")
			TEXT("  \"mainCameraCutFrames\":%llu,\n")
			TEXT("  \"secondaryCameraCutFrames\":%llu,\n")
			TEXT("  \"claimBoundary\":\"Telemetry validates runtime view/exposure ownership only. Final portal-vs-direct visual parity remains a manual PIE gate.\"\n")
			TEXT("}\n"),
			LifecycleStatus,
			*Classification,
			GMainTonemapFrames.Load(),
			GSecondaryTonemapFrames.Load(),
			GLastMainFrame.Load(),
			GLastSecondaryFrame.Load(),
			MainPreExposure,
			SecondaryPreExposure,
			ExposureScale,
			GLastMainEyeAdaptation.Load(),
			GLastSecondaryEyeAdaptation.Load(),
			GLastMainAA.Load(),
			GLastSecondaryAA.Load(),
			GInvalidMainExposureFrames.Load(),
			GInvalidSecondaryExposureFrames.Load(),
			GSecondaryEyeAdaptationOffFrames.Load(),
			GMainCameraCutFrames.Load(),
			GSecondaryCameraCutFrames.Load());

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
			TEXT("PortalProductionParityValidation.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		FFileHelper::SaveStringToFile(Json, *ReportPath);
	}

	void StartValidation()
	{
		if (GRunning)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalProductionParity: already running."));
			return;
		}

		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalProductionParity: PIE/Game world unavailable."));
			return;
		}

		ResetTelemetry();
		GActiveWorld = World;
		GExtension = FSceneViewExtensions::NewExtension<FProductionParityExtension>(World);
		GRunning = true;
		WriteReport(TEXT("RUNNING"));
		UE_LOG(LogTemp, Display,
			TEXT("PortalProductionParity: START. Keep portal.StartFullViewFamilyTSRSpike running, then exercise static/slant/retreat/leave-return/crossing views."));
	}

	void DumpValidation()
	{
		WriteReport(GRunning ? TEXT("RUNNING") : TEXT("STOPPED"));
		UE_LOG(LogTemp, Display,
			TEXT("PortalProductionParity: MainFrames=%llu SecondaryFrames=%llu MainPre=%.9g SecondaryPre=%.9g Eye(Main/Secondary)=%d/%d AA(Main/Secondary)=%d/%d Invalid(Main/Secondary)=%llu/%llu SecondaryEyeOff=%llu SecondaryCuts=%llu Classification=%s"),
			GMainTonemapFrames.Load(), GSecondaryTonemapFrames.Load(),
			GLastMainPreExposure.Load(), GLastSecondaryPreExposure.Load(),
			GLastMainEyeAdaptation.Load(), GLastSecondaryEyeAdaptation.Load(),
			GLastMainAA.Load(), GLastSecondaryAA.Load(),
			GInvalidMainExposureFrames.Load(), GInvalidSecondaryExposureFrames.Load(),
			GSecondaryEyeAdaptationOffFrames.Load(), GSecondaryCameraCutFrames.Load(),
			*ClassifyTelemetry());
	}

	void StopValidation()
	{
		if (!GRunning)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalProductionParity: not running."));
			return;
		}

		FlushRenderingCommands();
		GExtension.Reset();
		GActiveWorld.Reset();
		GRunning = false;
		WriteReport(TEXT("STOPPED"));
		UE_LOG(LogTemp, Display,
			TEXT("PortalProductionParity: STOP. Classification=%s Report=Saved/AutomationReports/PortalProductionParityValidation.json"),
			*ClassifyTelemetry());
	}

	FAutoConsoleCommand GStartProductionParityValidationCommand(
		TEXT("portal.StartProductionParityValidation"),
		TEXT("Start STEP 1B.14D-C production TSR exposure/view telemetry. Start the full-view TSR producer first."),
		FConsoleCommandDelegate::CreateStatic(&StartValidation));

	FAutoConsoleCommand GDumpProductionParityValidationCommand(
		TEXT("portal.DumpProductionParityValidation"),
		TEXT("Dump STEP 1B.14D-C production parity telemetry to Saved/AutomationReports."),
		FConsoleCommandDelegate::CreateStatic(&DumpValidation));

	FAutoConsoleCommand GStopProductionParityValidationCommand(
		TEXT("portal.StopProductionParityValidation"),
		TEXT("Stop STEP 1B.14D-C production parity telemetry and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopValidation));
}
