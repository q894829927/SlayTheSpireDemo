#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"

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
#include "HAL/IConsoleManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "LegacyScreenPercentageDriver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace InteriorPortalFullViewFamilyBeforeDOFSpikePrivate
{
	class FPortalBeforeDOFExtractionExtension final : public FWorldSceneViewExtension
	{
	public:
		FPortalBeforeDOFExtractionExtension(
			const FAutoRegister& AutoRegister, UWorld* InWorld, FRenderTarget* InExtractionTarget)
			: FWorldSceneViewExtension(AutoRegister, InWorld)
			, ExtractionTarget(InExtractionTarget)
		{
		}

		bool WasExecuted() const
		{
			return bExecuted.Load();
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
			if (Pass != ISceneViewExtension::EPostProcessingPass::BeforeDOF
				|| !InView.Family || !InView.Family->bAdditionalViewFamily
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
					if (!SceneColor.IsValid() || !ExtractionTarget)
					{
						return SceneColor;
					}

					FRDGTextureRef ExtractionTexture =
						ExtractionTarget->GetRenderTargetTexture(GraphBuilder);
					if (!ExtractionTexture)
					{
						return SceneColor;
					}

					GraphBuilder.UseInternalAccessMode(ExtractionTexture);
					AddDrawTexturePass(
						GraphBuilder,
						View,
						SceneColor.Texture,
						ExtractionTexture,
						SceneColor.ViewRect.Min,
						SceneColor.ViewRect.Size(),
						FIntPoint::ZeroValue,
						ExtractionTexture->Desc.Extent,
						TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
					GraphBuilder.UseExternalAccessMode(ExtractionTexture, ERHIAccess::SRVMask);
					bExecuted.Store(true);
					return SceneColor;
				}));
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(
			const FSceneViewExtensionContext& Context) const override
		{
			return ExtractionTarget != nullptr
				&& FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
		}

	private:
		FRenderTarget* ExtractionTarget = nullptr;
		TAtomic<bool> bExecuted { false };
	};

	UWorld* FindPortalSpikeWorld()
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

	bool IsFiniteTransform(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		return FMath::IsFinite(Location.X)
			&& FMath::IsFinite(Location.Y)
			&& FMath::IsFinite(Location.Z)
			&& Transform.GetRotation().IsNormalized();
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

	TStrongObjectPtr<UTextureRenderTarget2D> MakeFloatTarget(const int32 Width, const int32 Height)
	{
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
		if (Target.IsValid())
		{
			Target->RenderTargetFormat = RTF_RGBA16f;
			Target->ClearColor = FLinearColor::Black;
			Target->bAutoGenerateMips = false;
			Target->InitCustomFormat(Width, Height, PF_FloatRGBA, true);
			Target->UpdateResourceImmediate(true);
		}
		return Target;
	}

	void WriteResult(
		const FString& Status,
		const FString& Detail,
		const FString& FinalPngPath,
		const FString& FinalExrPath,
		const FString& BeforeDofPngPath,
		const FString& BeforeDofExrPath,
		bool bBeforeDofCallbackExecuted,
		const FInteriorPortalRenderRequest* Request,
		const FIntPoint TargetSize)
	{
		const FString RequestJson = Request
			? FString::Printf(
				TEXT("{\"portalId\":%d,\"endpointIndex\":%d,\"viewLocation\":[%.6f,%.6f,%.6f],")
				TEXT("\"virtualRotation\":[%.8f,%.8f,%.8f,%.8f],\"exitClipPlane\":[%.8f,%.8f,%.8f,%.8f],")
				TEXT("\"projectedBounds\":[%.6f,%.6f,%.6f,%.6f]}"),
				Request->PortalId,
				Request->EndpointIndex,
				Request->ViewLocation.X, Request->ViewLocation.Y, Request->ViewLocation.Z,
				Request->VirtualView.GetRotation().X, Request->VirtualView.GetRotation().Y,
				Request->VirtualView.GetRotation().Z, Request->VirtualView.GetRotation().W,
				Request->ExitClipPlane.X, Request->ExitClipPlane.Y,
				Request->ExitClipPlane.Z, Request->ExitClipPlane.W,
				Request->ProjectedBounds.Min.X, Request->ProjectedBounds.Min.Y,
				Request->ProjectedBounds.Max.X, Request->ProjectedBounds.Max.Y)
			: TEXT("null");

		const FString Json = FString::Printf(
			TEXT("{\n")
			TEXT("  \"status\":\"%s\",\n")
			TEXT("  \"detail\":\"%s\",\n")
			TEXT("  \"rendererPath\":\"standalone full FSceneViewFamily + temporary additional-family BeforeDOF extraction extension\",\n")
			TEXT("  \"sceneViewIsSceneCapture\":false,\n")
			TEXT("  \"targetFormat\":\"PF_FloatRGBA / RTF_RGBA16f\",\n")
			TEXT("  \"targetSize\":[%d,%d],\n")
			TEXT("  \"beforeDOFCallbackExecuted\":%s,\n")
			TEXT("  \"finalOutputPNG\":\"%s\",\n")
			TEXT("  \"finalOutputEXR\":\"%s\",\n")
			TEXT("  \"beforeDOFOutputPNG\":\"%s\",\n")
			TEXT("  \"beforeDOFOutputEXR\":\"%s\",\n")
			TEXT("  \"claimBoundary\":\"BeforeDOF lit SceneColor extraction feasibility only; no main-view composition, exposure parity, temporal, stencil/depth or recursion acceptance claim\",\n")
			TEXT("  \"request\":%s\n")
			TEXT("}\n"),
			*Status.ReplaceCharWithEscapedChar(),
			*Detail.ReplaceCharWithEscapedChar(),
			TargetSize.X,
			TargetSize.Y,
			bBeforeDofCallbackExecuted ? TEXT("true") : TEXT("false"),
			*FinalPngPath.ReplaceCharWithEscapedChar(),
			*FinalExrPath.ReplaceCharWithEscapedChar(),
			*BeforeDofPngPath.ReplaceCharWithEscapedChar(),
			*BeforeDofExrPath.ReplaceCharWithEscapedChar(),
			*RequestJson);

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
			TEXT("PortalFullViewFamilyBeforeDOFSpike.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		FFileHelper::SaveStringToFile(Json, *ReportPath);
	}

	void RunPortalFullViewFamilyBeforeDOFSpike()
	{
		UWorld* World = FindPortalSpikeWorld();
		AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		if (!World || !PortalSystem || !Player || !Player->PlayerCameraManager)
		{
			WriteResult(TEXT("BLOCKED"),
				TEXT("PIE/Game world, portal system, player or PlayerCameraManager unavailable"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}
		if (!PortalSystem->IsLinked())
		{
			WriteResult(TEXT("BLOCKED"), TEXT("Portal pair is not linked/placed"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}

		ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
		FSceneViewProjectionData ProjectionData;
		if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
			|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			WriteResult(TEXT("BLOCKED"), TEXT("Player projection data unavailable"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}

		const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
		if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
		{
			WriteResult(TEXT("BLOCKED"), TEXT("Player constrained view rect is empty"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}

		const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
		const FTransform PlayerView(POV.Rotation, POV.Location);
		const FMatrix PortalViewPlanes(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		const FMatrix PlayerViewProjection =
			FTranslationMatrix(-PlayerView.GetLocation())
			* FInverseRotationMatrix(PlayerView.Rotator())
			* PortalViewPlanes
			* ProjectionData.ProjectionMatrix;

		AInteriorPortal* Entry = nullptr;
		AInteriorPortal* Exit = nullptr;
		int32 EndpointIndex = INDEX_NONE;
		int32 CandidateIndex = 0;
		for (AInteriorPortal* Candidate : {PortalSystem->BluePortal.Get(), PortalSystem->OrangePortal.Get()})
		{
			InteriorPortalMath::FPortalScreenBounds CandidateBounds;
			if (IsValid(Candidate)
				&& InteriorPortalMath::ProjectPortalApertureToScreenBounds(
					Candidate->GetLogicalFrame(), Candidate->HalfWidth, Candidate->HalfHeight,
					PlayerViewProjection, PlayerRect, CandidateBounds,
					ProjectionData.IsPerspectiveProjection(),
					ProjectionData.GetNearPlaneFromProjectionMatrix()))
			{
				Entry = Candidate;
				Exit = Candidate == PortalSystem->BluePortal
					? PortalSystem->OrangePortal.Get() : PortalSystem->BluePortal.Get();
				EndpointIndex = CandidateIndex;
				break;
			}
			++CandidateIndex;
		}

		if (!IsValid(Entry) || !IsValid(Exit) || EndpointIndex == INDEX_NONE)
		{
			WriteResult(TEXT("BLOCKED"), TEXT("No linked portal aperture is visible"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}

		FInteriorPortalRenderRequest Request;
		if (!FInteriorPortalRenderRequest::Build(
			EndpointIndex, EndpointIndex, 0,
			PlayerView, Entry->GetLogicalFrame(), Exit->GetLogicalFrame(),
			Entry->HalfWidth, Entry->HalfHeight,
			PlayerViewProjection, PlayerRect,
			ProjectionData.ProjectionMatrix,
			ProjectionData.IsPerspectiveProjection(),
			ProjectionData.GetNearPlaneFromProjectionMatrix(),
			PortalSystem->ClipPlaneBias,
			1, Request)
			|| !Request.IsValid() || !IsFiniteTransform(Request.VirtualView))
		{
			WriteResult(TEXT("BLOCKED"), TEXT("Transformed portal request construction failed"),
				FString(), FString(), FString(), FString(), false, nullptr, FIntPoint::ZeroValue);
			return;
		}

		const int32 Width = FMath::Clamp(PlayerRect.Width(), 256, 1920);
		const int32 Height = FMath::Max(144,
			FMath::RoundToInt(Width * double(PlayerRect.Height()) / double(PlayerRect.Width())));
		const FIntPoint TargetSize(Width, Height);

		TStrongObjectPtr<UTextureRenderTarget2D> FinalTarget = MakeFloatTarget(Width, Height);
		TStrongObjectPtr<UTextureRenderTarget2D> BeforeDofTarget = MakeFloatTarget(Width, Height);
		FRenderTarget* FinalTargetResource = FinalTarget.IsValid()
			? FinalTarget->GameThread_GetRenderTargetResource() : nullptr;
		FRenderTarget* BeforeDofTargetResource = BeforeDofTarget.IsValid()
			? BeforeDofTarget->GameThread_GetRenderTargetResource() : nullptr;
		if (!FinalTargetResource || !BeforeDofTargetResource || !World->Scene)
		{
			WriteResult(TEXT("BLOCKED"), TEXT("Final or BeforeDOF render target resource unavailable"),
				FString(), FString(), FString(), FString(), false, &Request, TargetSize);
			return;
		}

		FEngineShowFlags ShowFlags = GEngine && GEngine->GameViewport
			? GEngine->GameViewport->EngineShowFlags
			: FEngineShowFlags(ESFIM_Game);
		ShowFlags.SetEyeAdaptation(false);
		ShowFlags.SetMotionBlur(false);
		ShowFlags.SetTemporalAA(false);
		ShowFlags.SetScreenPercentage(false);

		FSceneViewStateReference ViewState;
		ViewState.Allocate(World->GetFeatureLevel());

		FSceneViewFamilyContext ViewFamily(
			FSceneViewFamily::ConstructionValues(FinalTargetResource, World->Scene, ShowFlags)
				.SetTime(World->GetTime())
				.SetResolveScene(true)
				.SetRealtimeUpdate(true)
				.SetAdditionalViewFamily(true));
		ViewFamily.EngineShowFlags = ShowFlags;
		ViewFamily.SceneCaptureSource = SCS_FinalColorHDR;
		ViewFamily.ViewMode = VMI_Lit;
		ViewFamily.SetScreenPercentageInterface(
			new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

		TSharedRef<FPortalBeforeDOFExtractionExtension, ESPMode::ThreadSafe> ExtractionExtension =
			FSceneViewExtensions::NewExtension<FPortalBeforeDOFExtractionExtension>(
				World, BeforeDofTargetResource);
		ViewFamily.ViewExtensions.Add(ExtractionExtension);

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
		ViewInitOptions.ViewOrigin = Request.ViewLocation;
		ViewInitOptions.ViewLocation = Request.ViewLocation;
		ViewInitOptions.ViewRotation = Request.VirtualView.Rotator();
		ViewInitOptions.ViewRotationMatrix = Request.ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionData.ProjectionMatrix;
		ViewInitOptions.FOV = POV.FOV;
		ViewInitOptions.DesiredFOV = POV.FOV;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::Transparent;
		ViewInitOptions.PlayerIndex = 0;
		ViewInitOptions.bUseFieldOfViewForLOD = true;
		HidePortalPrimitives(PortalSystem->BluePortal.Get(), ViewInitOptions);
		HidePortalPrimitives(PortalSystem->OrangePortal.Get(), ViewInitOptions);

		FSceneView* SceneView = new FSceneView(ViewInitOptions);
		ViewFamily.Views.Add(SceneView);
		SceneView->bIsGameView = true;
		SceneView->bIsSceneCapture = false;
		SceneView->bCameraCut = true;
		SceneView->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		SceneView->GlobalClippingPlane = Request.ExitClipPlane;
		SceneView->StartFinalPostprocessSettings(Request.ViewLocation);
		SceneView->OverridePostProcessSettings(
			POV.PostProcessSettings, POV.PostProcessBlendWeight, true);
		SceneView->FinalPostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		SceneView->FinalPostProcessSettings.DynamicGlobalIlluminationMethod =
			EDynamicGlobalIlluminationMethod::Lumen;
		SceneView->FinalPostProcessSettings.bOverride_ReflectionMethod = true;
		SceneView->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
		SceneView->EndFinalPostprocessSettings(ViewInitOptions);

		FCanvas Canvas(FinalTargetResource, nullptr, World, World->GetFeatureLevel(),
			FCanvas::CDM_DeferDrawing, 1.0f);
		IRendererModule& RendererModule =
			FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
		RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);
		FlushRenderingCommands();

		const FString ReportDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("AutomationReports"));
		IFileManager::Get().MakeDirectory(*ReportDirectory, true);
		const FString FinalPngPath = FPaths::Combine(
			ReportDirectory, TEXT("PortalFullViewFamilyBeforeDOF_Final.png"));
		const FString FinalExrPath = FPaths::Combine(
			ReportDirectory, TEXT("PortalFullViewFamilyBeforeDOF_Final.exr"));
		const FString BeforeDofPngPath = FPaths::Combine(
			ReportDirectory, TEXT("PortalFullViewFamilyBeforeDOF_SceneColor.png"));
		const FString BeforeDofExrPath = FPaths::Combine(
			ReportDirectory, TEXT("PortalFullViewFamilyBeforeDOF_SceneColor.exr"));

		FImage FinalImage;
		const bool bFinalReadback = FImageUtils::GetRenderTargetImage(FinalTarget.Get(), FinalImage);
		const bool bFinalPng = bFinalReadback
			&& FImageUtils::SaveImageByExtension(*FinalPngPath, FinalImage, 100);
		const bool bFinalExr = bFinalReadback
			&& FImageUtils::SaveImageByExtension(*FinalExrPath, FinalImage, 0);

		FImage BeforeDofImage;
		const bool bBeforeDofReadback =
			FImageUtils::GetRenderTargetImage(BeforeDofTarget.Get(), BeforeDofImage);
		const bool bBeforeDofPng = bBeforeDofReadback
			&& FImageUtils::SaveImageByExtension(*BeforeDofPngPath, BeforeDofImage, 100);
		const bool bBeforeDofExr = bBeforeDofReadback
			&& FImageUtils::SaveImageByExtension(*BeforeDofExrPath, BeforeDofImage, 0);
		const bool bCallbackExecuted = ExtractionExtension->WasExecuted();

		const FString Detail = FString::Printf(
			TEXT("Endpoint=%d callback=%d final(read=%d png=%d exr=%d) beforeDOF(read=%d png=%d exr=%d). ")
			TEXT("The BeforeDOF target is the lit pre-tonemap SceneColor candidate; inspect EXR values and compare spatial content to the final target."),
			EndpointIndex,
			bCallbackExecuted ? 1 : 0,
			bFinalReadback ? 1 : 0,
			bFinalPng ? 1 : 0,
			bFinalExr ? 1 : 0,
			bBeforeDofReadback ? 1 : 0,
			bBeforeDofPng ? 1 : 0,
			bBeforeDofExr ? 1 : 0);

		const bool bOutputWritten = bCallbackExecuted
			&& bBeforeDofReadback && (bBeforeDofPng || bBeforeDofExr);
		WriteResult(
			bOutputWritten ? TEXT("BEFOREDOF_OUTPUT_WRITTEN") : TEXT("BEFOREDOF_EXTRACTION_FAILED"),
			Detail,
			bFinalPng ? FinalPngPath : FString(),
			bFinalExr ? FinalExrPath : FString(),
			bBeforeDofPng ? BeforeDofPngPath : FString(),
			bBeforeDofExr ? BeforeDofExrPath : FString(),
			bCallbackExecuted,
			&Request,
			TargetSize);

		UE_LOG(LogTemp, Display, TEXT("PortalBeforeDOFSpike: %s"), *Detail);
	}

	FAutoConsoleCommand GRunPortalFullViewFamilyBeforeDOFSpike(
		TEXT("portal.RunFullViewFamilyBeforeDOFSpike"),
		TEXT("Render one transformed full view family and extract its lit SceneColor at BeforeDOF into a separate RGBA16f target."),
		FConsoleCommandDelegate::CreateStatic(&RunPortalFullViewFamilyBeforeDOFSpike));
}
