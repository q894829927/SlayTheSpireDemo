#include "InteriorPortalRenderer.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RHIStaticStates.h"
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "UnrealClient.h"

namespace
{
	constexpr uint8 PortalCompositionStencilBit = 0x40;

	TAutoConsoleVariable<int32> CVarPortalCompositionDiagnostics(
		TEXT("portal.CompositionDiagnostics"),
		0,
		TEXT("Emit public portal composition subscription/execution diagnostics. 0=off, 1=on."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPortalCompositionDebugMode(
		TEXT("portal.CompositionDebugMode"),
		0,
		TEXT("Portal composition diagnostic. 0=normal, 1=solid magenta aperture, 2=full-screen magenta, 3=BaseColor CRP, 4=main-depth visibility (green=open, red=foreground occluded), 5=secondary-depth remap (cyan=valid behind portal, magenta=invalid ordering, yellow=no remote depth), 6=STEP 1B.12C-A main-depth write verification (cyan=propagated, green=foreground preserved, yellow=no remote depth, red=write mismatch)."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalPreExposureRebase(
		TEXT("portal.PreExposureRebase"),
		0,
		TEXT("STEP 1B.8 diagnostic. Rebase portal HDR from the secondary pre-exposed SceneColor domain into the main view domain before BeforeDOF composition."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarPortalSecondaryPreExposure(
		TEXT("portal.SecondaryPreExposure"),
		1.0f,
		TEXT("STEP 1B.8 measured pre-exposure of the standalone transformed secondary view. Set by the spike, not an artistic brightness control."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalProjectiveAperture(
		TEXT("portal.ProjectiveAperture"),
		1,
		TEXT("STEP 1B.11A aperture hardening. 1=map main-view pixels back to the portal plane with the exact inverse homography; 0=retained axis-aligned bounds-ellipse comparison path."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalDepthAwareComposition(
		TEXT("portal.DepthAwareComposition"),
		0,
		TEXT("STEP 1B.12A bounded depth-continuity spike. 1=preserve main SceneColor where main SceneDepth is in front of the physical entry portal plane; 0=retained RGB-only composition."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarPortalDepthOcclusionEpsilonCm(
		TEXT("portal.DepthOcclusionEpsilonCm"),
		2.0f,
		TEXT("STEP 1B.12A/1B.12B/1B.12C world-space depth comparison tolerance in cm. This is a coplanar/ordering tolerance, not an artistic portal-depth offset."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalSecondaryDepthRemap(
		TEXT("portal.SecondaryDepthRemap"),
		0,
		TEXT("STEP 1B.12B proof switch. 1=consume the transported secondary R32F device depth and validate its exact main-view depth equivalence."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalMainDepthPropagation(
		TEXT("portal.MainDepthPropagation"),
		0,
		TEXT("STEP 1B.12C-A feasibility switch. 1=write validated transported remote depth into the current main SceneDepth inside the projective aperture after preserving real foreground occluders. 0=do not mutate main SceneDepth."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalStencilGatedComposition(
		TEXT("portal.StencilGatedComposition"),
		0,
		TEXT("STEP 1B.12D-B bounded proof. 1=clear/mark main stencil bit 0x40 and gate normal BeforeDOF portal composition with a real CF_Equal test in the same RDG chain."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalStencilCompositionBypassShaderAperture(
		TEXT("portal.StencilCompositionBypassShaderAperture"),
		0,
		TEXT("STEP 1B.12D-B proof switch. Effective only when portal.StencilGatedComposition=1. 1=skip portal HDR/depth sampling and emit an exposure-safe cyan tint from main SceneColor; only the hardware stencil test may confine the draw."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalBoundedMainPassScissor(
		TEXT("portal.BoundedMainPassScissor"),
		0,
		TEXT("STEP 1B.13A bounded-work proof. 1=restrict portal candidate/depth-write/stencil-mark/color passes to the conservative projected portal rectangle; 0=retain full main-view raster rectangles."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPortalBoundedMainPassPaddingPixels(
		TEXT("portal.BoundedMainPassPaddingPixels"),
		4,
		TEXT("STEP 1B.13A extra main-render pixels around the conservative projected portal rectangle. Clamped to [0,64]."),
		ECVF_RenderThreadSafe);

	bool PortalCompositionDiagnosticsEnabled()
	{
		return CVarPortalCompositionDiagnostics.GetValueOnAnyThread() != 0;
	}

	FIntRect BuildBoundedPortalPassRect(
		const InteriorPortalMath::FPortalScreenBounds& Bounds,
		const FIntRect& ViewRect,
		const int32 PaddingPixels)
	{
		if (!Bounds.bHasVisiblePortion || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
		{
			return ViewRect;
		}

		const float MinU = FMath::Clamp(Bounds.Min.X, 0.0f, 1.0f);
		const float MinV = FMath::Clamp(Bounds.Min.Y, 0.0f, 1.0f);
		const float MaxU = FMath::Clamp(Bounds.Max.X, 0.0f, 1.0f);
		const float MaxV = FMath::Clamp(Bounds.Max.Y, 0.0f, 1.0f);
		const int32 SafePadding = FMath::Clamp(PaddingPixels, 0, 64);

		FIntRect Result(
			ViewRect.Min.X + FMath::FloorToInt(MinU * ViewRect.Width()) - SafePadding,
			ViewRect.Min.Y + FMath::FloorToInt(MinV * ViewRect.Height()) - SafePadding,
			ViewRect.Min.X + FMath::CeilToInt(MaxU * ViewRect.Width()) + SafePadding,
			ViewRect.Min.Y + FMath::CeilToInt(MaxV * ViewRect.Height()) + SafePadding);

		Result.Min.X = FMath::Clamp(Result.Min.X, ViewRect.Min.X, ViewRect.Max.X);
		Result.Min.Y = FMath::Clamp(Result.Min.Y, ViewRect.Min.Y, ViewRect.Max.Y);
		Result.Max.X = FMath::Clamp(Result.Max.X, ViewRect.Min.X, ViewRect.Max.X);
		Result.Max.Y = FMath::Clamp(Result.Max.Y, ViewRect.Min.Y, ViewRect.Max.Y);
		if (Result.Width() <= 0 || Result.Height() <= 0)
		{
			return ViewRect;
		}
		return Result;
	}

	FCustomRenderPassBase::ERenderOutput PortalCustomRenderOutput()
	{
		return CVarPortalCompositionDebugMode.GetValueOnAnyThread() == 3
			? FCustomRenderPassBase::ERenderOutput::BaseColor
			: FCustomRenderPassBase::ERenderOutput::SceneColorNoAlpha;
	}
}

namespace InteriorPortalRenderer
{
	BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalCompositionParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PortalTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PortalSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MainSceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SecondaryDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SecondaryDepthSampler)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Portal)
		SHADER_PARAMETER(FVector4f, PortalBounds)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow0)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow1)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow2)
		SHADER_PARAMETER(FVector4f, PortalClipZRow)
		SHADER_PARAMETER(float, ProjectiveApertureEnabled)
		SHADER_PARAMETER(float, ProjectiveNearClipW)
		SHADER_PARAMETER(float, DepthAwareCompositionEnabled)
		SHADER_PARAMETER(float, SecondaryDepthRemapEnabled)
		SHADER_PARAMETER(float, MainDepthPropagationEnabled)
		SHADER_PARAMETER(float, DepthOcclusionEpsilonCm)
		SHADER_PARAMETER(float, CompositionDebugMode)
		SHADER_PARAMETER(float, PortalExposureScale)
		SHADER_PARAMETER(float, StencilBypassShaderAperture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FInteriorPortalCompositionPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FInteriorPortalCompositionPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FInteriorPortalCompositionPS, FGlobalShader);
		using FParameters = FInteriorPortalCompositionParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FInteriorPortalCompositionPS,
		TEXT("/Project/InteriorPortalComposition.usf"), TEXT("MainPS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalDepthCandidateParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MainSceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SecondaryDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SecondaryDepthSampler)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Portal)
		SHADER_PARAMETER(FVector4f, PortalBounds)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow0)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow1)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow2)
		SHADER_PARAMETER(FVector4f, PortalClipZRow)
		SHADER_PARAMETER(float, ProjectiveNearClipW)
		SHADER_PARAMETER(float, DepthOcclusionEpsilonCm)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FInteriorPortalDepthCandidatePS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FInteriorPortalDepthCandidatePS, Global);
		SHADER_USE_PARAMETER_STRUCT(FInteriorPortalDepthCandidatePS, FGlobalShader);
		using FParameters = FInteriorPortalDepthCandidateParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FInteriorPortalDepthCandidatePS,
		TEXT("/Project/InteriorPortalDepthPropagation.usf"), TEXT("BuildCandidatePS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalDepthWriteParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PropagatedDepthTexture)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, DepthOutput)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FInteriorPortalDepthWritePS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FInteriorPortalDepthWritePS, Global);
		SHADER_USE_PARAMETER_STRUCT(FInteriorPortalDepthWritePS, FGlobalShader);
		using FParameters = FInteriorPortalDepthWriteParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FInteriorPortalDepthWritePS,
		TEXT("/Project/InteriorPortalDepthPropagation.usf"), TEXT("WriteDepthPS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalStencilClearParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FInteriorPortalStencilClearPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FInteriorPortalStencilClearPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FInteriorPortalStencilClearPS, FGlobalShader);
		using FParameters = FInteriorPortalStencilClearParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FInteriorPortalStencilClearPS,
		TEXT("/Project/InteriorPortalStencilComposition.usf"), TEXT("ClearStencilPS"), SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalStencilMarkParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow0)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow1)
		SHADER_PARAMETER(FVector4f, ScreenToPortalRow2)
		SHADER_PARAMETER(float, ProjectiveNearClipW)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FInteriorPortalStencilMarkPS : public FGlobalShader
	{
	public:
		DECLARE_SHADER_TYPE(FInteriorPortalStencilMarkPS, Global);
		SHADER_USE_PARAMETER_STRUCT(FInteriorPortalStencilMarkPS, FGlobalShader);
		using FParameters = FInteriorPortalStencilMarkParameters;
	};

	IMPLEMENT_SHADER_TYPE(, FInteriorPortalStencilMarkPS,
		TEXT("/Project/InteriorPortalStencilComposition.usf"), TEXT("MarkAperturePS"), SF_Pixel);
}

FInteriorPortalCustomRenderPass::FInteriorPortalCustomRenderPass(
	const FString& InDebugName, FRenderTarget* InRenderTarget, const FIntPoint& InRenderTargetSize)
	: FCustomRenderPassBase(
		InDebugName,
		FCustomRenderPassBase::ERenderMode::DepthAndBasePass,
		PortalCustomRenderOutput(),
		InRenderTargetSize)
	, RenderTargetResource(InRenderTarget)
{
	bSceneColorWithTranslucent = PortalCustomRenderOutput()
		== FCustomRenderPassBase::ERenderOutput::SceneColorNoAlpha;
}

void FInteriorPortalCustomRenderPass::OnPreRender(FRDGBuilder& GraphBuilder)
{
	if (!RenderTargetResource)
	{
		return;
	}

	RenderTargetTexture = RenderTargetResource->GetRenderTargetTexture(GraphBuilder);
	if (RenderTargetTexture)
	{
		GraphBuilder.UseInternalAccessMode(RenderTargetTexture);
	}
}

void FInteriorPortalCustomRenderPass::OnEndPass(FRDGBuilder& GraphBuilder)
{
	if (RenderTargetTexture)
	{
		GraphBuilder.UseExternalAccessMode(RenderTargetTexture, ERHIAccess::SRVMask);
	}
}

FInteriorPortalViewExtension::FInteriorPortalViewExtension(const FAutoRegister& AutoRegister,
	UWorld* InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
{
}

void FInteriorPortalViewExtension::SetEnabled(const bool bInEnabled)
{
	bEnabled = bInEnabled;
	if (!bEnabled)
	{
		ClearRequest();
	}
}

void FInteriorPortalViewExtension::PublishRequest(const FInteriorPortalRenderRequest& InRequest)
{
	FScopeLock Lock(&RequestMutex);
	PublishedRequest = InRequest;
}

void FInteriorPortalViewExtension::ClearRequest()
{
	FScopeLock Lock(&RequestMutex);
	PublishedRequest.Reset();
}

bool FInteriorPortalViewExtension::HasPublishedRequest() const
{
	FScopeLock Lock(&RequestMutex);
	return PublishedRequest.IsSet();
}

FInteriorPortalRenderRequest FInteriorPortalViewExtension::GetPublishedRequest() const
{
	FScopeLock Lock(&RequestMutex);
	return PublishedRequest.IsSet() ? PublishedRequest.GetValue() : FInteriorPortalRenderRequest();
}

bool FInteriorPortalViewExtension::IsActiveThisFrame_Internal(
	const FSceneViewExtensionContext& Context) const
{
	return bEnabled && FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
}

void FInteriorPortalViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	(void)InViewFamily;
}

void FInteriorPortalViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	(void)GraphBuilder;
	(void)InViewFamily;
}

void FInteriorPortalViewExtension::PostRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	(void)GraphBuilder;
	(void)InViewFamily;
}

void FInteriorPortalViewExtension::SubscribeToPostProcessingPass(
	const ISceneViewExtension::EPostProcessingPass Pass,
	const FSceneView& InView,
	FPostProcessingPassDelegateArray& InOutPassCallbacks,
	const bool bIsPassEnabled)
{
	if (Pass != ISceneViewExtension::EPostProcessingPass::BeforeDOF)
	{
		return;
	}

	if (InView.Family && InView.Family->bAdditionalViewFamily)
	{
		return;
	}

	const FInteriorPortalRenderRequest Request = GetPublishedRequest();
	if (PortalCompositionDiagnosticsEnabled())
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition Subscribe Frame=%llu Enabled=%d BeforeDOFEnabled=%d RequestValid=%d HasTarget=%d HasDepthTarget=%d PortalId=%d Endpoint=%d"),
			GFrameCounter,
			bEnabled ? 1 : 0,
			bIsPassEnabled ? 1 : 0,
			Request.IsValid() ? 1 : 0,
			Request.PortalRenderTarget ? 1 : 0,
			Request.PortalDepthRenderTarget ? 1 : 0,
			Request.PortalId,
			Request.EndpointIndex);
	}

	if (!bEnabled)
	{
		return;
	}
	if (!Request.IsValid() || !Request.PortalRenderTarget)
	{
		if (PortalCompositionDiagnosticsEnabled())
		{
			UE_LOG(LogTemp, Display,
				TEXT("PortalComposition Skip Frame=%llu Reason=InvalidRequestOrTarget"),
				GFrameCounter);
		}
		return;
	}

	InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateLambda(
		[Request](FRDGBuilder& GraphBuilder, const FSceneView& View,
			const FPostProcessMaterialInputs& Inputs)
		{
			if (PortalCompositionDiagnosticsEnabled())
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalComposition Execute Frame=%llu PortalId=%d Endpoint=%d Bounds=(%.4f,%.4f)-(%.4f,%.4f)"),
					GFrameCounter,
					Request.PortalId,
					Request.EndpointIndex,
					Request.ProjectedBounds.Min.X,
					Request.ProjectedBounds.Min.Y,
					Request.ProjectedBounds.Max.X,
					Request.ProjectedBounds.Max.Y);
			}
			return FInteriorPortalViewExtension::ComposePortalIntoSceneColor(
				GraphBuilder, View, Inputs, Request);
		}));
}

FScreenPassTexture FInteriorPortalViewExtension::ComposePortalIntoSceneColor(
	FRDGBuilder& GraphBuilder, const FSceneView& InView,
	const FPostProcessMaterialInputs& Inputs,
	const FInteriorPortalRenderRequest& Request)
{
	const FScreenPassTextureSlice SceneColorSlice =
		Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneColorSlice);
	if (!SceneColor.IsValid() || !Request.PortalRenderTarget)
	{
		if (PortalCompositionDiagnosticsEnabled())
		{
			UE_LOG(LogTemp, Display,
				TEXT("PortalComposition ComposeSkip Frame=%llu SceneColorValid=%d HasTarget=%d"),
				GFrameCounter,
				SceneColor.IsValid() ? 1 : 0,
				Request.PortalRenderTarget ? 1 : 0);
		}
		return SceneColor;
	}

	FRDGTextureRef PortalTexture = Request.PortalRenderTarget->GetRenderTargetTexture(GraphBuilder);
	if (!PortalTexture)
	{
		if (PortalCompositionDiagnosticsEnabled())
		{
			UE_LOG(LogTemp, Display,
				TEXT("PortalComposition ComposeSkip Frame=%llu Reason=PortalTextureUnavailable"),
				GFrameCounter);
		}
		return SceneColor;
	}
	GraphBuilder.UseInternalAccessMode(PortalTexture);

	const FScreenPassRenderTarget Output = FScreenPassRenderTarget::CreateFromInput(
		GraphBuilder, SceneColor, ERenderTargetLoadAction::ELoad,
		TEXT("InteriorPortalBeforeDOFComposition"));
	const FScreenPassTextureViewport OutputViewport(Output);
	const FScreenPassTextureViewport PortalViewport(PortalTexture);
	const int32 CompositionDebugMode = CVarPortalCompositionDebugMode.GetValueOnRenderThread();

	const bool bRebasePreExposure = CVarPortalPreExposureRebase.GetValueOnRenderThread() != 0;
	const float SecondaryPreExposure = FMath::Max(
		CVarPortalSecondaryPreExposure.GetValueOnRenderThread(), UE_SMALL_NUMBER);
	const float MainPreExposure = InView.State
		? FMath::Max(InView.State->GetPreExposure(), UE_SMALL_NUMBER)
		: 1.0f;
	const float PortalExposureScale = bRebasePreExposure
		? MainPreExposure / SecondaryPreExposure
		: 1.0f;
	const bool bUseProjectiveAperture =
		CVarPortalProjectiveAperture.GetValueOnRenderThread() != 0
		&& Request.ProjectiveAperture.bValid;
	const bool bStencilCompositionRequested =
		CVarPortalStencilGatedComposition.GetValueOnRenderThread() != 0;
	const bool bMainDepthPropagationRequested =
		CVarPortalMainDepthPropagation.GetValueOnRenderThread() != 0
		|| (CompositionDebugMode >= 6 && CompositionDebugMode < 7);
	const bool bDepthAwareRequested =
		CVarPortalDepthAwareComposition.GetValueOnRenderThread() != 0
		|| (CompositionDebugMode >= 4 && CompositionDebugMode < 5)
		|| bMainDepthPropagationRequested;
	const bool bSecondaryDepthRequested =
		CVarPortalSecondaryDepthRemap.GetValueOnRenderThread() != 0
		|| (CompositionDebugMode >= 5 && CompositionDebugMode < 6)
		|| bMainDepthPropagationRequested;
	const float DepthOcclusionEpsilonCm = FMath::Max(
		CVarPortalDepthOcclusionEpsilonCm.GetValueOnRenderThread(), 0.0f);

	FRDGTextureRef MainSceneDepthTexture = SceneColor.Texture;
	bool bMainSceneDepthValid = false;
	if (bDepthAwareRequested || bStencilCompositionRequested)
	{
		const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextureUniformBuffer =
			CreateSceneTextureUniformBuffer(
				GraphBuilder, InView, ESceneTextureSetupMode::SceneDepth);
		if (SceneTextureUniformBuffer)
		{
			const FSceneTextureUniformParameters* SceneTextureContents =
				SceneTextureUniformBuffer->GetContents();
			if (SceneTextureContents && SceneTextureContents->SceneDepthTexture)
			{
				MainSceneDepthTexture = SceneTextureContents->SceneDepthTexture;
				bMainSceneDepthValid = true;
			}
		}
	}

	FRDGTextureRef SecondaryDepthTexture = SceneColor.Texture;
	bool bSecondaryDepthTextureValid = false;
	if (Request.PortalDepthRenderTarget)
	{
		FRDGTextureRef CandidateDepthTexture =
			Request.PortalDepthRenderTarget->GetRenderTargetTexture(GraphBuilder);
		if (CandidateDepthTexture)
		{
			SecondaryDepthTexture = CandidateDepthTexture;
			bSecondaryDepthTextureValid = true;
			GraphBuilder.UseInternalAccessMode(SecondaryDepthTexture);
		}
	}

	const bool bUseDepthAwareComposition =
		bDepthAwareRequested && bUseProjectiveAperture && bMainSceneDepthValid;
	const bool bUseSecondaryDepthRemap =
		bSecondaryDepthRequested && bUseProjectiveAperture && bSecondaryDepthTextureValid;
	const bool bMainDepthTargetable =
		bMainSceneDepthValid
		&& EnumHasAnyFlags(MainSceneDepthTexture->Desc.Flags, TexCreate_DepthStencilTargetable);
	const bool bMainDepthStencilTargetable =
		bMainDepthTargetable && MainSceneDepthTexture->Desc.Format == PF_DepthStencil;
	const bool bUseStencilComposition =
		bStencilCompositionRequested
		&& bUseProjectiveAperture
		&& bMainDepthStencilTargetable;
	const bool bBypassShaderAperture =
		bUseStencilComposition
		&& CVarPortalStencilCompositionBypassShaderAperture.GetValueOnRenderThread() != 0;
	const bool bUseMainDepthPropagation =
		bMainDepthPropagationRequested
		&& bUseProjectiveAperture
		&& bMainSceneDepthValid
		&& bSecondaryDepthTextureValid
		&& bMainDepthTargetable;
	const bool bBoundedMainPassRequested =
		CVarPortalBoundedMainPassScissor.GetValueOnRenderThread() != 0;
	const int32 BoundedMainPassPaddingPixels = FMath::Clamp(
		CVarPortalBoundedMainPassPaddingPixels.GetValueOnRenderThread(), 0, 64);
	const bool bFullScreenDebug = CompositionDebugMode >= 1.5 && CompositionDebugMode < 2.5;
	// The stencil bypass proof must remain full-view so it cannot be accidentally
	// "proven" by the CPU rectangle. Only the real CF_Equal test may confine it.
	const bool bUseBoundedMainPassScissor =
		bBoundedMainPassRequested
		&& bUseProjectiveAperture
		&& !bBypassShaderAperture
		&& !bFullScreenDebug;
	const FIntRect PortalPassRect = bUseBoundedMainPassScissor
		? BuildBoundedPortalPassRect(
			Request.ProjectedBounds, SceneColor.ViewRect, BoundedMainPassPaddingPixels)
		: SceneColor.ViewRect;
	const FScreenPassTextureViewport OutputPassViewport(Output.Texture, PortalPassRect);
	const FScreenPassTextureViewport SceneColorPassViewport(SceneColor.Texture, PortalPassRect);
	const int64 FullMainPassPixels = int64(SceneColor.ViewRect.Width()) * int64(SceneColor.ViewRect.Height());
	const int64 BoundedMainPassPixels = int64(PortalPassRect.Width()) * int64(PortalPassRect.Height());
	const double BoundedMainPassCoverage = FullMainPassPixels > 0
		? double(BoundedMainPassPixels) / double(FullMainPassPixels)
		: 1.0;
	const bool bSparseOutput = bUseStencilComposition || bUseBoundedMainPassScissor;
	const bool bOutputPrefilled = bSparseOutput && Output.Texture != SceneColor.Texture;
	if (bOutputPrefilled)
	{
		// Sparse raster work must preserve the incoming main color outside the
		// portal rectangle / stencil coverage. CreateFromInput allocates but does
		// not populate the new post-process output texture.
		AddCopyTexturePass(GraphBuilder, SceneColor.Texture, Output.Texture);
	}

	FRDGTextureRef PropagatedDepthCandidateTexture = nullptr;
	if (bUseMainDepthPropagation)
	{
		const FRDGTextureDesc CandidateDesc = FRDGTextureDesc::Create2D(
			SceneColor.Texture->Desc.Extent,
			PF_R32_FLOAT,
			FClearValueBinding(FLinearColor::Black),
			TexCreate_RenderTargetable | TexCreate_ShaderResource,
			1,
			1,
			0);
		PropagatedDepthCandidateTexture = GraphBuilder.CreateTexture(
			CandidateDesc, TEXT("InteriorPortal.MainDepthPropagationCandidate"));

		const FScreenPassRenderTarget CandidateOutput(
			PropagatedDepthCandidateTexture,
			SceneColor.ViewRect,
			ERenderTargetLoadAction::EClear);
		const FScreenPassTextureViewport CandidateViewport(CandidateOutput);
		const FScreenPassTextureViewport CandidatePassViewport(
			PropagatedDepthCandidateTexture, PortalPassRect);

		InteriorPortalRenderer::FInteriorPortalDepthCandidateParameters* CandidateParameters =
			GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalDepthCandidateParameters>();
		CandidateParameters->View = InView.ViewUniformBuffer;
		CandidateParameters->MainSceneDepthTexture = MainSceneDepthTexture;
		CandidateParameters->SecondaryDepthTexture = SecondaryDepthTexture;
		CandidateParameters->SecondaryDepthSampler =
			TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		CandidateParameters->Output = GetScreenPassTextureViewportParameters(CandidateViewport);
		CandidateParameters->Portal = GetScreenPassTextureViewportParameters(PortalViewport);
		CandidateParameters->PortalBounds = FVector4f(
			Request.ProjectedBounds.Min.X, Request.ProjectedBounds.Min.Y,
			Request.ProjectedBounds.Max.X, Request.ProjectedBounds.Max.Y);
		CandidateParameters->ScreenToPortalRow0 = Request.ProjectiveAperture.Row0;
		CandidateParameters->ScreenToPortalRow1 = Request.ProjectiveAperture.Row1;
		CandidateParameters->ScreenToPortalRow2 = Request.ProjectiveAperture.Row2;
		CandidateParameters->PortalClipZRow = Request.ProjectiveAperture.ClipZRow;
		CandidateParameters->ProjectiveNearClipW = Request.ProjectiveNearClipW;
		CandidateParameters->DepthOcclusionEpsilonCm = DepthOcclusionEpsilonCm;
		CandidateParameters->RenderTargets[0] = CandidateOutput.GetRenderTargetBinding();

		TShaderMapRef<InteriorPortalRenderer::FInteriorPortalDepthCandidatePS> CandidatePixelShader(
			GetGlobalShaderMap(InView.GetFeatureLevel()));
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::BuildMainDepthPropagationCandidate"),
			InView,
			CandidatePassViewport,
			SceneColorPassViewport,
			CandidatePixelShader,
			CandidateParameters,
			EScreenPassDrawFlags::None);

		const FScreenPassTextureViewport MainDepthViewport(
			MainSceneDepthTexture, SceneColor.ViewRect);
		const FScreenPassTextureViewport MainDepthPassViewport(
			MainSceneDepthTexture, PortalPassRect);
		InteriorPortalRenderer::FInteriorPortalDepthWriteParameters* DepthWriteParameters =
			GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalDepthWriteParameters>();
		DepthWriteParameters->PropagatedDepthTexture = PropagatedDepthCandidateTexture;
		DepthWriteParameters->DepthOutput = GetScreenPassTextureViewportParameters(MainDepthViewport);
		DepthWriteParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			MainSceneDepthTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilNop);

		TShaderMapRef<FScreenPassVS> VertexShader(GetGlobalShaderMap(InView.GetFeatureLevel()));
		TShaderMapRef<InteriorPortalRenderer::FInteriorPortalDepthWritePS> DepthWritePixelShader(
			GetGlobalShaderMap(InView.GetFeatureLevel()));
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::WriteMainSceneDepth"),
			InView,
			MainDepthPassViewport,
			CandidatePassViewport,
			VertexShader,
			DepthWritePixelShader,
			TStaticBlendState<>::GetRHI(),
			TStaticDepthStencilState<true, CF_Always>::GetRHI(),
			DepthWriteParameters,
			EScreenPassDrawFlags::None);
	}

	if (bUseStencilComposition)
	{
		const FScreenPassTextureViewport MainDepthViewport(
			MainSceneDepthTexture, SceneColor.ViewRect);
		const FScreenPassTextureViewport MainDepthPassViewport(
			MainSceneDepthTexture, PortalPassRect);
		TShaderMapRef<FScreenPassVS> VertexShader(GetGlobalShaderMap(InView.GetFeatureLevel()));

		InteriorPortalRenderer::FInteriorPortalStencilClearParameters* ClearParameters =
			GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalStencilClearParameters>();
		ClearParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			MainSceneDepthTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthNop_StencilWrite);
		TShaderMapRef<InteriorPortalRenderer::FInteriorPortalStencilClearPS> ClearPixelShader(
			GetGlobalShaderMap(InView.GetFeatureLevel()));
		FRHIDepthStencilState* ClearStencilState =
			TStaticDepthStencilState<
				false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				PortalCompositionStencilBit, PortalCompositionStencilBit>::GetRHI();
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::ClearCompositionStencilBit"),
			InView,
			MainDepthViewport,
			MainDepthViewport,
			FScreenPassPipelineState(
				VertexShader, ClearPixelShader,
				TStaticBlendState<>::GetRHI(), ClearStencilState,
				0),
			ClearParameters,
			EScreenPassDrawFlags::None,
			[ClearPixelShader, ClearParameters](FRHICommandList& RHICmdList)
			{
				SetShaderParameters(
					RHICmdList, ClearPixelShader, ClearPixelShader.GetPixelShader(), *ClearParameters);
				RHICmdList.SetStencilRef(0);
			});

		InteriorPortalRenderer::FInteriorPortalStencilMarkParameters* MarkParameters =
			GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalStencilMarkParameters>();
		MarkParameters->Output = GetScreenPassTextureViewportParameters(MainDepthViewport);
		MarkParameters->ScreenToPortalRow0 = Request.ProjectiveAperture.Row0;
		MarkParameters->ScreenToPortalRow1 = Request.ProjectiveAperture.Row1;
		MarkParameters->ScreenToPortalRow2 = Request.ProjectiveAperture.Row2;
		MarkParameters->ProjectiveNearClipW = Request.ProjectiveNearClipW;
		MarkParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			MainSceneDepthTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthNop_StencilWrite);
		TShaderMapRef<InteriorPortalRenderer::FInteriorPortalStencilMarkPS> MarkPixelShader(
			GetGlobalShaderMap(InView.GetFeatureLevel()));
		FRHIDepthStencilState* MarkStencilState =
			TStaticDepthStencilState<
				false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				PortalCompositionStencilBit, PortalCompositionStencilBit>::GetRHI();
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::MarkCompositionStencil Endpoint=%d", Request.EndpointIndex),
			InView,
			MainDepthPassViewport,
			MainDepthPassViewport,
			FScreenPassPipelineState(
				VertexShader, MarkPixelShader,
				TStaticBlendState<>::GetRHI(), MarkStencilState,
				PortalCompositionStencilBit),
			MarkParameters,
			EScreenPassDrawFlags::None,
			[MarkPixelShader, MarkParameters](FRHICommandList& RHICmdList)
			{
				SetShaderParameters(
					RHICmdList, MarkPixelShader, MarkPixelShader.GetPixelShader(), *MarkParameters);
				RHICmdList.SetStencilRef(PortalCompositionStencilBit);
			});
	}

	if (PortalCompositionDiagnosticsEnabled())
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition ComposeReady Frame=%llu SceneRect=%dx%d PortalExtent=%dx%d DebugMode=%d Rebase=%d MainPreExposure=%.9g SecondaryPreExposure=%.9g ExposureScale=%.9g Projective=%d ProjectiveValid=%d ProjectiveQuality=%.9g NearClipW=%.9g NearClip=%d Crossing=%d ViewportClip=%d DepthAware=%d DepthRequested=%d DepthTextureValid=%d SecondaryDepthRequested=%d SecondaryDepthTextureValid=%d SecondaryDepthRemap=%d SecondaryDepthExtent=%dx%d MainDepthPropagationRequested=%d MainDepthTargetable=%d MainDepthPropagation=%d CandidateExtent=%dx%d DepthEpsilonCm=%.4f"),
			GFrameCounter,
			SceneColor.ViewRect.Width(),
			SceneColor.ViewRect.Height(),
			PortalTexture->Desc.Extent.X,
			PortalTexture->Desc.Extent.Y,
			CompositionDebugMode,
			bRebasePreExposure ? 1 : 0,
			MainPreExposure,
			SecondaryPreExposure,
			PortalExposureScale,
			bUseProjectiveAperture ? 1 : 0,
			Request.ProjectiveAperture.bValid ? 1 : 0,
			Request.ProjectiveAperture.DeterminantQuality,
			Request.ProjectiveNearClipW,
			Request.ProjectedBounds.bIntersectsNearClip ? 1 : 0,
			Request.ProjectedBounds.bCameraCrossing ? 1 : 0,
			Request.ProjectedBounds.bClippedToViewport ? 1 : 0,
			bUseDepthAwareComposition ? 1 : 0,
			bDepthAwareRequested ? 1 : 0,
			bMainSceneDepthValid ? 1 : 0,
			bSecondaryDepthRequested ? 1 : 0,
			bSecondaryDepthTextureValid ? 1 : 0,
			bUseSecondaryDepthRemap ? 1 : 0,
			SecondaryDepthTexture->Desc.Extent.X,
			SecondaryDepthTexture->Desc.Extent.Y,
			bMainDepthPropagationRequested ? 1 : 0,
			bMainDepthTargetable ? 1 : 0,
			bUseMainDepthPropagation ? 1 : 0,
			PropagatedDepthCandidateTexture ? PropagatedDepthCandidateTexture->Desc.Extent.X : 0,
			PropagatedDepthCandidateTexture ? PropagatedDepthCandidateTexture->Desc.Extent.Y : 0,
			DepthOcclusionEpsilonCm);
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition StencilGate Frame=%llu Requested=%d Active=%d StencilTargetable=%d Format=%d StencilBit=0x%02x PipelineStencilRef=0x%02x BypassShaderAperture=%d ProofMode=%s OutputPrefilled=%d"),
			GFrameCounter,
			bStencilCompositionRequested ? 1 : 0,
			bUseStencilComposition ? 1 : 0,
			bMainDepthStencilTargetable ? 1 : 0,
			bMainSceneDepthValid ? int32(MainSceneDepthTexture->Desc.Format) : -1,
			PortalCompositionStencilBit,
			bUseStencilComposition ? PortalCompositionStencilBit : 0,
			bBypassShaderAperture ? 1 : 0,
			bBypassShaderAperture ? TEXT("SafeMainColorTint") : TEXT("NormalPortalRGB"),
			bOutputPrefilled ? 1 : 0);
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition BoundedPass Frame=%llu Requested=%d Active=%d Rect=(%d,%d)-(%d,%d) Pixels=%lld/%lld Coverage=%.6f Padding=%d"),
			GFrameCounter,
			bBoundedMainPassRequested ? 1 : 0,
			bUseBoundedMainPassScissor ? 1 : 0,
			PortalPassRect.Min.X, PortalPassRect.Min.Y,
			PortalPassRect.Max.X, PortalPassRect.Max.Y,
			BoundedMainPassPixels, FullMainPassPixels,
			BoundedMainPassCoverage,
			BoundedMainPassPaddingPixels);
	}

	InteriorPortalRenderer::FInteriorPortalCompositionParameters* PassParameters =
		GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalCompositionParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneColorTexture = SceneColor.Texture;
	PassParameters->SceneColorSampler =
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PortalTexture = PortalTexture;
	PassParameters->PortalSampler =
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->MainSceneDepthTexture = MainSceneDepthTexture;
	PassParameters->SecondaryDepthTexture = SecondaryDepthTexture;
	PassParameters->SecondaryDepthSampler =
		TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
	PassParameters->Portal = GetScreenPassTextureViewportParameters(PortalViewport);
	PassParameters->PortalBounds = FVector4f(
		Request.ProjectedBounds.Min.X, Request.ProjectedBounds.Min.Y,
		Request.ProjectedBounds.Max.X, Request.ProjectedBounds.Max.Y);
	PassParameters->ScreenToPortalRow0 = Request.ProjectiveAperture.Row0;
	PassParameters->ScreenToPortalRow1 = Request.ProjectiveAperture.Row1;
	PassParameters->ScreenToPortalRow2 = Request.ProjectiveAperture.Row2;
	PassParameters->PortalClipZRow = Request.ProjectiveAperture.ClipZRow;
	PassParameters->ProjectiveApertureEnabled = bUseProjectiveAperture ? 1.0f : 0.0f;
	PassParameters->ProjectiveNearClipW = Request.ProjectiveNearClipW;
	PassParameters->DepthAwareCompositionEnabled = bUseDepthAwareComposition ? 1.0f : 0.0f;
	PassParameters->SecondaryDepthRemapEnabled = bUseSecondaryDepthRemap ? 1.0f : 0.0f;
	PassParameters->MainDepthPropagationEnabled = bUseMainDepthPropagation ? 1.0f : 0.0f;
	PassParameters->DepthOcclusionEpsilonCm = DepthOcclusionEpsilonCm;
	PassParameters->CompositionDebugMode = float(CompositionDebugMode);
	PassParameters->PortalExposureScale = PortalExposureScale;
	PassParameters->StencilBypassShaderAperture = bBypassShaderAperture ? 1.0f : 0.0f;
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	TShaderMapRef<InteriorPortalRenderer::FInteriorPortalCompositionPS> PixelShader(
		GetGlobalShaderMap(InView.GetFeatureLevel()));
	if (bUseStencilComposition)
	{
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			MainSceneDepthTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthNop_StencilRead);
		TShaderMapRef<FScreenPassVS> VertexShader(GetGlobalShaderMap(InView.GetFeatureLevel()));
		FRHIDepthStencilState* StencilTestState =
			TStaticDepthStencilState<
				false, CF_Always,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				PortalCompositionStencilBit, 0x00>::GetRHI();
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::BeforeDOFStencilGatedComposition"),
			InView,
			OutputPassViewport,
			SceneColorPassViewport,
			FScreenPassPipelineState(
				VertexShader, PixelShader,
				TStaticBlendState<>::GetRHI(), StencilTestState,
				PortalCompositionStencilBit),
			PassParameters,
			EScreenPassDrawFlags::None,
			[PixelShader, PassParameters](FRHICommandList& RHICmdList)
			{
				SetShaderParameters(
					RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
				RHICmdList.SetStencilRef(PortalCompositionStencilBit);
			});
	}
	else
	{
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("InteriorPortal::BeforeDOFComposition"),
			InView,
			OutputPassViewport,
			SceneColorPassViewport,
			PixelShader,
			PassParameters);
	}

	if (PortalCompositionDiagnosticsEnabled())
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition DrawQueued Frame=%llu PortalId=%d Endpoint=%d DebugMode=%d MainDepthPropagation=%d StencilGate=%d BypassShaderAperture=%d BoundedScissor=%d"),
			GFrameCounter,
			Request.PortalId,
			Request.EndpointIndex,
			CompositionDebugMode,
			bUseMainDepthPropagation ? 1 : 0,
			bUseStencilComposition ? 1 : 0,
			bBypassShaderAperture ? 1 : 0,
			bUseBoundedMainPassScissor ? 1 : 0);
	}
	return Output;
}
