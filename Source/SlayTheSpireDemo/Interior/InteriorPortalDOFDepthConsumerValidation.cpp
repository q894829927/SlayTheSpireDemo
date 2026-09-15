#include "Engine/Engine.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalDOFDepthConsumerValidationPrivate
{
	TAutoConsoleVariable<int32> CVarDOFValidationEnabled(
		TEXT("portal.DOFDepthConsumerValidation"),
		0,
		TEXT("STEP 1B.12C-B. 1=force a strong main-view cinematic DOF setup so downstream consumption of propagated portal depth can be A/B validated. The additional secondary view family is never modified."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDOFFocalDistanceCm(
		TEXT("portal.DOFValidationFocalDistanceCm"),
		150.0f,
		TEXT("STEP 1B.12C-B forced main-view DOF focal distance in cm. Adjust so the physical entry portal plane is approximately in focus for the propagation-off baseline."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDOFFstop(
		TEXT("portal.DOFValidationFstop"),
		1.2f,
		TEXT("STEP 1B.12C-B forced main-view DOF aperture (f-stop). Lower values strengthen the validation effect. Clamped to [1.0, 32.0]."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDOFSensorWidthMm(
		TEXT("portal.DOFValidationSensorWidthMm"),
		36.0f,
		TEXT("STEP 1B.12C-B forced main-view DOF sensor width in millimeters. Clamped to [0.1, 1000]."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDOFDiagnostics(
		TEXT("portal.DOFValidationDiagnostics"),
		1,
		TEXT("STEP 1B.12C-B diagnostics. 0=quiet, 1=log main SetupView override and AfterDOF callback evidence."),
		ECVF_Default);

	TAtomic<uint64> GSetupViewFrames { 0 };
	TAtomic<uint64> GAfterDOFFrames { 0 };
	TAtomic<uint64> GLastAfterDOFFrame { 0 };
	TAtomic<bool> GLastAfterDOFPassEnabled { false };
	TAtomic<float> GLastFocalDistanceCm { 0.0f };
	TAtomic<float> GLastFstop { 0.0f };
	TAtomic<float> GLastSensorWidthMm { 0.0f };

	class FPortalDOFValidationExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalDOFValidationExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
		{
		}

		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
		{
			if (InViewFamily.bAdditionalViewFamily
				|| CVarDOFValidationEnabled.GetValueOnGameThread() == 0)
			{
				return;
			}

			const float FocalDistanceCm = FMath::Max(
				CVarDOFFocalDistanceCm.GetValueOnGameThread(), 1.0f);
			const float Fstop = FMath::Clamp(
				CVarDOFFstop.GetValueOnGameThread(), 1.0f, 32.0f);
			const float SensorWidthMm = FMath::Clamp(
				CVarDOFSensorWidthMm.GetValueOnGameThread(), 0.1f, 1000.0f);

			// Use UE's public post-process override layer instead of mutating renderer-
			// private DOF state. The secondary full-view renderer deliberately keeps
			// DOF disabled; this extension affects only the real player main view.
			FPostProcessSettings ValidationSettings;
			ValidationSettings.bOverride_DepthOfFieldFocalDistance = true;
			ValidationSettings.DepthOfFieldFocalDistance = FocalDistanceCm;
			ValidationSettings.bOverride_DepthOfFieldFstop = true;
			ValidationSettings.DepthOfFieldFstop = Fstop;
			ValidationSettings.bOverride_DepthOfFieldSensorWidth = true;
			ValidationSettings.DepthOfFieldSensorWidth = SensorWidthMm;
			InView.OverridePostProcessSettings(ValidationSettings, 1.0f, false);
			InViewFamily.EngineShowFlags.SetDepthOfField(true);

			GLastFocalDistanceCm.Store(InView.FinalPostProcessSettings.DepthOfFieldFocalDistance);
			GLastFstop.Store(InView.FinalPostProcessSettings.DepthOfFieldFstop);
			GLastSensorWidthMm.Store(InView.FinalPostProcessSettings.DepthOfFieldSensorWidth);
			const uint64 SetupCount = GSetupViewFrames.FetchAdd(1) + 1;
			if (CVarDOFDiagnostics.GetValueOnGameThread() != 0
				&& (SetupCount == 1 || (SetupCount % 60) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalDOFValidation SetupView Count=%llu FocalDistanceCm=%.3f Fstop=%.3f SensorWidthMm=%.3f AdditionalFamily=0"),
					SetupCount,
					GLastFocalDistanceCm.Load(),
					GLastFstop.Load(),
					GLastSensorWidthMm.Load());
			}
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
			if (Pass != ISceneViewExtension::EPostProcessingPass::AfterDOF
				|| !InView.Family
				|| InView.Family->bAdditionalViewFamily
				|| CVarDOFValidationEnabled.GetValueOnAnyThread() == 0)
			{
				return;
			}

			GLastAfterDOFPassEnabled.Store(bIsPassEnabled);
			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[bIsPassEnabled](FRDGBuilder& GraphBuilder, const FSceneView& View,
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

					const uint64 AfterCount = GAfterDOFFrames.FetchAdd(1) + 1;
					GLastAfterDOFFrame.Store(GFrameCounter);
					GLastAfterDOFPassEnabled.Store(bIsPassEnabled);
					if (CVarDOFDiagnostics.GetValueOnRenderThread() != 0
						&& (AfterCount == 1 || (AfterCount % 60) == 0))
					{
						UE_LOG(LogTemp, Display,
							TEXT("PortalDOFValidation AfterDOF Frame=%llu Count=%llu PassEnabled=%d SceneRect=%dx%d FocalDistanceCm=%.3f Fstop=%.3f SensorWidthMm=%.3f"),
							GFrameCounter,
							AfterCount,
							bIsPassEnabled ? 1 : 0,
							SceneColor.ViewRect.Width(),
							SceneColor.ViewRect.Height(),
							View.FinalPostProcessSettings.DepthOfFieldFocalDistance,
							View.FinalPostProcessSettings.DepthOfFieldFstop,
							View.FinalPostProcessSettings.DepthOfFieldSensorWidth);
					}
					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return CVarDOFValidationEnabled.GetValueOnAnyThread() != 0
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}
	};

	TSharedPtr<FPortalDOFValidationExtension, ESPMode::ThreadSafe> GDOFValidationExtension;

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

	void ResetCounters()
	{
		GSetupViewFrames.Store(0);
		GAfterDOFFrames.Store(0);
		GLastAfterDOFFrame.Store(0);
		GLastAfterDOFPassEnabled.Store(false);
		GLastFocalDistanceCm.Store(0.0f);
		GLastFstop.Store(0.0f);
		GLastSensorWidthMm.Store(0.0f);
	}

	void WriteReport(const TCHAR* Status)
	{
		const FString Json = FString::Printf(
			TEXT("{\n  \"status\": \"%s\",\n  \"setupViewFrames\": %llu,\n  \"afterDOFFrames\": %llu,\n  \"lastAfterDOFFrame\": %llu,\n  \"lastAfterDOFPassEnabled\": %s,\n  \"focalDistanceCm\": %.6f,\n  \"fstop\": %.6f,\n  \"sensorWidthMm\": %.6f,\n  \"claimBoundary\": \"STEP 1B.12C-B proves a real main-view AfterDOF downstream consumer is active under controlled DOF settings and can be A/B compared with main depth propagation. It does not prove stencil/HZB/SSR/Lumen integration or production acceptance.\"\n}\n"),
			Status,
			GSetupViewFrames.Load(),
			GAfterDOFFrames.Load(),
			GLastAfterDOFFrame.Load(),
			GLastAfterDOFPassEnabled.Load() ? TEXT("true") : TEXT("false"),
			GLastFocalDistanceCm.Load(),
			GLastFstop.Load(),
			GLastSensorWidthMm.Load());

		const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("AutomationReports");
		IFileManager::Get().MakeDirectory(*ReportDir, true);
		const FString ReportPath = ReportDir / TEXT("PortalDOFDepthConsumerValidation.json");
		FFileHelper::SaveStringToFile(Json, *ReportPath);
		UE_LOG(LogTemp, Display,
			TEXT("PortalDOFValidation: report written to %s SetupView=%llu AfterDOF=%llu LastAfterDOFFrame=%llu PassEnabled=%d"),
			*ReportPath,
			GSetupViewFrames.Load(),
			GAfterDOFFrames.Load(),
			GLastAfterDOFFrame.Load(),
			GLastAfterDOFPassEnabled.Load() ? 1 : 0);
	}

	void StartDOFValidation()
	{
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalDOFValidation: no PIE/Game world found. Enter PIE first."));
			return;
		}

		if (GDOFValidationExtension.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalDOFValidation: already running."));
			return;
		}

		ResetCounters();
		CVarDOFValidationEnabled->Set(1, ECVF_SetByCode);
		GDOFValidationExtension =
			FSceneViewExtensions::NewExtension<FPortalDOFValidationExtension>(World);
		UE_LOG(LogTemp, Display,
			TEXT("PortalDOFValidation: started. FocalDistanceCm=%.3f Fstop=%.3f SensorWidthMm=%.3f. A/B portal.MainDepthPropagation 0 vs 1 while keeping camera fixed."),
			CVarDOFFocalDistanceCm.GetValueOnGameThread(),
			CVarDOFFstop.GetValueOnGameThread(),
			CVarDOFSensorWidthMm.GetValueOnGameThread());
	}

	void DumpDOFValidation()
	{
		WriteReport(GDOFValidationExtension.IsValid() ? TEXT("RUNNING") : TEXT("STOPPED"));
	}

	void StopDOFValidation()
	{
		if (!GDOFValidationExtension.IsValid())
		{
			CVarDOFValidationEnabled->Set(0, ECVF_SetByCode);
			UE_LOG(LogTemp, Display, TEXT("PortalDOFValidation: not running."));
			return;
		}

		WriteReport(TEXT("STOPPED"));
		CVarDOFValidationEnabled->Set(0, ECVF_SetByCode);
		FlushRenderingCommands();
		GDOFValidationExtension.Reset();
		UE_LOG(LogTemp, Display,
			TEXT("PortalDOFValidation: stopped. SetupView=%llu AfterDOF=%llu LastAfterDOFFrame=%llu PassEnabled=%d."),
			GSetupViewFrames.Load(),
			GAfterDOFFrames.Load(),
			GLastAfterDOFFrame.Load(),
			GLastAfterDOFPassEnabled.Load() ? 1 : 0);
	}

	FAutoConsoleCommand GStartDOFValidationCommand(
		TEXT("portal.StartDOFDepthConsumerValidation"),
		TEXT("STEP 1B.12C-B: force controlled main-view cinematic DOF and observe the real AfterDOF downstream callback. Use portal.MainDepthPropagation 0/1 for the fixed-camera A/B."),
		FConsoleCommandDelegate::CreateStatic(&StartDOFValidation));

	FAutoConsoleCommand GDumpDOFValidationCommand(
		TEXT("portal.DumpDOFDepthConsumerValidation"),
		TEXT("Write STEP 1B.12C-B DOF downstream validation telemetry to Saved/AutomationReports/PortalDOFDepthConsumerValidation.json."),
		FConsoleCommandDelegate::CreateStatic(&DumpDOFValidation));

	FAutoConsoleCommand GStopDOFValidationCommand(
		TEXT("portal.StopDOFDepthConsumerValidation"),
		TEXT("Stop STEP 1B.12C-B controlled main-view DOF validation and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopDOFValidation));
}
