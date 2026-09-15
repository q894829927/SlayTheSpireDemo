#include "InteriorPortalRenderer.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "RenderGraphBuilder.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneRenderTargetParameters.h"
#include "ScreenPass.h"
#include "UnrealClient.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPortalCompositionDiagnostics(
		TEXT("portal.CompositionDiagnostics"),
		0,
		TEXT("Emit STEP 1B.3 public composition subscription/execution diagnostics. 0=off, 1=on."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPortalCompositionDebugMode(
		TEXT("portal.CompositionDebugMode"),
		0,
		TEXT("Portal composition diagnostic. 0=normal, 1=solid magenta aperture, 2=full-screen magenta, 3=BaseColor CRP, 4=STEP 1B.12A main-depth visibility (green=open, red=foreground occluded)."),
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
		TEXT("STEP 1B.12A world-space depth comparison tolerance in cm. This is a coplanar host-wall/rim tolerance, not an artistic portal-depth offset."),
		ECVF_RenderThreadSafe);

	bool PortalCompositionDiagnosticsEnabled()
	{
		return CVarPortalCompositionDiagnostics.GetValueOnAnyThread() != 0;
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
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PortalTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PortalSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MainSceneDepthTexture)
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
		SHADER_PARAMETER(float, DepthOcclusionEpsilonCm)
		SHADER_PARAMETER(float, CompositionDebugMode)
		SHADER_PARAMETER(float, PortalExposureScale)
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
			TEXT("PortalComposition Subscribe Frame=%llu Enabled=%d BeforeDOFEnabled=%d RequestValid=%d HasTarget=%d PortalId=%d Endpoint=%d"),
			GFrameCounter,
			bEnabled ? 1 : 0,
			bIsPassEnabled ? 1 : 0,
			Request.IsValid() ? 1 : 0,
			Request.PortalRenderTarget ? 1 : 0,
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
	const bool bDepthAwareRequested =
		CVarPortalDepthAwareComposition.GetValueOnRenderThread() != 0
		|| (CompositionDebugMode >= 4 && CompositionDebugMode < 5);
	const float DepthOcclusionEpsilonCm = FMath::Max(
		CVarPortalDepthOcclusionEpsilonCm.GetValueOnRenderThread(), 0.0f);

	// Do not bind the full SceneTextures uniform buffer into this custom screen
	// shader. D3D12 requires every reflected uniform-buffer slot to be populated,
	// and SceneViewExtension callbacks do not guarantee that FSceneTextureShaderParameters
	// contains a bindable deferred buffer. Instead, create the current main view's
	// SceneDepth RDG parameters only when the depth gate is requested, extract the
	// texture from the RDG uniform buffer's contents, and bind that texture directly.
	FRDGTextureRef MainSceneDepthTexture = SceneColor.Texture;
	bool bMainSceneDepthValid = false;
	if (bDepthAwareRequested)
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

	const bool bUseDepthAwareComposition =
		bDepthAwareRequested && bUseProjectiveAperture && bMainSceneDepthValid;

	if (PortalCompositionDiagnosticsEnabled())
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition ComposeReady Frame=%llu SceneRect=%dx%d PortalExtent=%dx%d DebugMode=%d Rebase=%d MainPreExposure=%.9g SecondaryPreExposure=%.9g ExposureScale=%.9g Projective=%d ProjectiveValid=%d ProjectiveQuality=%.9g NearClipW=%.9g NearClip=%d Crossing=%d ViewportClip=%d DepthAware=%d DepthRequested=%d DepthTextureValid=%d DepthEpsilonCm=%.4f"),
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
			DepthOcclusionEpsilonCm);
	}

	InteriorPortalRenderer::FInteriorPortalCompositionParameters* PassParameters =
		GraphBuilder.AllocParameters<InteriorPortalRenderer::FInteriorPortalCompositionParameters>();
	PassParameters->SceneColorTexture = SceneColor.Texture;
	PassParameters->SceneColorSampler =
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PortalTexture = PortalTexture;
	PassParameters->PortalSampler =
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->MainSceneDepthTexture = MainSceneDepthTexture;
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
	PassParameters->DepthOcclusionEpsilonCm = DepthOcclusionEpsilonCm;
	PassParameters->CompositionDebugMode = float(CompositionDebugMode);
	PassParameters->PortalExposureScale = PortalExposureScale;
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	TShaderMapRef<InteriorPortalRenderer::FInteriorPortalCompositionPS> PixelShader(
		GetGlobalShaderMap(InView.GetFeatureLevel()));
	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("InteriorPortal::BeforeDOFComposition"),
		InView,
		OutputViewport,
		OutputViewport,
		PixelShader,
		PassParameters);

	if (PortalCompositionDiagnosticsEnabled())
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalComposition DrawQueued Frame=%llu PortalId=%d Endpoint=%d DebugMode=%d"),
			GFrameCounter,
			Request.PortalId,
			Request.EndpointIndex,
			CompositionDebugMode);
	}
	return Output;
}
