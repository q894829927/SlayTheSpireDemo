#include "InteriorPortalRenderSample.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

namespace InteriorPortalProductionParityValidationPrivate
{
	TAutoConsoleVariable<int32> CVarProductionParityDiagnostics(
		TEXT("portal.ProductionParityDiagnostics"),
		1,
		TEXT("STEP 1B.14D-C main-view parity telemetry. 0=quiet, 1=periodic main-view diagnostics. Secondary telemetry stays producer-owned."),
		ECVF_Default);

	TAtomic<uint64> GMainViewFamilyFrames { 0 };
	TAtomic<uint64> GInvalidMainExposureFrames { 0 };
	TAtomic<uint64> GMainCameraCutFrames { 0 };
	TAtomic<uint64> GLastMainFrame { 0 };
	TAtomic<float> GLastMainPreExposure { 0.0f };
	TAtomic<int32> GLastMainAA { -1 };
	TAtomic<int32> GLastMainEyeAdaptation { -1 };

	TSharedPtr<class FProductionParityExtension, ESPMode::ThreadSafe> GExtension;
	TWeakObjectPtr<UWorld> GActiveWorld;
	bool GRunning = false;

	bool IsValidPreExposure(const float Value)
	{
		return FMath::IsFinite(Value) && Value > UE_SMALL_NUMBER;
	}

	void ResetTelemetry()
	{
		GMainViewFamilyFrames.Store(0);
		GInvalidMainExposureFrames.Store(0);
		GMainCameraCutFrames.Store(0);
		GLastMainFrame.Store(0);
		GLastMainPreExposure.Store(0.0f);
		GLastMainAA.Store(-1);
		GLastMainEyeAdaptation.Store(-1);
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

			// Critical isolation rule: manually constructed portal families are
			// additional view families. Do not subscribe to, copy from, or otherwise
			// add work to their post-process graph from this world-scoped observer.
			// The accepted TSR producer already owns the secondary Tonemap extraction
			// and its exact FColorSample metadata.
			if (!InViewFamily.bIsMainViewFamily || InViewFamily.bAdditionalViewFamily)
			{
				return;
			}

			for (const FSceneView* View : InViewFamily.Views)
			{
				if (!View || !InteriorPortalRendering::IsPlayerMainView(InViewFamily, *View))
				{
					continue;
				}

				const float PreExposure = View->State
					? View->State->GetPreExposure()
					: 0.0f;
				const bool bValidExposure = IsValidPreExposure(PreExposure);
				const int32 AAMethod = static_cast<int32>(View->AntiAliasingMethod);
				const int32 bEyeAdaptation = InViewFamily.EngineShowFlags.EyeAdaptation ? 1 : 0;

				const uint64 Count = GMainViewFamilyFrames.Load() + 1;
				GMainViewFamilyFrames.Store(Count);
				GLastMainFrame.Store(GFrameCounter);
				GLastMainPreExposure.Store(PreExposure);
				GLastMainAA.Store(AAMethod);
				GLastMainEyeAdaptation.Store(bEyeAdaptation);
				if (!bValidExposure)
				{
					GInvalidMainExposureFrames.Store(GInvalidMainExposureFrames.Load() + 1);
				}
				if (View->bCameraCut)
				{
					GMainCameraCutFrames.Store(GMainCameraCutFrames.Load() + 1);
				}

				if (CVarProductionParityDiagnostics.GetValueOnRenderThread() != 0
					&& (Count == 1 || (Count % 120) == 0))
				{
					UE_LOG(LogTemp, Display,
						TEXT("PortalProductionParity Main Frame=%llu Count=%llu PreExposure=%.9g EyeAdaptation=%d AA=%d CameraCut=%d"),
						GFrameCounter,
						Count,
						PreExposure,
						bEyeAdaptation,
						AAMethod,
						View->bCameraCut ? 1 : 0);
				}

				// Single-local-player validation: exactly one authoritative player
				// main view is expected. Avoid double-counting stereo/non-authority
				// views if the family ever grows additional entries.
				break;
			}
		}
	};

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
		if (GMainViewFamilyFrames.Load() < 30)
		{
			return TEXT("INSUFFICIENT_MAIN_DATA");
		}
		if (GInvalidMainExposureFrames.Load() > 0)
		{
			return TEXT("INVALID_MAIN_PREEXPOSURE_OBSERVED");
		}
		return TEXT("MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE");
	}

	void WriteReport(const TCHAR* LifecycleStatus)
	{
		const FString Classification = ClassifyTelemetry();
		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"classification\":\"%s\",\n")
			TEXT("  \"mainViewFamilyFrames\":%llu,\n")
			TEXT("  \"lastMainFrame\":%llu,\n")
			TEXT("  \"mainPreExposure\":%.9g,\n")
			TEXT("  \"mainEyeAdaptation\":%d,\n")
			TEXT("  \"mainAAMethod\":%d,\n")
			TEXT("  \"invalidMainExposureFrames\":%llu,\n")
			TEXT("  \"mainCameraCutFrames\":%llu,\n")
			TEXT("  \"secondaryTelemetrySource\":\"PortalFullViewFamilyTSRSpike producer-owned Tonemap extraction / exact FColorSample\",\n")
			TEXT("  \"secondaryObserverPolicy\":\"No world-scoped parity callback is registered into additional portal view families\",\n")
			TEXT("  \"claimBoundary\":\"This report validates player-main-view ownership only. Pair it with PortalFullViewFamilyTSRSpike.json and the manual PIE visual gate for production parity.\"\n")
			TEXT("}\n"),
			LifecycleStatus,
			*Classification,
			GMainViewFamilyFrames.Load(),
			GLastMainFrame.Load(),
			GLastMainPreExposure.Load(),
			GLastMainEyeAdaptation.Load(),
			GLastMainAA.Load(),
			GInvalidMainExposureFrames.Load(),
			GMainCameraCutFrames.Load());

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("AutomationReports"),
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
			TEXT("PortalProductionParity: START main-view-only observer. Secondary telemetry remains inside portal.StartFullViewFamilyTSRSpike; no parity callback is added to the secondary view family."));
	}

	void DumpValidation()
	{
		WriteReport(GRunning ? TEXT("RUNNING") : TEXT("STOPPED"));
		UE_LOG(LogTemp, Display,
			TEXT("PortalProductionParity: MainFrames=%llu MainPre=%.9g Eye=%d AA=%d InvalidMain=%llu MainCuts=%llu Classification=%s SecondarySource=PortalFullViewFamilyTSRSpike"),
			GMainViewFamilyFrames.Load(),
			GLastMainPreExposure.Load(),
			GLastMainEyeAdaptation.Load(),
			GLastMainAA.Load(),
			GInvalidMainExposureFrames.Load(),
			GMainCameraCutFrames.Load(),
			*ClassifyTelemetry());
	}

	void StopValidation()
	{
		if (!GRunning)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalProductionParity: not running."));
			return;
		}

		// Blocking synchronization is teardown-only. There is no per-frame flush.
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
		TEXT("Start STEP 1B.14D-C player-main-view telemetry. Secondary telemetry stays inside the accepted TSR producer."),
		FConsoleCommandDelegate::CreateStatic(&StartValidation));

	FAutoConsoleCommand GDumpProductionParityValidationCommand(
		TEXT("portal.DumpProductionParityValidation"),
		TEXT("Dump STEP 1B.14D-C main-view telemetry without stopping."),
		FConsoleCommandDelegate::CreateStatic(&DumpValidation));

	FAutoConsoleCommand GStopProductionParityValidationCommand(
		TEXT("portal.StopProductionParityValidation"),
		TEXT("Stop STEP 1B.14D-C main-view telemetry and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopValidation));
}
