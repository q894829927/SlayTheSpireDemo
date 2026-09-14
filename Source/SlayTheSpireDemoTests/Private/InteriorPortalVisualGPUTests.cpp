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
		uint64 RequestedFrame = 0;
		uint64 CapturedFrame = 0;

		bool IsValid() const
		{
			return Width > 0 && Height > 0 && Pixels.Num() == Width * Height
				&& Bounds.bHasVisiblePortion;
		}
	};

	static bool IsPixelInRegion(const FVector2D& UV,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, const EPixelRegion Region)
	{
		const FVector2D Center = 0.5 * (Bounds.Min + Bounds.Max);
		const FVector2D Radius = 0.5 * (Bounds.Max - Bounds.Min);
		if (Radius.X <= UE_SMALL_NUMBER || Radius.Y <= UE_SMALL_NUMBER)
		{
			return false;
		}

		if (UV.X < Bounds.Min.X || UV.X > Bounds.Max.X
			|| UV.Y < Bounds.Min.Y || UV.Y > Bounds.Max.Y)
		{
			return false;
		}

		const FVector2D Ellipse((UV.X - Center.X) / Radius.X, (UV.Y - Center.Y) / Radius.Y);
		const double RadiusSquared = Ellipse.SquaredLength();
		if (Region == EPixelRegion::InnerAperture)
		{
			// Stay well inside the shader's feathered edge and the physical portal rim.
			return RadiusSquared <= 0.36;
		}

		// The current feasibility shader is allowed to touch only the analytic ellipse.
		// Bounding-rectangle corners outside the ellipse must remain main-view pixels.
		return RadiusSquared >= 1.25 && RadiusSquared <= 1.95;
	}

	static FLinearColor LinearFromScreenshot(const FColor& Color)
	{
		return FLinearColor::FromSRGBColor(Color);
	}

	static double MeanAbsoluteColorDifference(const FVisualSample& A, const FVisualSample& B,
		const InteriorPortalMath::FPortalScreenBounds& Bounds, const EPixelRegion Region, int32& OutCount)
	{
		OutCount = 0;
		if (!A.IsValid() || !B.IsValid() || A.Width != B.Width || A.Height != B.Height)
		{
			return 1.0;
		}

		const int32 MinX = FMath::Clamp(FMath::FloorToInt(Bounds.Min.X * A.Width), 0, A.Width - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt(Bounds.Max.X * A.Width), 0, A.Width - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt(Bounds.Min.Y * A.Height), 0, A.Height - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt(Bounds.Max.Y * A.Height), 0, A.Height - 1);

		double Sum = 0.0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D UV((X + 0.5) / double(A.Width), (Y + 0.5) / double(A.Height));
				if (!IsPixelInRegion(UV, Bounds, Region))
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
		if (!Sample.IsValid())
		{
			return 0.0;
		}

		const int32 MinX = FMath::Clamp(FMath::FloorToInt(Bounds.Min.X * Sample.Width), 0, Sample.Width - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt(Bounds.Max.X * Sample.Width), 0, Sample.Width - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt(Bounds.Min.Y * Sample.Height), 0, Sample.Height - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt(Bounds.Max.Y * Sample.Height), 0, Sample.Height - 1);

		double Sum = 0.0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D UV((X + 0.5) / double(Sample.Width), (Y + 0.5) / double(Sample.Height));
				if (!IsPixelInRegion(UV, Bounds, EPixelRegion::InnerAperture))
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

		virtual ~FPortalCompositionGPUValidationCommand() override
		{
			RemoveScreenshotDelegate();
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
				if (!BeginScreenshot(TEXT("BaselineA"), BoundsA)) { return false; }
				Stage = EStage::CaptureBaselineA;
				return false;

			case EStage::CaptureBaselineA:
				if (!PollScreenshot()) { return false; }
				ApplyPose(PoseA, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitCompositionA, SettleFrames);
				return false;

			case EStage::WaitCompositionA:
				if (!WaitComplete()) { return false; }
				if (!BeginScreenshot(TEXT("CompositionA"), BoundsA)) { return false; }
				Stage = EStage::CaptureCompositionA;
				return false;

			case EStage::CaptureCompositionA:
				if (!PollScreenshot()) { return false; }
				ApplyPose(DirectA, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitDirectA, SettleFrames);
				return false;

			case EStage::WaitDirectA:
				if (!WaitComplete()) { return false; }
				if (!BeginScreenshot(TEXT("DirectA"), BoundsA)) { return false; }
				Stage = EStage::CaptureDirectA;
				return false;

			case EStage::CaptureDirectA:
				if (!PollScreenshot()) { return false; }
				// Deliberately submit the new pose and request its screenshot in the same
				// latent update. This catches a CRP target that is sampled one frame late.
				ApplyPose(PoseB, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				if (!BeginScreenshot(TEXT("PortalBFirst"), BoundsB))
				{
					Stage = EStage::BeginPortalBFirst;
					return false;
				}
				Stage = EStage::CapturePortalBFirst;
				return false;

			case EStage::BeginPortalBFirst:
				if (!BeginScreenshot(TEXT("PortalBFirst"), BoundsB)) { return false; }
				Stage = EStage::CapturePortalBFirst;
				return false;

			case EStage::CapturePortalBFirst:
				if (!PollScreenshot()) { return false; }
				SetStage(EStage::WaitPortalBSettled, SettleFrames);
				return false;

			case EStage::WaitPortalBSettled:
				if (!WaitComplete()) { return false; }
				if (!BeginScreenshot(TEXT("PortalBSettled"), BoundsB)) { return false; }
				Stage = EStage::CapturePortalBSettled;
				return false;

			case EStage::CapturePortalBSettled:
				if (!PollScreenshot()) { return false; }
				ApplyPose(DirectB, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike);
				SetStage(EStage::WaitDirectB, SettleFrames);
				return false;

			case EStage::WaitDirectB:
				if (!WaitComplete()) { return false; }
				if (!BeginScreenshot(TEXT("DirectB"), BoundsB)) { return false; }
				Stage = EStage::CaptureDirectB;
				return false;

			case EStage::CaptureDirectB:
				if (!PollScreenshot()) { return false; }
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
				// If the authored orange endpoint is malformed, try the blue endpoint before failing.
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
			// Submit from the exact camera cache used for this validation stage. The normal
			// game path will also tick, but this explicit call makes the first-frame probe
			// test the current request instead of waiting for a later Automation tick.
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

		void SetStage(const EStage NewStage, const int32 FramesToWait)
		{
			Stage = NewStage;
			TargetFrame = GFrameCounter + FMath::Max(0, FramesToWait);
		}

		bool WaitComplete() const
		{
			return GFrameCounter >= TargetFrame;
		}

		bool BeginScreenshot(const FName SampleName,
			const InteriorPortalMath::FPortalScreenBounds& Bounds)
		{
			if (bScreenshotRequested || FScreenshotRequest::IsScreenshotRequested()
				|| !GEngine || !GEngine->GameViewport)
			{
				return false;
			}

			PendingSampleName = SampleName;
			PendingBounds = Bounds;
			bScreenshotCaptured = false;
			ScreenshotRequestTime = FPlatformTime::Seconds();
			ScreenshotHandle = FScreenshotRequest::OnScreenshotCaptured().AddRaw(
				this, &FPortalCompositionGPUValidationCommand::OnScreenshotCaptured);
			bScreenshotRequested = true;

			const FString OutputPath = FPaths::Combine(OutputDirectory,
				FString::Printf(TEXT("%s.png"), *SampleName.ToString()));
			FScreenshotRequest::RequestScreenshot(OutputPath, false, false, false, FIntRect(), true);
			return true;
		}

		bool PollScreenshot()
		{
			if (bScreenshotCaptured)
			{
				RemoveScreenshotDelegate();
				bScreenshotRequested = false;
				bScreenshotCaptured = false;
				return true;
			}
			if (bScreenshotRequested && FPlatformTime::Seconds() - ScreenshotRequestTime > 10.0)
			{
				Test->AddError(FString::Printf(TEXT("Timed out capturing Portal GPU sample '%s'."),
					*PendingSampleName.ToString()));
				RemoveScreenshotDelegate();
				bScreenshotRequested = false;
				bScreenshotCaptured = false;
				return true;
			}
			return false;
		}

		void OnScreenshotCaptured(const int32 SizeX, const int32 SizeY, const TArray<FColor>& Bitmap)
		{
			FVisualSample Sample;
			Sample.Width = SizeX;
			Sample.Height = SizeY;
			Sample.Pixels = Bitmap;
			Sample.Bounds = PendingBounds;
			Sample.RequestedFrame = ScreenshotRequestedFrame;
			Sample.CapturedFrame = GFrameCounter;
			Samples.Add(PendingSampleName, MoveTemp(Sample));
			bScreenshotCaptured = true;
		}

		void RemoveScreenshotDelegate()
		{
			if (ScreenshotHandle.IsValid())
			{
				FScreenshotRequest::OnScreenshotCaptured().Remove(ScreenshotHandle);
				ScreenshotHandle = FDelegateHandle();
			}
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
				Test->AddError(TEXT("Portal GPU validation did not capture all six valid viewport samples."));
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
				TEXT("PortalVisualGPU apertureInside=%.4f apertureOutside=%.4f parityMaeA=%.4f parityMaeB=%.4f lumRelA=%.4f lumRelB=%.4f poseSeparation=%.4f firstToCurrent=%.4f firstToPrevious=%.4f firstVsSettled=%.4f"),
				ApertureInsideSignal, ApertureOutsideDelta, ParityMaeA, ParityMaeB,
				LuminanceRelativeA, LuminanceRelativeB, PoseSeparation,
				FirstToCurrent, FirstToPrevious, FirstVsSettled));

			const FString Json = FString::Printf(
				TEXT("{\n  \"map\": \"%s\",\n  \"fixedExposure\": true,\n  \"aperture\": {\"insideSignal\": %.6f, \"outsideDelta\": %.6f, \"pass\": %s},\n  \"parity\": {\"maeA\": %.6f, \"maeB\": %.6f, \"luminanceRelativeA\": %.6f, \"luminanceRelativeB\": %.6f, \"pass\": %s},\n  \"synchronization\": {\"poseSeparation\": %.6f, \"firstToCurrent\": %.6f, \"firstToPrevious\": %.6f, \"firstVsSettled\": %.6f, \"conclusive\": %s, \"pass\": %s}\n}\n"),
				TargetMap,
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
			RemoveScreenshotDelegate();
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
		FDelegateHandle ScreenshotHandle;
		bool bScreenshotRequested = false;
		bool bScreenshotCaptured = false;
		double ScreenshotRequestTime = 0.0;
		uint64 ScreenshotRequestedFrame = 0;
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

	// Start ordinary PIE (not SIE), run the GPU/readback probe, then leave the
	// user's editor map untouched. Manual visual acceptance remains a separate gate.
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(InteriorPortalVisualGPU::FPortalCompositionGPUValidationCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
