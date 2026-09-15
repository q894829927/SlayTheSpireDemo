#include "InteriorPortal.h"
#include "InteriorPortalProjectiveAperture.h"
#include "InteriorPortalSystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

namespace InteriorPortalStencilIdentityValidationPrivate
{
	// Bounded feasibility bit only. This is not a production engine-wide stencil
	// reservation. The validator can isolate the bit every frame so stale / pre-
	// existing values do not masquerade as portal identity during the proof.
	constexpr uint8 PortalStencilBit = 0x40;

	TAutoConsoleVariable<int32> CVarStencilIdentityValidation(
		TEXT("portal.StencilIdentityValidation"),
		0,
		TEXT("STEP 1B.12D-A. 1=write visible linked portal apertures into main SceneDepth stencil bit 0x40 at BeforeDOF."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarStencilIdentityDebug(
		TEXT("portal.StencilIdentityDebug"),
		1,
		TEXT("STEP 1B.12D-A. 1=AfterDOF overlay cyan only where the real main-stencil bit 0x40 passes CF_Equal."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarStencilIdentityDiagnostics(
		TEXT("portal.StencilIdentityDiagnostics"),
		1,
		TEXT("STEP 1B.12D-A diagnostics. 0=quiet, 1=periodic SetupView / BeforeDOF / AfterDOF logs."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarStencilIdentityIsolateBit(
		TEXT("portal.StencilIdentityIsolateBit"),
		1,
		TEXT("STEP 1B.12D-A validation only. 1=clear only stencil bit 0x40 across the current main view immediately before marking visible portal apertures. This prevents stale/pre-existing 0x40 values from contaminating the proof. Not a production ownership policy."),
		ECVF_RenderThreadSafe);

	struct FPortalApertureSnapshot
	{
		FVector4f Row0 = FVector4f(1, 0, 0, 0);
		FVector4f Row1 = FVector4f(0, 1, 0, 0);
		FVector4f Row2 = FVector4f(0, 0, 1, 0);
		int32 EndpointIndex = INDEX_NONE;
	};

	BEGIN_SHADER_PARAMETER_STRUCT(FPortalStencilClearParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FPortalStencilClearPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FPortalStencilClearPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FPortalStencilClearPS, FGlobalShader);
		using FParameters = FPortalStencilClearParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FPortalStencilClearPS,
		TEXT("/Project/InteriorPortalStencilIdentity.usf"), TEXT("ClearStencilPS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FPortalStencilMarkParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow0)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow1)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow2)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FPortalStencilMarkPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FPortalStencilMarkPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FPortalStencilMarkPS, FGlobalShader);
		using FParameters = FPortalStencilMarkParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FPortalStencilMarkPS,
		TEXT("/Project/InteriorPortalStencilIdentity.usf"), TEXT("MarkAperturePS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FPortalStencilOverlayParameters, )
		SHADER_PARAMETER(float, OverlayPreExposure)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FPortalStencilOverlayPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FPortalStencilOverlayPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FPortalStencilOverlayPS, FGlobalShader);
		using FParameters = FPortalStencilOverlayParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FPortalStencilOverlayPS,
		TEXT("/Project/InteriorPortalStencilIdentity.usf"), TEXT("StencilOverlayPS"), SF_Pixel);

	TAtomic<uint64> GSetupViewFrames { 0 };
	TAtomic<uint64> GStencilWriteFrames { 0 };
	TAtomic<uint64> GStencilOverlayFrames { 0 };
	TAtomic<int32> GLastApertureCount { 0 };
	TAtomic<bool> GLastStencilTargetable { false };
	TAtomic<bool> GLastAfterDOFPassEnabled { false };
	TAtomic<bool> GLastBitIsolated { false };
	TAtomic<float> GLastOverlayPreExposure { 1.0f };

	bool ProjectPointToScreenUV(
		const FVector& WorldPoint,
		const FMatrix& ViewProjection,
		FVector2f& OutUV)
	{
		const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(WorldPoint, 1.0));
		if (!FMath::IsFinite(Clip.W) || Clip.W <= 1.0e-4)
		{
			return false;
		}
		const double InvW = 1.0 / Clip.W;
		const double NdcX = Clip.X * InvW;
		const double NdcY = Clip.Y * InvW;
		OutUV = FVector2f(
			float(0.5 * (NdcX + 1.0)),
			float(0.5 * (1.0 - NdcY)));
		return FMath::IsFinite(OutUV.X) && FMath::IsFinite(OutUV.Y);
	}

	bool IsPortalPotentiallyVisible(
		const AInteriorPortal& Portal,
		const FMatrix& ViewProjection)
	{
		const FTransform Frame = Portal.GetLogicalFrame();
		const FVector Center = Frame.GetLocation();
		FVector2f CenterUV;
		if (!ProjectPointToScreenUV(Center, ViewProjection, CenterUV))
		{
			return false;
		}

		const FVector AxisY = Frame.GetUnitAxis(EAxis::Y);
		const FVector AxisZ = Frame.GetUnitAxis(EAxis::Z);
		FVector2f MinUV(FLT_MAX, FLT_MAX);
		FVector2f MaxUV(-FLT_MAX, -FLT_MAX);
		bool bAnyProjected = false;
		constexpr int32 SampleCount = 16;
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const double Angle = (2.0 * UE_PI * double(Index)) / double(SampleCount);
			const FVector Point = Center
				+ AxisY * (Portal.HalfWidth * FMath::Cos(Angle))
				+ AxisZ * (Portal.HalfHeight * FMath::Sin(Angle));
			FVector2f UV;
			if (!ProjectPointToScreenUV(Point, ViewProjection, UV))
			{
				continue;
			}
			bAnyProjected = true;
			MinUV.X = FMath::Min(MinUV.X, UV.X);
			MinUV.Y = FMath::Min(MinUV.Y, UV.Y);
			MaxUV.X = FMath::Max(MaxUV.X, UV.X);
			MaxUV.Y = FMath::Max(MaxUV.Y, UV.Y);
		}

		if (!bAnyProjected)
		{
			return false;
		}
		return MaxUV.X >= 0.0f && MaxUV.Y >= 0.0f
			&& MinUV.X <= 1.0f && MinUV.Y <= 1.0f;
	}

	class FPortalStencilIdentityExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalStencilIdentityExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, World(InWorld)
		{
		}

		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
		{
			if (InViewFamily.bAdditionalViewFamily
				|| CVarStencilIdentityValidation.GetValueOnGameThread() == 0)
			{
				return;
			}

			TArray<FPortalApertureSnapshot> NewSnapshots;
			UWorld* LocalWorld = World.Get();
			AInteriorPortalSystem* PortalSystem = nullptr;
			if (LocalWorld)
			{
				for (TActorIterator<AInteriorPortalSystem> It(LocalWorld); It; ++It)
				{
					PortalSystem = *It;
					break;
				}
			}

			if (IsValid(PortalSystem) && PortalSystem->IsLinked())
			{
				const FMatrix ViewProjection = InView.ViewMatrices.GetViewProjectionMatrix();
				int32 EndpointIndex = 0;
				for (AInteriorPortal* Portal : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
				{
					if (IsValid(Portal) && Portal->bPlaced
						&& IsPortalPotentiallyVisible(*Portal, ViewProjection))
					{
						InteriorPortalProjectiveAperture::FScreenToPortalMapping Mapping;
						if (InteriorPortalProjectiveAperture::BuildScreenToPortalMapping(
							Portal->GetLogicalFrame(), Portal->HalfWidth, Portal->HalfHeight,
							ViewProjection, Mapping))
						{
							FPortalApertureSnapshot Snapshot;
							Snapshot.Row0 = Mapping.Row0;
							Snapshot.Row1 = Mapping.Row1;
							Snapshot.Row2 = Mapping.Row2;
							Snapshot.EndpointIndex = EndpointIndex;
							NewSnapshots.Add(Snapshot);
						}
					}
					++EndpointIndex;
				}
			}

			{
				FScopeLock Lock(&SnapshotMutex);
				Snapshots = MoveTemp(NewSnapshots);
				GLastApertureCount.Store(Snapshots.Num());
			}

			const uint64 SetupCount = GSetupViewFrames.Load() + 1;
			GSetupViewFrames.Store(SetupCount);
			if (CVarStencilIdentityDiagnostics.GetValueOnGameThread() != 0
				&& (SetupCount == 1 || (SetupCount % 60) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalStencilIdentity SetupView Count=%llu VisibleApertures=%d AdditionalFamily=0 StencilBit=0x%02x IsolateBit=%d"),
					SetupCount, GLastApertureCount.Load(), PortalStencilBit,
					CVarStencilIdentityIsolateBit.GetValueOnGameThread() != 0 ? 1 : 0);
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
			if (!InView.Family || InView.Family->bAdditionalViewFamily
				|| CVarStencilIdentityValidation.GetValueOnAnyThread() == 0)
			{
				return;
			}

			if (Pass == ISceneViewExtension::EPostProcessingPass::BeforeDOF)
			{
				const TArray<FPortalApertureSnapshot> SnapshotCopy = GetSnapshots();
				if (SnapshotCopy.IsEmpty())
				{
					return;
				}

				InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
					[SnapshotCopy](FRDGBuilder& GraphBuilder, const FSceneView& View,
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
						if (!bStencilTargetable)
						{
							return SceneColor;
						}

						const FScreenPassTextureViewport DepthViewport(
							SceneDepthTexture, SceneColor.ViewRect);
						TShaderMapRef<FScreenPassVS> VertexShader(
							GetGlobalShaderMap(View.GetFeatureLevel()));

						const bool bIsolateBit =
							CVarStencilIdentityIsolateBit.GetValueOnRenderThread() != 0;
						GLastBitIsolated.Store(bIsolateBit);
						if (bIsolateBit)
						{
							FPortalStencilClearParameters* ClearParameters =
								GraphBuilder.AllocParameters<FPortalStencilClearParameters>();
							ClearParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
								SceneDepthTexture,
								ERenderTargetLoadAction::ELoad,
								ERenderTargetLoadAction::ELoad,
								FExclusiveDepthStencil::DepthNop_StencilWrite);
							TShaderMapRef<FPortalStencilClearPS> ClearPixelShader(
								GetGlobalShaderMap(View.GetFeatureLevel()));
							FRHIDepthStencilState* ClearStencilState =
								TStaticDepthStencilState<
									false, CF_Always,
									true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
									false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
									PortalStencilBit, PortalStencilBit>::GetRHI();
							AddDrawScreenPass(
								GraphBuilder,
								RDG_EVENT_NAME("InteriorPortal::IsolateStencilBit"),
								View,
								DepthViewport,
								DepthViewport,
								FScreenPassPipelineState(
									VertexShader, ClearPixelShader,
									TStaticBlendState<>::GetRHI(), ClearStencilState),
								ClearParameters,
								EScreenPassDrawFlags::None,
								[ClearPixelShader, ClearParameters](FRHICommandList& RHICmdList)
								{
									SetShaderParameters(
										RHICmdList, ClearPixelShader, ClearPixelShader.GetPixelShader(), *ClearParameters);
									RHICmdList.SetStencilRef(0);
								});
						}

						TShaderMapRef<FPortalStencilMarkPS> PixelShader(
							GetGlobalShaderMap(View.GetFeatureLevel()));
						FRHIDepthStencilState* MarkStencilState =
							TStaticDepthStencilState<
								false, CF_Always,
								true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
								false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
								PortalStencilBit, PortalStencilBit>::GetRHI();

						for (const FPortalApertureSnapshot& Snapshot : SnapshotCopy)
						{
							FPortalStencilMarkParameters* Parameters =
								GraphBuilder.AllocParameters<FPortalStencilMarkParameters>();
							Parameters->Output = GetScreenPassTextureViewportParameters(DepthViewport);
							Parameters->ScreenToPortalRow0 = Snapshot.Row0;
							Parameters->ScreenToPortalRow1 = Snapshot.Row1;
							Parameters->ScreenToPortalRow2 = Snapshot.Row2;
							Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
								SceneDepthTexture,
								ERenderTargetLoadAction::ELoad,
								ERenderTargetLoadAction::ELoad,
								FExclusiveDepthStencil::DepthNop_StencilWrite);

							AddDrawScreenPass(
								GraphBuilder,
								RDG_EVENT_NAME("InteriorPortal::MarkStencilIdentity Endpoint=%d", Snapshot.EndpointIndex),
								View,
								DepthViewport,
								DepthViewport,
								FScreenPassPipelineState(
									VertexShader, PixelShader,
									TStaticBlendState<>::GetRHI(), MarkStencilState),
								Parameters,
								EScreenPassDrawFlags::None,
								[PixelShader, Parameters](FRHICommandList& RHICmdList)
								{
									SetShaderParameters(
										RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);
									RHICmdList.SetStencilRef(PortalStencilBit);
								});
						}

						const uint64 WriteCount = GStencilWriteFrames.Load() + 1;
						GStencilWriteFrames.Store(WriteCount);
						if (CVarStencilIdentityDiagnostics.GetValueOnRenderThread() != 0
							&& (WriteCount == 1 || (WriteCount % 60) == 0))
						{
							UE_LOG(LogTemp, Display,
								TEXT("PortalStencilIdentity BeforeDOF Frame=%llu Count=%llu VisibleApertures=%d StencilTargetable=1 Format=%d StencilBit=0x%02x Isolated=%d"),
								GFrameCounter, WriteCount, SnapshotCopy.Num(),
								int32(SceneDepthTexture->Desc.Format), PortalStencilBit,
								bIsolateBit ? 1 : 0);
						}
						return SceneColor;
					}));
				return;
			}

			if (Pass == ISceneViewExtension::EPostProcessingPass::AfterDOF
				&& CVarStencilIdentityDebug.GetValueOnAnyThread() != 0)
			{
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
						if (!SceneDepthTexture
							|| SceneDepthTexture->Desc.Format != PF_DepthStencil
							|| !EnumHasAnyFlags(
								SceneDepthTexture->Desc.Flags, TexCreate_DepthStencilTargetable))
						{
							return SceneColor;
						}

						const FScreenPassRenderTarget Output = FScreenPassRenderTarget::CreateFromInput(
							GraphBuilder, SceneColor, ERenderTargetLoadAction::ELoad,
							TEXT("InteriorPortalStencilIdentityOverlay"));
						const FScreenPassTextureViewport OutputViewport(Output);
						const FScreenPassTextureViewport DepthViewport(
							SceneDepthTexture, SceneColor.ViewRect);

						FPortalStencilOverlayParameters* Parameters =
							GraphBuilder.AllocParameters<FPortalStencilOverlayParameters>();
						const float OverlayPreExposure = View.State
							? FMath::Max(View.State->GetPreExposure(), UE_SMALL_NUMBER)
							: 1.0f;
						GLastOverlayPreExposure.Store(OverlayPreExposure);
						Parameters->OverlayPreExposure = OverlayPreExposure;
						Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
						Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
							SceneDepthTexture,
							ERenderTargetLoadAction::ELoad,
							ERenderTargetLoadAction::ELoad,
							FExclusiveDepthStencil::DepthNop_StencilRead);

						TShaderMapRef<FScreenPassVS> VertexShader(
							GetGlobalShaderMap(View.GetFeatureLevel()));
						TShaderMapRef<FPortalStencilOverlayPS> PixelShader(
							GetGlobalShaderMap(View.GetFeatureLevel()));
						FRHIDepthStencilState* TestStencilState =
							TStaticDepthStencilState<
								false, CF_Always,
								true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
								false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
								PortalStencilBit, 0x00>::GetRHI();

						AddDrawScreenPass(
							GraphBuilder,
							RDG_EVENT_NAME("InteriorPortal::VisualizeStencilIdentity"),
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

						const uint64 OverlayCount = GStencilOverlayFrames.Load() + 1;
						GStencilOverlayFrames.Store(OverlayCount);
						GLastAfterDOFPassEnabled.Store(bIsPassEnabled);
						if (CVarStencilIdentityDiagnostics.GetValueOnRenderThread() != 0
							&& (OverlayCount == 1 || (OverlayCount % 60) == 0))
						{
							UE_LOG(LogTemp, Display,
								TEXT("PortalStencilIdentity AfterDOF Frame=%llu Count=%llu PassEnabled=%d SceneRect=%dx%d StencilBit=0x%02x OverlayPreExposure=%.9g"),
								GFrameCounter, OverlayCount, bIsPassEnabled ? 1 : 0,
								SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
								PortalStencilBit, OverlayPreExposure);
						}
						return FScreenPassTexture(Output);
					}));
			}
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return CVarStencilIdentityValidation.GetValueOnAnyThread() != 0
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		TArray<FPortalApertureSnapshot> GetSnapshots() const
		{
			FScopeLock Lock(&SnapshotMutex);
			return Snapshots;
		}

