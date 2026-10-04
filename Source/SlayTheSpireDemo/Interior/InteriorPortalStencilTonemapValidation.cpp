#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"

namespace InteriorPortalStencilTonemapValidationPrivate
{
	constexpr uint8 PortalStencilBit = 0x40;

	TAutoConsoleVariable<int32> CVarTonemapStencilProofEnabled(
		TEXT("portal.StencilIdentityTonemapValidation"),
		0,
		TEXT("STEP 1B.12D-A isolation proof. 1=visualize the already-written portal stencil identity after the real Tonemap pass so the proof cannot feed main eye adaptation."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarTonemapStencilProofDiagnostics(
		TEXT("portal.StencilIdentityTonemapDiagnostics"),
		1,
		TEXT("STEP 1B.12D-A post-tonemap proof diagnostics. 0=quiet, 1=periodic Tonemap stencil-test logs."),
		ECVF_Default);

	BEGIN_SHADER_PARAMETER_STRUCT(FPortalStencilTonemapOverlayParameters, )
		SHADER_PARAMETER(float, OverlayPreExposure)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FPortalStencilTonemapOverlayPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FPortalStencilTonemapOverlayPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FPortalStencilTonemapOverlayPS, FGlobalShader);
		using FParameters = FPortalStencilTonemapOverlayParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FPortalStencilTonemapOverlayPS,
		TEXT("/Project/InteriorPortalStencilIdentity.usf"), TEXT("StencilOverlayPS"), SF_Pixel);

	TAtomic<uint64> GTonemapOverlayFrames { 0 };
	TAtomic<bool> GLastStencilTargetable { false };
	TAtomic<bool> GLastTonemapPassEnabled { false };
	TAtomic<int32> GLastSceneRectWidth { 0 };
	TAtomic<int32> GLastSceneRectHeight { 0 };

	class FPortalStencilTonemapValidationExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalStencilTonemapValidationExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
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
			if (Pass != ISceneViewExtension::EPostProcessingPass::Tonemap
				|| !InView.Family
				|| InView.Family->bAdditionalViewFamily
				|| CVarTonemapStencilProofEnabled.GetValueOnAnyThread() == 0)
			{
				return;
			}

			GLastTonemapPassEnabled.Store(bIsPassEnabled);
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

					FRDGTextureRef SceneDepthTexture = nullptr;
					const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures =
						CreateSceneTextureUniformBuffer(
							GraphBuilder, View, ESceneTextureSetupMode::SceneDepth);
					if (SceneTextures)
					{
						const FSceneTextureUniformParameters* Contents = SceneTextures->GetContents();
						if (Contents)
						{
							SceneDepthTexture = Contents->SceneDepthTexture;
						}
					}

					const bool bStencilTargetable = SceneDepthTexture
						&& SceneDepthTexture->Desc.Format == PF_DepthStencil
						&& EnumHasAnyFlags(
							SceneDepthTexture->Desc.Flags, TexCreate_DepthStencilTargetable);
					GLastStencilTargetable.Store(bStencilTargetable);
					GLastTonemapPassEnabled.Store(bIsPassEnabled);
					GLastSceneRectWidth.Store(SceneColor.ViewRect.Width());
					GLastSceneRectHeight.Store(SceneColor.ViewRect.Height());
					if (!bStencilTargetable)
					{
						return SceneColor;
					}

					const FScreenPassRenderTarget Output = FScreenPassRenderTarget::CreateFromInput(
						GraphBuilder, SceneColor, ERenderTargetLoadAction::ELoad,
						TEXT("InteriorPortalStencilTonemapOverlay"));
					const FScreenPassTextureViewport OutputViewport(Output);
					const FScreenPassTextureViewport DepthViewport(
						SceneDepthTexture, SceneColor.ViewRect);

