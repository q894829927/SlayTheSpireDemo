#include "Engine/Engine.h"
#include "InteriorPortalRenderSample.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalViewParityDiagnosticsPrivate
{
	TAutoConsoleVariable<int32> CVarViewParityDiagnostics(
		TEXT("portal.ViewParityDiagnostics"),
		0,
		TEXT("STEP 1B.14B main-vs-secondary visual-parity diagnostics. 0=off, 1=record Tonemap-stage view/exposure/post-process telemetry."),
		ECVF_Default);

	TAtomic<uint64> GMainTonemapFrames { 0 };
	TAtomic<uint64> GSecondaryTonemapFrames { 0 };
	TAtomic<uint64> GLastMainFrame { 0 };
	TAtomic<uint64> GLastSecondaryFrame { 0 };

	TAtomic<float> GMainPreExposure { 1.0f };
	TAtomic<float> GSecondaryObservedPreExposure { 1.0f };
	TAtomic<int32> GMainAAMethod { -1 };
	TAtomic<int32> GSecondaryAAMethod { -1 };
	TAtomic<int32> GMainWidth { 0 };
	TAtomic<int32> GMainHeight { 0 };
	TAtomic<int32> GSecondaryWidth { 0 };
	TAtomic<int32> GSecondaryHeight { 0 };

	TAtomic<float> GMainExposureBias { 0.0f };
	TAtomic<float> GSecondaryExposureBias { 0.0f };
	TAtomic<int32> GMainAutoExposureMethod { -1 };
	TAtomic<int32> GSecondaryAutoExposureMethod { -1 };
	TAtomic<int32> GMainDynamicGIMethod { -1 };
	TAtomic<int32> GSecondaryDynamicGIMethod { -1 };
	TAtomic<int32> GMainReflectionMethod { -1 };
	TAtomic<int32> GSecondaryReflectionMethod { -1 };
	TAtomic<float> GMainIndirectLightingIntensity { 1.0f };
	TAtomic<float> GSecondaryIndirectLightingIntensity { 1.0f };

	float ReadFloatCVar(const TCHAR* Name, const float Fallback)
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return Var->GetFloat();
		}
		return Fallback;
	}

	int32 ReadIntCVar(const TCHAR* Name, const int32 Fallback)
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return Var->GetInt();
		}
		return Fallback;
	}

	void ResetTelemetry()
	{
		GMainTonemapFrames.Store(0);
		GSecondaryTonemapFrames.Store(0);
		GLastMainFrame.Store(0);
		GLastSecondaryFrame.Store(0);
		GMainPreExposure.Store(1.0f);
		GSecondaryObservedPreExposure.Store(1.0f);
		GMainAAMethod.Store(-1);
		GSecondaryAAMethod.Store(-1);
		GMainWidth.Store(0);
		GMainHeight.Store(0);
		GSecondaryWidth.Store(0);
		GSecondaryHeight.Store(0);
		GMainExposureBias.Store(0.0f);
		GSecondaryExposureBias.Store(0.0f);
		GMainAutoExposureMethod.Store(-1);
		GSecondaryAutoExposureMethod.Store(-1);
		GMainDynamicGIMethod.Store(-1);
		GSecondaryDynamicGIMethod.Store(-1);
		GMainReflectionMethod.Store(-1);
		GSecondaryReflectionMethod.Store(-1);
		GMainIndirectLightingIntensity.Store(1.0f);
		GSecondaryIndirectLightingIntensity.Store(1.0f);
	}

	class FPortalViewParityDiagnosticsExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalViewParityDiagnosticsExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
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
				|| CVarViewParityDiagnostics.GetValueOnAnyThread() == 0
				|| !InView.Family)
			{
				return;
			}

			const bool bAdditionalViewFamily = InView.Family->bAdditionalViewFamily;
			if (!bAdditionalViewFamily
				&& !InteriorPortalRendering::IsPlayerMainView(*InView.Family, InView))
			{
				return;
			}
			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
				[bAdditionalViewFamily](FRDGBuilder& GraphBuilder, const FSceneView& View,
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
					const int32 AAMethod = static_cast<int32>(View.AntiAliasingMethod);
					const int32 AutoExposureMethod =
						static_cast<int32>(View.FinalPostProcessSettings.AutoExposureMethod);
					const int32 DynamicGIMethod =
						static_cast<int32>(View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod);
					const int32 ReflectionMethod =
						static_cast<int32>(View.FinalPostProcessSettings.ReflectionMethod);

					if (bAdditionalViewFamily)
					{
						const uint64 Count = GSecondaryTonemapFrames.Load() + 1;
						GSecondaryTonemapFrames.Store(Count);
						GLastSecondaryFrame.Store(GFrameCounter);
						GSecondaryObservedPreExposure.Store(PreExposure);
						GSecondaryAAMethod.Store(AAMethod);
						GSecondaryWidth.Store(SceneColor.ViewRect.Width());
						GSecondaryHeight.Store(SceneColor.ViewRect.Height());
						GSecondaryExposureBias.Store(View.FinalPostProcessSettings.AutoExposureBias);
						GSecondaryAutoExposureMethod.Store(AutoExposureMethod);
						GSecondaryDynamicGIMethod.Store(DynamicGIMethod);
						GSecondaryReflectionMethod.Store(ReflectionMethod);
						GSecondaryIndirectLightingIntensity.Store(
							View.FinalPostProcessSettings.IndirectLightingIntensity);
						if (Count == 1 || (Count % 60) == 0)
						{
							UE_LOG(LogTemp, Display,
								TEXT("PortalViewParity Secondary Tonemap Frame=%llu Count=%llu PreExposure=%.9g AA=%d SceneRect=%dx%d ExposureBias=%.6f AutoExposureMethod=%d DynamicGI=%d Reflection=%d IndirectLightingIntensity=%.6f"),
								GFrameCounter, Count, PreExposure, AAMethod,
								SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
								View.FinalPostProcessSettings.AutoExposureBias,
								AutoExposureMethod, DynamicGIMethod, ReflectionMethod,
								View.FinalPostProcessSettings.IndirectLightingIntensity);
						}
					}
					else
					{
						const uint64 Count = GMainTonemapFrames.Load() + 1;
						GMainTonemapFrames.Store(Count);
						GLastMainFrame.Store(GFrameCounter);
						GMainPreExposure.Store(PreExposure);
						GMainAAMethod.Store(AAMethod);
						GMainWidth.Store(SceneColor.ViewRect.Width());
						GMainHeight.Store(SceneColor.ViewRect.Height());
						GMainExposureBias.Store(View.FinalPostProcessSettings.AutoExposureBias);
						GMainAutoExposureMethod.Store(AutoExposureMethod);
						GMainDynamicGIMethod.Store(DynamicGIMethod);
						GMainReflectionMethod.Store(ReflectionMethod);
						GMainIndirectLightingIntensity.Store(
							View.FinalPostProcessSettings.IndirectLightingIntensity);
						if (Count == 1 || (Count % 60) == 0)
						{
							UE_LOG(LogTemp, Display,
								TEXT("PortalViewParity Main Tonemap Frame=%llu Count=%llu PreExposure=%.9g AA=%d SceneRect=%dx%d ExposureBias=%.6f AutoExposureMethod=%d DynamicGI=%d Reflection=%d IndirectLightingIntensity=%.6f"),
								GFrameCounter, Count, PreExposure, AAMethod,
								SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
								View.FinalPostProcessSettings.AutoExposureBias,
								AutoExposureMethod, DynamicGIMethod, ReflectionMethod,
								View.FinalPostProcessSettings.IndirectLightingIntensity);
						}
					}

					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return CVarViewParityDiagnostics.GetValueOnAnyThread() != 0
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}
	};

	TSharedPtr<FPortalViewParityDiagnosticsExtension, ESPMode::ThreadSafe> GDiagnosticsExtension;

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

	void SetDiagnosticsEnabled(const bool bEnabled)
	{
		if (IConsoleVariable* Var =
			IConsoleManager::Get().FindConsoleVariable(TEXT("portal.ViewParityDiagnostics")))
		{
			Var->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
		}
	}

	void WriteReport(const TCHAR* Status)
	{
		const float SecondaryProducerPreExposure = ReadFloatCVar(
			TEXT("portal.SecondaryPreExposure"), 1.0f);
		const int32 PreExposureRebase = ReadIntCVar(
			TEXT("portal.PreExposureRebase"), 0);
		const float PrimaryFraction = ReadFloatCVar(
			TEXT("portal.FullViewFamilyTSRPrimaryFraction"), 1.0f);
		const bool bSecondaryObserved = GSecondaryTonemapFrames.Load() > 0;
		const float SecondaryComparisonPreExposure = bSecondaryObserved
			? GSecondaryObservedPreExposure.Load()
			: SecondaryProducerPreExposure;
		const float RebaseScale = SecondaryComparisonPreExposure > UE_SMALL_NUMBER
			? GMainPreExposure.Load() / SecondaryComparisonPreExposure
			: 0.0f;

		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"mainTonemapFrames\":%llu,\n")
			TEXT("  \"secondaryTonemapFramesObservedByDiagnosticExtension\":%llu,\n")
			TEXT("  \"lastMainFrame\":%llu,\n")
			TEXT("  \"lastSecondaryFrame\":%llu,\n")
			TEXT("  \"main\":{\"preExposure\":%.9g,\"aaMethod\":%d,\"sceneRect\":[%d,%d],\"exposureBias\":%.6f,\"autoExposureMethod\":%d,\"dynamicGI\":%d,\"reflection\":%d,\"indirectLightingIntensity\":%.6f},\n")
			TEXT("  \"secondaryObserved\":{\"available\":%s,\"preExposure\":%.9g,\"aaMethod\":%d,\"sceneRect\":[%d,%d],\"exposureBias\":%.6f,\"autoExposureMethod\":%d,\"dynamicGI\":%d,\"reflection\":%d,\"indirectLightingIntensity\":%.6f},\n")
			TEXT("  \"secondaryProducerPolicy\":{\"eyeAdaptation\":false,\"motionBlur\":false,\"depthOfField\":false,\"temporalAA\":true,\"screenPercentage\":true,\"sceneCaptureSource\":\"SCS_FinalColorHDR\",\"forcedDynamicGI\":\"Lumen\",\"forcedReflection\":\"Lumen\",\"measuredPreExposureCVar\":%.9g,\"primaryResolutionFraction\":%.6f},\n")
			TEXT("  \"preExposureRebaseEnabled\":%s,\n")
			TEXT("  \"mainToSecondaryPreExposureScale\":%.9g,\n")
			TEXT("  \"interpretation\":\"If secondaryObserved.available is false, the manually constructed additional family did not attach this global diagnostic extension; use secondaryProducerPolicy plus portal.SecondaryPreExposure as the authoritative configured secondary evidence. The primary mismatch candidate to test next is the deliberate secondary EyeAdaptation=false policy versus the real main-view exposure/post-process path; do not compensate with brightness or gamma.\"\n")
			TEXT("}\n"),
			Status,
			GMainTonemapFrames.Load(),
			GSecondaryTonemapFrames.Load(),
			GLastMainFrame.Load(),
			GLastSecondaryFrame.Load(),
			GMainPreExposure.Load(), GMainAAMethod.Load(), GMainWidth.Load(), GMainHeight.Load(),
			GMainExposureBias.Load(), GMainAutoExposureMethod.Load(), GMainDynamicGIMethod.Load(),
			GMainReflectionMethod.Load(), GMainIndirectLightingIntensity.Load(),
			bSecondaryObserved ? TEXT("true") : TEXT("false"),
			GSecondaryObservedPreExposure.Load(), GSecondaryAAMethod.Load(),
			GSecondaryWidth.Load(), GSecondaryHeight.Load(), GSecondaryExposureBias.Load(),
			GSecondaryAutoExposureMethod.Load(), GSecondaryDynamicGIMethod.Load(),
			GSecondaryReflectionMethod.Load(), GSecondaryIndirectLightingIntensity.Load(),
			SecondaryProducerPreExposure, PrimaryFraction,
			PreExposureRebase != 0 ? TEXT("true") : TEXT("false"),
			RebaseScale);

		const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("AutomationReports");
		IFileManager::Get().MakeDirectory(*ReportDir, true);
		const FString ReportPath = ReportDir / TEXT("PortalViewParityDiagnostics.json");
		FFileHelper::SaveStringToFile(Json, *ReportPath);

		UE_LOG(LogTemp, Display,
			TEXT("PortalViewParity Report MainFrames=%llu SecondaryObservedFrames=%llu MainPreExposure=%.9g SecondaryProducerPreExposure=%.9g Rebase=%d RebaseScale=%.9g MainAA=%d MainExposureBias=%.6f MainAutoExposureMethod=%d MainGI=%d MainReflection=%d MainIndirect=%.6f SecondaryPolicyEyeAdaptation=0 PrimaryFraction=%.3f Path=%s"),
			GMainTonemapFrames.Load(),
			GSecondaryTonemapFrames.Load(),
			GMainPreExposure.Load(),
			SecondaryProducerPreExposure,
			PreExposureRebase,
			RebaseScale,
			GMainAAMethod.Load(),
			GMainExposureBias.Load(),
			GMainAutoExposureMethod.Load(),
			GMainDynamicGIMethod.Load(),
			GMainReflectionMethod.Load(),
			GMainIndirectLightingIntensity.Load(),
			PrimaryFraction,
			*ReportPath);
	}

	void StartDiagnostics()
	{
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalViewParity: no PIE/Game world found. Enter PIE first."));
			return;
		}
		if (GDiagnosticsExtension.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalViewParity: diagnostics already running."));
			return;
		}

		ResetTelemetry();
		SetDiagnosticsEnabled(true);
		GDiagnosticsExtension =
			FSceneViewExtensions::NewExtension<FPortalViewParityDiagnosticsExtension>(World);
		UE_LOG(LogTemp, Display,
			TEXT("PortalViewParity: START. Capturing main Tonemap telemetry and any additional-family callbacks that expose the registered extension. The current secondary producer policy deliberately has EyeAdaptation=0, MotionBlur=0 and DOF=0."));
	}

	void DumpDiagnostics()
	{
		WriteReport(GDiagnosticsExtension.IsValid() ? TEXT("RUNNING") : TEXT("STOPPED"));
	}

	void StopDiagnostics()
	{
		if (!GDiagnosticsExtension.IsValid())
		{
			SetDiagnosticsEnabled(false);
			UE_LOG(LogTemp, Display, TEXT("PortalViewParity: diagnostics not running."));
			return;
		}

		WriteReport(TEXT("STOPPING"));
		SetDiagnosticsEnabled(false);
		GDiagnosticsExtension.Reset();
		UE_LOG(LogTemp, Display, TEXT("PortalViewParity: STOP."));
	}

	FAutoConsoleCommand GStartViewParityDiagnosticsCommand(
		TEXT("portal.StartViewParityDiagnostics"),
		TEXT("Start STEP 1B.14B main-vs-secondary view/exposure/post-process diagnostics."),
		FConsoleCommandDelegate::CreateStatic(&StartDiagnostics));

	FAutoConsoleCommand GDumpViewParityDiagnosticsCommand(
		TEXT("portal.DumpViewParityDiagnostics"),
		TEXT("Write STEP 1B.14B parity telemetry to Saved/AutomationReports/PortalViewParityDiagnostics.json."),
		FConsoleCommandDelegate::CreateStatic(&DumpDiagnostics));

	FAutoConsoleCommand GStopViewParityDiagnosticsCommand(
		TEXT("portal.StopViewParityDiagnostics"),
		TEXT("Stop STEP 1B.14B parity diagnostics."),
		FConsoleCommandDelegate::CreateStatic(&StopDiagnostics));
}