		TWeakObjectPtr<UWorld> World;
		mutable FCriticalSection SnapshotMutex;
		TArray<FPortalApertureSnapshot> Snapshots;
	};

	TSharedPtr<FPortalStencilIdentityExtension, ESPMode::ThreadSafe> GStencilIdentityExtension;

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
		GStencilWriteFrames.Store(0);
		GStencilOverlayFrames.Store(0);
		GLastApertureCount.Store(0);
		GLastStencilTargetable.Store(false);
		GLastAfterDOFPassEnabled.Store(false);
		GLastBitIsolated.Store(false);
		GLastOverlayPreExposure.Store(1.0f);
	}

	void WriteReport(const TCHAR* Status)
	{
		const FString Json = FString::Printf(
			TEXT("{\n  \"status\": \"%s\",\n  \"setupViewFrames\": %llu,\n  \"stencilWriteFrames\": %llu,\n  \"stencilOverlayFrames\": %llu,\n  \"lastVisibleApertureCount\": %d,\n  \"lastStencilTargetable\": %s,\n  \"lastAfterDOFPassEnabled\": %s,\n  \"bitIsolationEnabled\": %s,\n  \"overlayPreExposure\": %.9g,\n  \"stencilBit\": 64,\n  \"claimBoundary\": \"STEP 1B.12D-A bounded validation only: visible linked apertures are written into main stencil bit 0x40 after isolating that bit for the proof, and a real CF_Equal AfterDOF stencil test visualizes identity. This does not reserve 0x40 engine-wide or define a production collision policy.\"\n}\n"),
			Status,
			GSetupViewFrames.Load(),
			GStencilWriteFrames.Load(),
			GStencilOverlayFrames.Load(),
			GLastApertureCount.Load(),
			GLastStencilTargetable.Load() ? TEXT("true") : TEXT("false"),
			GLastAfterDOFPassEnabled.Load() ? TEXT("true") : TEXT("false"),
			GLastBitIsolated.Load() ? TEXT("true") : TEXT("false"),
			GLastOverlayPreExposure.Load());

		const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("AutomationReports");
		IFileManager::Get().MakeDirectory(*ReportDir, true);
		const FString ReportPath = ReportDir / TEXT("PortalStencilIdentityValidation.json");
		FFileHelper::SaveStringToFile(Json, *ReportPath);
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilIdentity: report written to %s SetupView=%llu Writes=%llu Overlays=%llu VisibleApertures=%d Targetable=%d AfterDOFEnabled=%d Isolated=%d OverlayPreExposure=%.9g"),
			*ReportPath,
			GSetupViewFrames.Load(),
			GStencilWriteFrames.Load(),
			GStencilOverlayFrames.Load(),
			GLastApertureCount.Load(),
			GLastStencilTargetable.Load() ? 1 : 0,
			GLastAfterDOFPassEnabled.Load() ? 1 : 0,
			GLastBitIsolated.Load() ? 1 : 0,
			GLastOverlayPreExposure.Load());
	}

	void StartStencilIdentityValidation()
	{
		UWorld* LocalWorld = FindPlayableWorld();
		if (!LocalWorld)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalStencilIdentity: no PIE/Game world found. Enter PIE first."));
			return;
		}
		if (GStencilIdentityExtension.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalStencilIdentity: already running."));
			return;
		}

		ResetCounters();
		CVarStencilIdentityValidation->Set(1, ECVF_SetByCode);
		GStencilIdentityExtension =
			FSceneViewExtensions::NewExtension<FPortalStencilIdentityExtension>(LocalWorld);
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilIdentity: started. Main stencil feasibility bit=0x%02x DebugOverlay=%d IsolateBit=%d. The debug overlay is pre-exposure aware and only visible linked apertures are marked."),
			PortalStencilBit,
			CVarStencilIdentityDebug.GetValueOnGameThread(),
			CVarStencilIdentityIsolateBit.GetValueOnGameThread());
	}

	void DumpStencilIdentityValidation()
	{
		WriteReport(GStencilIdentityExtension.IsValid() ? TEXT("RUNNING") : TEXT("STOPPED"));
	}

	void StopStencilIdentityValidation()
	{
		if (!GStencilIdentityExtension.IsValid())
		{
			CVarStencilIdentityValidation->Set(0, ECVF_SetByCode);
			UE_LOG(LogTemp, Display, TEXT("PortalStencilIdentity: not running."));
			return;
		}

		WriteReport(TEXT("STOPPED"));
		CVarStencilIdentityValidation->Set(0, ECVF_SetByCode);
		FlushRenderingCommands();
		GStencilIdentityExtension.Reset();
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilIdentity: stopped. Writes=%llu Overlays=%llu VisibleApertures=%d Targetable=%d Isolated=%d."),
			GStencilWriteFrames.Load(),
			GStencilOverlayFrames.Load(),
			GLastApertureCount.Load(),
			GLastStencilTargetable.Load() ? 1 : 0,
			GLastBitIsolated.Load() ? 1 : 0);
	}

	FAutoConsoleCommand GStartStencilIdentityValidationCommand(
		TEXT("portal.StartStencilIdentityValidation"),
		TEXT("STEP 1B.12D-A: isolate stencil bit 0x40 for the bounded proof, write visible linked portal apertures, and optionally visualize the real stencil test as pre-exposure-correct cyan AfterDOF."),
		FConsoleCommandDelegate::CreateStatic(&StartStencilIdentityValidation));

	FAutoConsoleCommand GDumpStencilIdentityValidationCommand(
		TEXT("portal.DumpStencilIdentityValidation"),
		TEXT("Write STEP 1B.12D-A stencil identity telemetry to Saved/AutomationReports/PortalStencilIdentityValidation.json."),
		FConsoleCommandDelegate::CreateStatic(&DumpStencilIdentityValidation));

	FAutoConsoleCommand GStopStencilIdentityValidationCommand(
		TEXT("portal.StopStencilIdentityValidation"),
		TEXT("Stop STEP 1B.12D-A main-stencil aperture identity validation and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopStencilIdentityValidation));
}