					FPortalStencilTonemapOverlayParameters* Parameters =
						GraphBuilder.AllocParameters<FPortalStencilTonemapOverlayParameters>();
					// The callback is registered on the real Tonemap pass, so diagnostic
					// cyan is authored in the display-domain output and must not be scaled
					// by the scene's pre-exposure. The shared shader multiplies by this value.
					Parameters->OverlayPreExposure = 1.0f;
					Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
					Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
						SceneDepthTexture,
						ERenderTargetLoadAction::ELoad,
						ERenderTargetLoadAction::ELoad,
						FExclusiveDepthStencil::DepthNop_StencilRead);

					TShaderMapRef<FScreenPassVS> VertexShader(
						GetGlobalShaderMap(View.GetFeatureLevel()));
					TShaderMapRef<FPortalStencilTonemapOverlayPS> PixelShader(
						GetGlobalShaderMap(View.GetFeatureLevel()));
					FRHIDepthStencilState* TestStencilState =
						TStaticDepthStencilState<
							false, CF_Always,
							true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
							false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
							PortalStencilBit, 0x00>::GetRHI();

					AddDrawScreenPass(
						GraphBuilder,
						RDG_EVENT_NAME("InteriorPortal::VisualizeStencilIdentityAfterTonemap"),
						View,
						OutputViewport,
						DepthViewport,
						FScreenPassPipelineState(
							VertexShader, PixelShader,
							TStaticBlendState<>::GetRHI(), TestStencilState),
						Parameters,
						EScreenPassDrawFlags::None,
						[PixelShader, Parameters](FRHICommandList& RHICmdList)
						{
							SetShaderParameters(
								RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);
							RHICmdList.SetStencilRef(PortalStencilBit);
						});

					const uint64 Count = GTonemapOverlayFrames.Load() + 1;
					GTonemapOverlayFrames.Store(Count);
					if (CVarTonemapStencilProofDiagnostics.GetValueOnRenderThread() != 0
						&& (Count == 1 || (Count % 60) == 0))
					{
						UE_LOG(LogTemp, Display,
							TEXT("PortalStencilTonemapProof Tonemap Frame=%llu Count=%llu PassEnabled=%d SceneRect=%dx%d StencilTargetable=1 StencilBit=0x%02x"),
							GFrameCounter, Count, bIsPassEnabled ? 1 : 0,
							SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(), PortalStencilBit);
					}
					return FScreenPassTexture(Output);
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return CVarTonemapStencilProofEnabled.GetValueOnAnyThread() != 0
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}
	};

	TSharedPtr<FPortalStencilTonemapValidationExtension, ESPMode::ThreadSafe> GTonemapProofExtension;

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
		GTonemapOverlayFrames.Store(0);
		GLastStencilTargetable.Store(false);
		GLastTonemapPassEnabled.Store(false);
		GLastSceneRectWidth.Store(0);
		GLastSceneRectHeight.Store(0);
	}

	void WriteReport(const TCHAR* Status)
	{
		const FString Json = FString::Printf(
			TEXT("{\n  \"status\": \"%s\",\n  \"tonemapOverlayFrames\": %llu,\n  \"lastStencilTargetable\": %s,\n  \"lastTonemapPassEnabled\": %s,\n  \"lastSceneRect\": [%d, %d],\n  \"stencilBit\": 64,\n  \"claimBoundary\": \"STEP 1B.12D-A post-tonemap isolation proof only. It verifies that the stencil bit written by the main stencil validator survives to the real Tonemap callback and can gate display-domain cyan without feeding scene exposure. It does not reserve bit 0x40 engine-wide or promote production composition.\"\n}\n"),
			Status,
			GTonemapOverlayFrames.Load(),
			GLastStencilTargetable.Load() ? TEXT("true") : TEXT("false"),
			GLastTonemapPassEnabled.Load() ? TEXT("true") : TEXT("false"),
			GLastSceneRectWidth.Load(),
			GLastSceneRectHeight.Load());

		const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("AutomationReports");
		IFileManager::Get().MakeDirectory(*ReportDir, true);
		const FString ReportPath = ReportDir / TEXT("PortalStencilTonemapValidation.json");
		FFileHelper::SaveStringToFile(Json, *ReportPath);
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilTonemapProof: report written to %s Frames=%llu Targetable=%d TonemapEnabled=%d SceneRect=%dx%d"),
			*ReportPath,
			GTonemapOverlayFrames.Load(),
			GLastStencilTargetable.Load() ? 1 : 0,
			GLastTonemapPassEnabled.Load() ? 1 : 0,
			GLastSceneRectWidth.Load(), GLastSceneRectHeight.Load());
	}

	void StartTonemapProof()
	{
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalStencilTonemapProof: no PIE/Game world found. Enter PIE first."));
			return;
		}
		if (GTonemapProofExtension.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalStencilTonemapProof: already running."));
			return;
		}

		ResetCounters();
		CVarTonemapStencilProofEnabled->Set(1, ECVF_SetByCode);
		GTonemapProofExtension =
			FSceneViewExtensions::NewExtension<FPortalStencilTonemapValidationExtension>(World);
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilTonemapProof: started. Keep portal.StencilIdentityValidation running with portal.StencilIdentityDebug=0. Cyan is emitted only after the real Tonemap pass and is gated by main stencil bit 0x40."));
	}

	void DumpTonemapProof()
	{
		WriteReport(GTonemapProofExtension.IsValid() ? TEXT("RUNNING") : TEXT("STOPPED"));
	}

	void StopTonemapProof()
	{
		if (!GTonemapProofExtension.IsValid())
		{
			CVarTonemapStencilProofEnabled->Set(0, ECVF_SetByCode);
			UE_LOG(LogTemp, Display, TEXT("PortalStencilTonemapProof: not running."));
			return;
		}

		WriteReport(TEXT("STOPPED"));
		CVarTonemapStencilProofEnabled->Set(0, ECVF_SetByCode);
		FlushRenderingCommands();
		GTonemapProofExtension.Reset();
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilTonemapProof: stopped. Frames=%llu Targetable=%d TonemapEnabled=%d."),
			GTonemapOverlayFrames.Load(),
			GLastStencilTargetable.Load() ? 1 : 0,
			GLastTonemapPassEnabled.Load() ? 1 : 0);
	}

	FAutoConsoleCommand GStartTonemapProofCommand(
		TEXT("portal.StartStencilIdentityTonemapValidation"),
		TEXT("STEP 1B.12D-A: visualize the existing main-stencil portal identity after Tonemap without feeding eye adaptation."),
		FConsoleCommandDelegate::CreateStatic(&StartTonemapProof));

	FAutoConsoleCommand GDumpTonemapProofCommand(
		TEXT("portal.DumpStencilIdentityTonemapValidation"),
		TEXT("Write STEP 1B.12D-A post-tonemap proof telemetry to Saved/AutomationReports/PortalStencilTonemapValidation.json."),
		FConsoleCommandDelegate::CreateStatic(&DumpTonemapProof));

	FAutoConsoleCommand GStopTonemapProofCommand(
		TEXT("portal.StopStencilIdentityTonemapValidation"),
		TEXT("Stop STEP 1B.12D-A post-tonemap stencil identity visualization and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopTonemapProof));
}
