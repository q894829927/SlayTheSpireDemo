#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalMath.h"
#include "Interior/InteriorPortalSystem.h"
#include "Math/RotationMatrix.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "UnrealClient.h"

namespace InteriorPortalVisualGPU
{
	constexpr int32 SettleFrames = 6;
	constexpr double CaptureTimeoutSeconds = 10.0;
	constexpr double ApertureInteriorSignalMin = 0.008;
	constexpr double ApertureExteriorDeltaMax = 0.040;
	constexpr double FixedExposureColorMaeMax = 0.180;
	constexpr double FixedExposureLuminanceRelativeMax = 0.220;
	constexpr double SynchronizationPoseSeparationMin = 0.025;
	constexpr double SynchronizationCurrentPoseMargin = 0.008;
	constexpr double SynchronizationFirstVsSettledMax = 0.120;
	const TCHAR* TargetMap = TEXT("/Game/House/L_Interior_LivingKitchen");

	enum class EPixelRegion : uint8
	{
		InnerAperture,
		OuterBoundingCorners
	};

	struct FVisualSample
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
		InteriorPortalMath::FPortalScreenBounds Bounds;
		FIntRect ViewRect;
		uint64 RequestedFrame = 0;
		uint64 CapturedFrame = 0;

		bool IsValid() const
		{
			return Width > 0 && Height > 0 && Pixels.Num() == Width * Height
				&& Bounds.bHasVisiblePortion
				&& ViewRect.Width() > 0 && ViewRect.Height() > 0
				&& ViewRect.Min.X >= 0 && ViewRect.Min.Y >= 0
				&& ViewRect.Max.X <= Width && ViewRect.Max.Y <= Height;
		}
	};

	static bool IsPixelInRegion(const FVector2D& ViewUV,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, const EPixelRegion Region)
	{
		const FVector2D Center = 0.5 * (Bounds.Min + Bounds.Max);
		const FVector2D Radius = 0.5 * (Bounds.Max - Bounds.Min);
		if (Radius.X <= UE_SMALL_NUMBER || Radius.Y <= UE_SMALL_NUMBER)
		{
			return false;
		}

		if (ViewUV.X < Bounds.Min.X || ViewUV.X > Bounds.Max.X
			|| ViewUV.Y < Bounds.Min.Y || ViewUV.Y > Bounds.Max.Y)
		{
			return false;
		}

		const FVector2D Ellipse((ViewUV.X - Center.X) / Radius.X, (ViewUV.Y - Center.Y) / Radius.Y);
		const double RadiusSquared = Ellipse.SquaredLength();
		if (Region == EPixelRegion::InnerAperture)
		{
			return RadiusSquared <= 0.36;
		}

		return RadiusSquared >= 1.25 && RadiusSquared <= 1.95;
	}

	static bool AreRasterDomainsCompatible(const FVisualSample& A, const FVisualSample& B)
	{
		return A.IsValid() && B.IsValid()
			&& A.Width == B.Width && A.Height == B.Height
			&& A.ViewRect.Min == B.ViewRect.Min && A.ViewRect.Max == B.ViewRect.Max;
	}

	static bool BuildSamplePixelRect(const FVisualSample& Sample,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, FIntRect& OutPixelRect)
	{
		if (!Sample.IsValid()
			|| !InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, Sample.ViewRect, OutPixelRect))
		{
			return false;
		}

		OutPixelRect.Min.X = FMath::Clamp(OutPixelRect.Min.X, 0, Sample.Width);
		OutPixelRect.Min.Y = FMath::Clamp(OutPixelRect.Min.Y, 0, Sample.Height);
		OutPixelRect.Max.X = FMath::Clamp(OutPixelRect.Max.X, 0, Sample.Width);
		OutPixelRect.Max.Y = FMath::Clamp(OutPixelRect.Max.Y, 0, Sample.Height);
		return OutPixelRect.Width() > 0 && OutPixelRect.Height() > 0;
	}

	static FVector2D PixelToViewUV(const FVisualSample& Sample, const int32 X, const int32 Y)
	{
		return FVector2D(
			float((double(X) + 0.5 - double(Sample.ViewRect.Min.X)) / double(Sample.ViewRect.Width())),
			float((double(Y) + 0.5 - double(Sample.ViewRect.Min.Y)) / double(Sample.ViewRect.Height())));
	}

	static FLinearColor LinearFromScreenshot(const FColor& Color)
	{
		return FLinearColor::FromSRGBColor(Color);
	}

	static double MeanAbsoluteColorDifference(const FVisualSample& A, const FVisualSample& B,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, const EPixelRegion Region, int32& OutCount)
	{
		OutCount = 0;
		if (!AreRasterDomainsCompatible(A, B))
		{
			return 1.0;
		}

		FIntRect PixelRect;
		if (!BuildSamplePixelRect(A, Bounds, PixelRect))
		{
			return 1.0;
		}

		double Sum = 0.0;
		for (int32 Y = PixelRect.Min.Y; Y < PixelRect.Max.Y; ++Y)
		{
			for (int32 X = PixelRect.Min.X; X < PixelRect.Max.X; ++X)
			{
				const FVector2D ViewUV = PixelToViewUV(A, X, Y);
				if (!IsPixelInRegion(ViewUV, Bounds, Region))
				{
					continue;
				}
				const int32 Index = Y * A.Width + X;
				const FLinearColor CA = LinearFromScreenshot(A.Pixels[Index]);
				const FLinearColor CB = LinearFromScreenshot(B.Pixels[Index]);
				Sum += (FMath::Abs(CA.R - CB.R) + FMath::Abs(CA.G - CB.G) + FMath::Abs(CA.B - CB.B)) / 3.0;
				++OutCount;
			}
		}
		return OutCount > 0 ? Sum / OutCount : 1.0;
	}

	static double MeanLuminance(const FVisualSample& Sample,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, int32& OutCount)
	{
		OutCount = 0;
		FIntRect PixelRect;
		if (!BuildSamplePixelRect(Sample, Bounds, PixelRect))
		{
			return 0.0;
		}

		double Sum = 0.0;
		for (int32 Y = PixelRect.Min.Y; Y < PixelRect.Max.Y; ++Y)
		{
			for (int32 X = PixelRect.Min.X; X < PixelRect.Max.X; ++X)
			{
				const FVector2D ViewUV = PixelToViewUV(Sample, X, Y);
				if (!IsPixelInRegion(ViewUV, Bounds, EPixelRegion::InnerAperture))
				{
					continue;
				}
				const FLinearColor C = LinearFromScreenshot(Sample.Pixels[Y * Sample.Width + X]);
				Sum += 0.2126 * C.R + 0.7152 * C.G + 0.0722 * C.B;
				++OutCount;
			}
		}
		return OutCount > 0 ? Sum / OutCount : 0.0;
	}

	static double RelativeDifference(const double A, const double B)
	{
		return FMath::Abs(A - B) / FMath::Max(FMath::Abs(B), 0.02);
	}

	static FTransform MakeEntryCameraTransform(const FTransform& EntryFrame,
		const double LocalY, const double LocalZ)
	{
		const FVector Location = EntryFrame.TransformPosition(FVector(260.0, LocalY, LocalZ));
		const FVector Forward = (EntryFrame.GetLocation() - Location).GetSafeNormal();
		FVector Up = EntryFrame.GetUnitAxis(EAxis::Z).GetSafeNormal();
		if (FMath::Abs(FVector::DotProduct(Forward, Up)) > 0.95)
		{
			Up = EntryFrame.GetUnitAxis(EAxis::Y).GetSafeNormal();
		}
		return FTransform(FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat(), Location);
	}

	class FPortalCompositionGPUValidationCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FPortalCompositionGPUValidationCommand(FAutomationTestBase* InTest)
			: Test(InTest), StartTime(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			if (bFinished)
			{
				return true;
			}

			if (!GEditor || !GEditor->IsPlayingSessionInEditor())
			{
				if (Stage == EStage::WaitForPIE && FPlatformTime::Seconds() - StartTime < 20.0)
				{
					return false;
				}
				return Fail(TEXT("PIE did not start, or ended before the Portal GPU validation completed."));
			}

			switch (Stage)
			{
			case EStage::WaitForPIE:
				if (!SetupFixture())
				{
					if (FPlatformTime::Seconds() - StartTime < 20.0)
					{
						return false;
					}
					return Fail(TEXT("Timed out waiting for the Portal PIE fixture."));
				}
				ApplyPose(PoseA, EInteriorPortalRendererBackend::CustomRenderPassSpike);
				SetStage(EStage::WaitBaselineA, SettleFrames);
				return false;

			case EStage::WaitBaselineA:
				if (!WaitComplete()) { return false; }
				if (!BeginCapture(TEXT("BaselineA"), BoundsA)) { return false; }
				Stage = EStage::CaptureBaselineA;
				return false;

			case EStage::CaptureBaselineA:
				if (!PollCapture()) { return false; }
				ApplyPose(PoseA, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitCompositionA, SettleFrames);
				return false;

			case EStage::WaitCompositionA:
				if (!WaitComplete()) { return false; }
				if (!BeginCapture(TEXT("CompositionA"), BoundsA)) { return false; }
				Stage = EStage::CaptureCompositionA;
				return false;

			case EStage::CaptureCompositionA:
				if (!PollCapture()) { return false; }
				ApplyPose(DirectA, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitDirectA, SettleFrames);
				return false;

			case EStage::WaitDirectA:
				if (!WaitComplete()) { return false; }
				if (!BeginCapture(TEXT("DirectA"), BoundsA)) { return false; }
				Stage = EStage::CaptureDirectA;
				return false;

			case EStage::CaptureDirectA:
				if (!PollCapture()) { return false; }
				ApplyPose(PoseB, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				if (!BeginCapture(TEXT("PortalBFirst"), BoundsB))
				{
					Stage = EStage::BeginPortalBFirst;
					return false;
				}
				Stage = EStage::CapturePortalBFirst;
				return false;

			case EStage::BeginPortalBFirst:
				if (!BeginCapture(TEXT("PortalBFirst"), BoundsB)) { return false; }
				Stage = EStage::CapturePortalBFirst;
				return false;

			case EStage::CapturePortalBFirst:
				if (!PollCapture()) { return false; }
				SetStage(EStage::WaitPortalBSettled, SettleFrames);
				return false;

			case EStage::WaitPortalBSettled:
				if (!WaitComplete()) { return false; }
				if (!BeginCapture(TEXT("PortalBSettled"), BoundsB)) { return false; }
				Stage = EStage::CapturePortalBSettled;
				return false;

			case EStage::CapturePortalBSettled:
				if (!PollCapture()) { return false; }
				ApplyPose(DirectB, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitDirectB, SettleFrames);
				return false;

			case EStage::WaitDirectB:
				if (!WaitComplete()) { return false; }
				if (!BeginCapture(TEXT("DirectB"), BoundsB)) { return false; }
				Stage = EStage::CaptureDirectB;
				return false;

			case EStage::CaptureDirectB:
				if (!PollCapture()) { return false; }
				AnalyzeSamples();
				CleanupFixture();
				bFinished = true;
				return true;
			}

			return Fail(TEXT("Portal GPU validation reached an invalid stage."));
		}

	private:
		enum class EStage : uint8
		{
			WaitForPIE,
			WaitBaselineA,
			CaptureBaselineA,
			WaitCompositionA,
			CaptureCompositionA,
			WaitDirectA,
			CaptureDirectA,
			BeginPortalBFirst,
			CapturePortalBFirst,
			WaitPortalBSettled,
			CapturePortalBSettled,
			WaitDirectB,
			CaptureDirectB
		};

		bool SetupFixture()
		{
			FWorldContext* PIEContext = GEditor ? GEditor->GetPIEWorldContext() : nullptr;
			UWorld* World = PIEContext ? PIEContext->World() : nullptr;
			if (!World)
			{
				return false;
			}

			Player = World->GetFirstPlayerController();
			if (!Player.IsValid() || !Player->PlayerCameraManager)
			{
				return false;
			}

			for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
			{
				if (IsValid(*It))
				{
					System = *It;
					break;
				}
			}
			if (!System.IsValid() || !IsValid(System->BluePortal) || !IsValid(System->OrangePortal))
			{
				return false;
			}

			Entry = System->OrangePortal;
			Exit = System->BluePortal;
			OriginalBackend = System->RendererBackend;
			OriginalRecursionDepth = System->RecursionDepth;
			bOriginalBluePlaced = System->BluePortal->bPlaced;
			bOriginalOrangePlaced = System->OrangePortal->bPlaced;
			System->BluePortal->bPlaced = true;
			System->OrangePortal->bPlaced = true;
			System->BluePortal->RefreshAppearance();
			System->OrangePortal->RefreshAppearance();
			System->RecursionDepth = 1;

			OriginalViewTarget = Player->GetViewTarget();
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			CameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!CameraActor.IsValid() || !CameraActor->GetCameraComponent())
			{
				return false;
			}
			CameraActor->GetCameraComponent()->SetFieldOfView(Player->PlayerCameraManager->GetFOVAngle());

			EyeAdaptationCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality"));
			PreExposureCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride"));
			if (EyeAdaptationCVar)
			{
				SavedEyeAdaptationQuality = EyeAdaptationCVar->GetInt();
				EyeAdaptationCVar->Set(0, ECVF_SetByCode);
				bSavedEyeAdaptation = true;
			}
			if (PreExposureCVar)
			{
				SavedPreExposure = PreExposureCVar->GetFloat();
				PreExposureCVar->Set(1.0f, ECVF_SetByCode);
				bSavedPreExposure = true;
			}

			const FTransform EntryFrame = Entry->GetLogicalFrame();
			const FTransform ExitFrame = Exit->GetLogicalFrame();
			PoseA = MakeEntryCameraTransform(EntryFrame, 0.0, 0.0);
			PoseB = MakeEntryCameraTransform(EntryFrame, 60.0, 35.0);
			DirectA = InteriorPortalMath::BuildVirtualViewTransform(PoseA, EntryFrame, ExitFrame);
			DirectB = InteriorPortalMath::BuildVirtualViewTransform(PoseB, EntryFrame, ExitFrame);

			ApplyPose(PoseA, EInteriorPortalRendererBackend::CustomRenderPassSpike);
			if (!ComputeProjectedBounds(PoseA, BoundsA))
			{
				Swap(Entry, Exit);
				const FTransform AlternateEntryFrame = Entry->GetLogicalFrame();
				const FTransform AlternateExitFrame = Exit->GetLogicalFrame();
				PoseA = MakeEntryCameraTransform(AlternateEntryFrame, 0.0, 0.0);
				PoseB = MakeEntryCameraTransform(AlternateEntryFrame, 60.0, 35.0);
				DirectA = InteriorPortalMath::BuildVirtualViewTransform(PoseA, AlternateEntryFrame, AlternateExitFrame);
				DirectB = InteriorPortalMath::BuildVirtualViewTransform(PoseB, AlternateEntryFrame, AlternateExitFrame);
				ApplyPose(PoseA, EInteriorPortalRendererBackend::CustomRenderPassSpike);
				if (!ComputeProjectedBounds(PoseA, BoundsA))
				{
					return false;
				}
			}
			if (!ComputeProjectedBounds(PoseB, BoundsB))
			{
				return false;
			}

			OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AutomationReports/PortalVisualGPU"));
			IFileManager::Get().MakeDirectory(*OutputDirectory, true);
			IFileManager::Get().Delete(*FPaths::Combine(OutputDirectory, TEXT("metrics.json")), false, true);
			bFixtureSetup = true;
			return true;
		}

		void ApplyPose(const FTransform& CameraTransform, const EInteriorPortalRendererBackend Backend)
		{
			if (!System.IsValid() || !Player.IsValid() || !CameraActor.IsValid())
			{
				return;
			}
			System->RendererBackend = Backend;
			System->RecursionDepth = 1;
			CameraActor->SetActorTransform(CameraTransform);
			Player->SetViewTarget(CameraActor.Get());
			Player->PlayerCameraManager->UpdateCamera(0.0f);
			System->RenderViews(Player.Get());
		}

		bool ComputeProjectedBounds(const FTransform& CameraTransform,
			InteriorPortalMath::FPortalScreenBounds& OutBounds) const
		{
			if (!Player.IsValid() || !Entry.IsValid())
			{
				return false;
			}
			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				return false;
			}

			const FMatrix PortalViewPlanes(
				FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
				FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
			const FMatrix ViewProjection = FTranslationMatrix(-CameraTransform.GetLocation())
				* FInverseRotationMatrix(CameraTransform.Rotator())
				* PortalViewPlanes * ProjectionData.ProjectionMatrix;
			const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
			return InteriorPortalMath::ProjectPortalApertureToScreenBounds(
				Entry->GetLogicalFrame(), Entry->HalfWidth, Entry->HalfHeight,
				ViewProjection, ViewRect, OutBounds, ProjectionData.IsPerspectiveProjection(),
				ProjectionData.GetNearPlaneFromProjectionMatrix());
		}

		bool GetCurrentViewRect(FIntRect& OutViewRect) const
		{
			OutViewRect = FIntRect();
			if (!Player.IsValid())
			{
				return false;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				return false;
			}

			OutViewRect = ProjectionData.GetConstrainedViewRect();
			return OutViewRect.Width() > 0 && OutViewRect.Height() > 0;
		}

		void SetStage(const EStage NewStage, const int32 FramesToWait)
		{
			Stage = NewStage;
			TargetFrame = GFrameCounter + FMath::Max(0, FramesToWait);
		}

		bool WaitComplete() const
		{
			return GFrameCounter >= TargetFrame;
		}

		bool BeginCapture(const FName SampleName,
			const InteriorPortalMath::FPortalScreenBounds& Bounds)
		{
			if (bCapturePending || !GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport)
			{
				return false;
			}

			FIntRect CurrentViewRect;
			if (!GetCurrentViewRect(CurrentViewRect))
			{
				return false;
			}

			PendingSampleName = SampleName;
			PendingBounds = Bounds;
			PendingViewRect = CurrentViewRect;
			CaptureRequestTime = FPlatformTime::Seconds();
			CaptureRequestedFrame = GFrameCounter;
			bCapturePending = true;

			// Keep the human-readable PNG artifact path, but do not use the screenshot
			// delegate as the source of Automation evidence. UnrealEditor-Cmd with
			// -RenderOffscreen writes the screenshot while not broadcasting
			// FScreenshotRequest::OnScreenshotCaptured, which caused false 10s timeouts.
			if (!FScreenshotRequest::IsScreenshotRequested())
			{
				const FString OutputPath = FPaths::Combine(OutputDirectory,
					FString::Printf(TEXT("%s.png"), *SampleName.ToString()));
				IFileManager::Get().Delete(*OutputPath, false, true);
				FScreenshotRequest::RequestScreenshot(OutputPath, false, false, false, FIntRect(), true);
			}
			return true;
		}

		bool PollCapture()
		{
			if (!bCapturePending)
			{
				return true;
			}

			// Read only after at least one renderer frame had an opportunity to consume
			// the pose/request. This preserves PortalBFirst as a genuine first-frame
			// synchronization probe instead of reading the previous back buffer.
			if (GFrameCounter <= CaptureRequestedFrame)
			{
				return false;
			}

			FViewport* Viewport = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->Viewport : nullptr;
			if (Viewport)
			{
				const FIntPoint Size = Viewport->GetSizeXY();
				TArray<FColor> Bitmap;
				if (Size.X > 0 && Size.Y > 0 && Viewport->ReadPixels(Bitmap)
					&& Bitmap.Num() == Size.X * Size.Y)
				{
					FVisualSample Sample;
					Sample.Width = Size.X;
					Sample.Height = Size.Y;
					Sample.Pixels = MoveTemp(Bitmap);
					Sample.Bounds = PendingBounds;
					Sample.ViewRect = PendingViewRect;
					Sample.RequestedFrame = CaptureRequestedFrame;
					Sample.CapturedFrame = GFrameCounter;
					Samples.Add(PendingSampleName, MoveTemp(Sample));
					bCapturePending = false;
					return true;
				}
			}

			if (FPlatformTime::Seconds() - CaptureRequestTime > CaptureTimeoutSeconds)
			{
				Test->AddError(FString::Printf(
					TEXT("Timed out reading Portal GPU sample '%s' from the game viewport."),
					*PendingSampleName.ToString()));
				bCapturePending = false;
				return true;
			}
			return false;
		}

		const FVisualSample* FindSample(const FName Name) const
		{
			return Samples.Find(Name);
		}

		void AnalyzeSamples()
		{
			const FVisualSample* BaselineA = FindSample(TEXT("BaselineA"));
			const FVisualSample* CompositionA = FindSample(TEXT("CompositionA"));
			const FVisualSample* DirectASample = FindSample(TEXT("DirectA"));
			const FVisualSample* PortalBFirst = FindSample(TEXT("PortalBFirst"));
			const FVisualSample* PortalBSettled = FindSample(TEXT("PortalBSettled"));
			const FVisualSample* DirectBSample = FindSample(TEXT("DirectB"));
			if (!BaselineA || !CompositionA || !DirectASample || !PortalBFirst || !PortalBSettled || !DirectBSample
				|| !BaselineA->IsValid() || !CompositionA->IsValid() || !DirectASample->IsValid()
				|| !PortalBFirst->IsValid() || !PortalBSettled->IsValid() || !DirectBSample->IsValid())
			{
				Test->AddError(TEXT("Portal GPU validation did not read all six valid viewport samples."));
				return;
			}

			int32 InsideCount = 0;
			int32 OutsideCount = 0;
			const double ApertureInsideSignal = MeanAbsoluteColorDifference(
				*CompositionA, *BaselineA, BoundsA, EPixelRegion::InnerAperture, InsideCount);
			const double ApertureOutsideDelta = MeanAbsoluteColorDifference(
				*CompositionA, *BaselineA, BoundsA, EPixelRegion::OuterBoundingCorners, OutsideCount);
			const bool bApertureSignalPass = InsideCount > 64 && ApertureInsideSignal >= ApertureInteriorSignalMin;
			const bool bApertureOutsidePass = OutsideCount > 32 && ApertureOutsideDelta <= ApertureExteriorDeltaMax;
			Test->TestTrue(TEXT("GPU aperture: composition produces a measurable signal inside the portal"),
				bApertureSignalPass);
			Test->TestTrue(TEXT("GPU aperture: bounding-box corners outside the ellipse remain main-view pixels"),
				bApertureOutsidePass);

			int32 ParityCountA = 0;
			int32 ParityCountB = 0;
			const double ParityMaeA = MeanAbsoluteColorDifference(
				*CompositionA, *DirectASample, BoundsA, EPixelRegion::InnerAperture, ParityCountA);
			const double ParityMaeB = MeanAbsoluteColorDifference(
				*PortalBSettled, *DirectBSample, BoundsB, EPixelRegion::InnerAperture, ParityCountB);
			int32 PortalLumCountA = 0;
			int32 DirectLumCountA = 0;
			int32 PortalLumCountB = 0;
			int32 DirectLumCountB = 0;
			const double PortalLumA = MeanLuminance(*CompositionA, BoundsA, PortalLumCountA);
			const double DirectLumA = MeanLuminance(*DirectASample, BoundsA, DirectLumCountA);
			const double PortalLumB = MeanLuminance(*PortalBSettled, BoundsB, PortalLumCountB);
			const double DirectLumB = MeanLuminance(*DirectBSample, BoundsB, DirectLumCountB);
			const double LuminanceRelativeA = RelativeDifference(PortalLumA, DirectLumA);
			const double LuminanceRelativeB = RelativeDifference(PortalLumB, DirectLumB);
			const bool bColorParityPass = ParityCountA > 64 && ParityCountB > 64
				&& FMath::Max(ParityMaeA, ParityMaeB) <= FixedExposureColorMaeMax;
			const bool bLuminanceParityPass = PortalLumCountA > 64 && DirectLumCountA > 64
				&& PortalLumCountB > 64 && DirectLumCountB > 64
				&& FMath::Max(LuminanceRelativeA, LuminanceRelativeB) <= FixedExposureLuminanceRelativeMax;
			Test->TestTrue(TEXT("GPU parity: portal/direct inner-aperture color stays within fixed-exposure tolerance"),
				bColorParityPass);
			Test->TestTrue(TEXT("GPU parity: portal/direct mean luminance stays within fixed-exposure tolerance"),
				bLuminanceParityPass);

			int32 PoseSeparationCount = 0;
			int32 FirstToCurrentCount = 0;
			int32 FirstToPreviousCount = 0;
			int32 FirstVsSettledCount = 0;
			const double PoseSeparation = MeanAbsoluteColorDifference(
				*DirectASample, *DirectBSample, BoundsB, EPixelRegion::InnerAperture, PoseSeparationCount);
			const double FirstToCurrent = MeanAbsoluteColorDifference(
				*PortalBFirst, *DirectBSample, BoundsB, EPixelRegion::InnerAperture, FirstToCurrentCount);
			const double FirstToPrevious = MeanAbsoluteColorDifference(
				*PortalBFirst, *DirectASample, BoundsB, EPixelRegion::InnerAperture, FirstToPreviousCount);
			const double FirstVsSettled = MeanAbsoluteColorDifference(
				*PortalBFirst, *PortalBSettled, BoundsB, EPixelRegion::InnerAperture, FirstVsSettledCount);
			const bool bSyncConclusive = PoseSeparationCount > 64 && PoseSeparation >= SynchronizationPoseSeparationMin;
			bool bSynchronizationPass = false;
			if (bSyncConclusive)
			{
				bSynchronizationPass = FirstToCurrentCount > 64 && FirstToPreviousCount > 64
					&& FirstVsSettledCount > 64
					&& FirstToCurrent + SynchronizationCurrentPoseMargin < FirstToPrevious
					&& FirstVsSettled <= SynchronizationFirstVsSettledMax;
				Test->TestTrue(TEXT("GPU synchronization: the first B frame matches current pose B better than previous pose A"),
					bSynchronizationPass);
			}
			else
			{
				Test->AddWarning(FString::Printf(
					TEXT("GPU synchronization metric is inconclusive because DirectA/DirectB separation %.4f is below %.4f; use the saved screenshots or a more discriminating authored view."),
					PoseSeparation, SynchronizationPoseSeparationMin));
			}

			Test->AddInfo(FString::Printf(
				TEXT("PortalVisualGPU viewport=%dx%d viewRect=(%d,%d)-(%d,%d) apertureInside=%.4f apertureOutside=%.4f parityMaeA=%.4f parityMaeB=%.4f lumRelA=%.4f lumRelB=%.4f poseSeparation=%.4f firstToCurrent=%.4f firstToPrevious=%.4f firstVsSettled=%.4f"),
				CompositionA->Width, CompositionA->Height,
				CompositionA->ViewRect.Min.X, CompositionA->ViewRect.Min.Y,
				CompositionA->ViewRect.Max.X, CompositionA->ViewRect.Max.Y,
				ApertureInsideSignal, ApertureOutsideDelta, ParityMaeA, ParityMaeB,
				LuminanceRelativeA, LuminanceRelativeB, PoseSeparation,
				FirstToCurrent, FirstToPrevious, FirstVsSettled));

			const FString Json = FString::Printf(
				TEXT("{\n  \"map\": \"%s\",\n  \"fixedExposure\": true,\n  \"rasterDomain\": {\"viewportSize\": [%d, %d], \"viewRectMin\": [%d, %d], \"viewRectSize\": [%d, %d]},\n  \"aperture\": {\"insideSignal\": %.6f, \"outsideDelta\": %.6f, \"pass\": %s},\n  \"parity\": {\"maeA\": %.6f, \"maeB\": %.6f, \"luminanceRelativeA\": %.6f, \"luminanceRelativeB\": %.6f, \"pass\": %s},\n  \"synchronization\": {\"poseSeparation\": %.6f, \"firstToCurrent\": %.6f, \"firstToPrevious\": %.6f, \"firstVsSettled\": %.6f, \"conclusive\": %s, \"pass\": %s}\n}\n"),
				TargetMap,
				CompositionA->Width, CompositionA->Height,
				CompositionA->ViewRect.Min.X, CompositionA->ViewRect.Min.Y,
				CompositionA->ViewRect.Width(), CompositionA->ViewRect.Height(),
				ApertureInsideSignal, ApertureOutsideDelta,
				(bApertureSignalPass && bApertureOutsidePass) ? TEXT("true") : TEXT("false"),
				ParityMaeA, ParityMaeB, LuminanceRelativeA, LuminanceRelativeB,
				(bColorParityPass && bLuminanceParityPass) ? TEXT("true") : TEXT("false"),
				PoseSeparation, FirstToCurrent, FirstToPrevious, FirstVsSettled,
				bSyncConclusive ? TEXT("true") : TEXT("false"),
				bSynchronizationPass ? TEXT("true") : TEXT("false"));
			FFileHelper::SaveStringToFile(Json,
				*FPaths::Combine(OutputDirectory, TEXT("metrics.json")));
		}

		void CleanupFixture()
		{
			if (bCleaned)
			{
				return;
			}
			if (bSavedEyeAdaptation && EyeAdaptationCVar)
			{
				EyeAdaptationCVar->Set(SavedEyeAdaptationQuality, ECVF_SetByCode);
			}
			if (bSavedPreExposure && PreExposureCVar)
			{
				PreExposureCVar->Set(SavedPreExposure, ECVF_SetByCode);
			}
			if (System.IsValid())
			{
				System->RendererBackend = OriginalBackend;
				System->RecursionDepth = OriginalRecursionDepth;
				if (IsValid(System->BluePortal))
				{
					System->BluePortal->bPlaced = bOriginalBluePlaced;
					System->BluePortal->RefreshAppearance();
				}
				if (IsValid(System->OrangePortal))
				{
					System->OrangePortal->bPlaced = bOriginalOrangePlaced;
					System->OrangePortal->RefreshAppearance();
				}
			}
			if (Player.IsValid() && OriginalViewTarget.IsValid())
			{
				Player->SetViewTarget(OriginalViewTarget.Get());
				if (Player->PlayerCameraManager)
				{
					Player->PlayerCameraManager->UpdateCamera(0.0f);
				}
			}
			if (CameraActor.IsValid())
			{
				CameraActor->Destroy();
			}
			bCapturePending = false;
			bCleaned = true;
		}

		bool Fail(const FString& Message)
		{
			Test->AddError(Message);
			CleanupFixture();
			bFinished = true;
			return true;
		}

		FAutomationTestBase* Test = nullptr;
		EStage Stage = EStage::WaitForPIE;
		double StartTime = 0.0;
		uint64 TargetFrame = 0;
		bool bFinished = false;
		bool bFixtureSetup = false;
		bool bCleaned = false;

		TWeakObjectPtr<AInteriorPortalSystem> System;
		TWeakObjectPtr<AInteriorPortal> Entry;
		TWeakObjectPtr<AInteriorPortal> Exit;
		TWeakObjectPtr<APlayerController> Player;
		TWeakObjectPtr<ACameraActor> CameraActor;
		TWeakObjectPtr<AActor> OriginalViewTarget;
		EInteriorPortalRendererBackend OriginalBackend = EInteriorPortalRendererBackend::SceneCapture;
		int32 OriginalRecursionDepth = 1;
		bool bOriginalBluePlaced = false;
		bool bOriginalOrangePlaced = false;

		IConsoleVariable* EyeAdaptationCVar = nullptr;
		IConsoleVariable* PreExposureCVar = nullptr;
		int32 SavedEyeAdaptationQuality = 0;
		float SavedPreExposure = 0.0f;
		bool bSavedEyeAdaptation = false;
		bool bSavedPreExposure = false;

		FTransform PoseA;
		FTransform PoseB;
		FTransform DirectA;
		FTransform DirectB;
		InteriorPortalMath::FPortalScreenBounds BoundsA;
		InteriorPortalMath::FPortalScreenBounds BoundsB;

		FString OutputDirectory;
		TMap<FName, FVisualSample> Samples;
		FName PendingSampleName;
		InteriorPortalMath::FPortalScreenBounds PendingBounds;
		FIntRect PendingViewRect;
		bool bCapturePending = false;
		double CaptureRequestTime = 0.0;
		uint64 CaptureRequestedFrame = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalCompositionGPUVisualTest,
	"SlayTheSpireDemo.Interior.PortalVisualGPU.CompositionPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalCompositionGPUVisualTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEditor)
	{
		AddError(TEXT("Portal GPU validation requires the Unreal Editor."));
		return true;
	}
	if (GEditor->IsPlayingSessionInEditor())
	{
		AddError(TEXT("Stop the current PIE/SIE session before running PortalVisualGPU.CompositionPipeline."));
		return true;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	const FString CurrentMap = EditorWorld ? EditorWorld->GetOutermost()->GetName() : FString();
	if (CurrentMap != InteriorPortalVisualGPU::TargetMap)
	{
		AddError(FString::Printf(
			TEXT("Open %s before running this GPU test. The test deliberately does not load/save maps so user-owned dirty map changes are preserved. Current map: %s"),
			InteriorPortalVisualGPU::TargetMap, *CurrentMap));
		return true;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(InteriorPortalVisualGPU::FPortalCompositionGPUValidationCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR