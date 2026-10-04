#include "InteriorPortalRenderer.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace InteriorPortalNearGrazingDiagnosticsPrivate
{
	TAutoConsoleVariable<int32> CVarNearGrazingDiagnostics(
		TEXT("portal.NearGrazingDiagnostics"),
		0,
		TEXT("STEP 1B.11A diagnostics. 0=quiet, 1=log near/grazing/projective-aperture state every 60 observed frames."),
		ECVF_Default);

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

	class FNearGrazingMonitor
	{
	public:
		bool Start(UWorld* World)
		{
			if (bRunning || !World)
			{
				return false;
			}
			ActiveWorld = World;
			WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
				this, &FNearGrazingMonitor::OnWorldPostActorTick);
			bRunning = true;
			Status = TEXT("RUNNING");
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalNearGrazing: started. Run alongside the full-view TSR spike and exercise close/grazing/screen-edge views."));
			return true;
		}

		void Stop()
		{
			if (WorldPostActorTickHandle.IsValid())
			{
				FWorldDelegates::OnWorldPostActorTick.Remove(WorldPostActorTickHandle);
				WorldPostActorTickHandle.Reset();
			}
			bRunning = false;
			Status = TEXT("STOPPED");
			WriteReport();
			ActiveWorld.Reset();
			UE_LOG(LogTemp, Display,
				TEXT("PortalNearGrazing: stopped. Observed=%llu Skipped=%llu NearClip=%llu Crossing=%llu ViewportClip=%llu Grazing=%llu Close=%llu ProjectiveInvalid=%llu"),
				FramesObserved, FramesSkipped, NearClipFrames, CameraCrossingFrames,
				ViewportClippedFrames, GrazingFrames, CloseFrames, ProjectiveInvalidFrames);
		}

		bool IsRunning() const { return bRunning; }

		void DumpReport() const
		{
			WriteReport();
			UE_LOG(LogTemp, Display,
				TEXT("PortalNearGrazing: report written. Observed=%llu Skipped=%llu MinPlaneDistance=%.4f MinFacingAbsDot=%.6f NearClip=%llu Crossing=%llu ViewportClip=%llu ProjectiveInvalid=%llu"),
				FramesObserved, FramesSkipped, MinPlaneDistance, MinFacingAbsDot,
				NearClipFrames, CameraCrossingFrames, ViewportClippedFrames,
				ProjectiveInvalidFrames);
		}

	private:
		void OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			(void)TickType;
			(void)DeltaSeconds;
			if (!bRunning || World != ActiveWorld.Get())
			{
				return;
			}
			Sample(World);
		}

		void Skip(const TCHAR* Reason)
		{
			++FramesSkipped;
			LastSkipReason = Reason;
		}

		void Sample(UWorld* World)
		{
			AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!PortalSystem || !Player || !Player->PlayerCameraManager || !PortalSystem->IsLinked())
			{
				Skip(TEXT("portal pair/player unavailable"));
				return;
			}

			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
				|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
			{
				Skip(TEXT("projection unavailable"));
				return;
			}

			const FIntRect PlayerRect = ProjectionData.GetConstrainedViewRect();
			if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0)
			{
				Skip(TEXT("view rect unavailable"));
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
				Skip(TEXT("no visible portal"));
				return;
			}

			const FTransform EntryFrame = Entry->GetLogicalFrame();
			const FTransform ExitFrame = Exit->GetLogicalFrame();
			const double NearClip = ProjectionData.GetNearPlaneFromProjectionMatrix();
			FInteriorPortalRenderRequest Request;
			if (!FInteriorPortalRenderRequest::Build(
				EndpointIndex, EndpointIndex, 0,
				PlayerView, EntryFrame, ExitFrame,
				Entry->HalfWidth, Entry->HalfHeight,
				PlayerViewProjection, PlayerRect,
				ProjectionData.ProjectionMatrix,
				ProjectionData.IsPerspectiveProjection(),
				NearClip,
				PortalSystem->ClipPlaneBias,
				1, Request)
				|| !Request.IsValid())
			{
				Skip(TEXT("request build failed"));
				return;
			}

			const FVector EntryNormal = EntryFrame.GetUnitAxis(EAxis::X).GetSafeNormal();
			const double PlaneDistance = FMath::Abs(FVector::DotProduct(
				EntryNormal, PlayerView.GetLocation() - EntryFrame.GetLocation()));
			const double FacingAbsDot = FMath::Abs(FVector::DotProduct(
				POV.Rotation.Vector().GetSafeNormal(), EntryNormal));
			const double CloseThreshold = FMath::Max(10.0, 2.0 * FMath::Max(0.001, NearClip));

			++FramesObserved;
			LastEndpointIndex = EndpointIndex;
			LastPlaneDistance = PlaneDistance;
			LastFacingAbsDot = FacingAbsDot;
			LastNearClip = NearClip;
			LastProjectiveValid = Request.ProjectiveAperture.bValid;
			LastProjectiveQuality = Request.ProjectiveAperture.DeterminantQuality;
			LastBounds = Request.ProjectedBounds;
			LastSkipReason = TEXT("");
			MinPlaneDistance = FMath::Min(MinPlaneDistance, PlaneDistance);
			MinFacingAbsDot = FMath::Min(MinFacingAbsDot, FacingAbsDot);

			if (Request.ProjectedBounds.bIntersectsNearClip) { ++NearClipFrames; }
			if (Request.ProjectedBounds.bCameraCrossing) { ++CameraCrossingFrames; }
			if (Request.ProjectedBounds.bClippedToViewport) { ++ViewportClippedFrames; }
			if (FacingAbsDot < 0.15) { ++GrazingFrames; }
			if (PlaneDistance < CloseThreshold) { ++CloseFrames; }
			if (!Request.ProjectiveAperture.bValid) { ++ProjectiveInvalidFrames; }

			if (CVarNearGrazingDiagnostics.GetValueOnGameThread() != 0
				&& (FramesObserved == 1 || (FramesObserved % 60) == 0))
			{
				UE_LOG(LogTemp, Display,
					TEXT("PortalNearGrazing Frame=%llu Observed=%llu Endpoint=%d PlaneDistance=%.4f FacingAbsDot=%.6f NearClipW=%.4f BoundsNear=%d Crossing=%d ViewportClip=%d ProjectiveValid=%d ProjectiveQuality=%.9g Counts(Near=%llu Crossing=%llu Viewport=%llu Grazing=%llu Close=%llu Invalid=%llu)"),
					GFrameCounter, FramesObserved, EndpointIndex,
					PlaneDistance, FacingAbsDot, NearClip,
					Request.ProjectedBounds.bIntersectsNearClip ? 1 : 0,
					Request.ProjectedBounds.bCameraCrossing ? 1 : 0,
					Request.ProjectedBounds.bClippedToViewport ? 1 : 0,
					Request.ProjectiveAperture.bValid ? 1 : 0,
					Request.ProjectiveAperture.DeterminantQuality,
					NearClipFrames, CameraCrossingFrames, ViewportClippedFrames,
					GrazingFrames, CloseFrames, ProjectiveInvalidFrames);
			}
		}

		void WriteReport() const
		{
			const double SafeMinPlaneDistance = FMath::IsFinite(MinPlaneDistance) ? MinPlaneDistance : -1.0;
			const double SafeMinFacingAbsDot = FMath::IsFinite(MinFacingAbsDot) ? MinFacingAbsDot : -1.0;
			const FString Json = FString::Printf(
				TEXT("{\n")
				TEXT("  \"status\":\"%s\",\n")
				TEXT("  \"framesObserved\":%llu,\n")
				TEXT("  \"framesSkipped\":%llu,\n")
				TEXT("  \"lastEndpointIndex\":%d,\n")
				TEXT("  \"lastPlaneDistance\":%.9g,\n")
				TEXT("  \"minPlaneDistance\":%.9g,\n")
				TEXT("  \"lastFacingAbsDot\":%.9g,\n")
				TEXT("  \"minFacingAbsDot\":%.9g,\n")
				TEXT("  \"lastNearClipW\":%.9g,\n")
				TEXT("  \"lastProjectiveApertureValid\":%s,\n")
				TEXT("  \"lastProjectiveDeterminantQuality\":%.9g,\n")
				TEXT("  \"lastBoundsIntersectsNearClip\":%s,\n")
				TEXT("  \"lastBoundsCameraCrossing\":%s,\n")
				TEXT("  \"lastBoundsClippedToViewport\":%s,\n")
				TEXT("  \"nearClipFrameCount\":%llu,\n")
				TEXT("  \"cameraCrossingFrameCount\":%llu,\n")
				TEXT("  \"viewportClippedFrameCount\":%llu,\n")
				TEXT("  \"grazingFrameCount\":%llu,\n")
				TEXT("  \"closeFrameCount\":%llu,\n")
				TEXT("  \"projectiveInvalidFrameCount\":%llu,\n")
				TEXT("  \"lastProjectedBounds\":[%.9g,%.9g,%.9g,%.9g],\n")
				TEXT("  \"lastSkipReason\":\"%s\",\n")
				TEXT("  \"claimBoundary\":\"STEP 1B.11A projective aperture and near/grazing composition telemetry only; no main depth/stencil continuity, recursion or production performance acceptance claim\"\n")
				TEXT("}\n"),
				*Status.ReplaceCharWithEscapedChar(),
				FramesObserved, FramesSkipped, LastEndpointIndex,
				LastPlaneDistance, SafeMinPlaneDistance,
				LastFacingAbsDot, SafeMinFacingAbsDot,
				LastNearClip,
				LastProjectiveValid ? TEXT("true") : TEXT("false"),
				LastProjectiveQuality,
				LastBounds.bIntersectsNearClip ? TEXT("true") : TEXT("false"),
				LastBounds.bCameraCrossing ? TEXT("true") : TEXT("false"),
				LastBounds.bClippedToViewport ? TEXT("true") : TEXT("false"),
				NearClipFrames, CameraCrossingFrames, ViewportClippedFrames,
				GrazingFrames, CloseFrames, ProjectiveInvalidFrames,
				LastBounds.Min.X, LastBounds.Min.Y, LastBounds.Max.X, LastBounds.Max.Y,
				*LastSkipReason.ReplaceCharWithEscapedChar());

			const FString ReportPath = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("AutomationReports"),
				TEXT("PortalNearGrazingDiagnostics.json"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
			FFileHelper::SaveStringToFile(Json, *ReportPath);
		}

		bool bRunning = false;
		TWeakObjectPtr<UWorld> ActiveWorld;
		FDelegateHandle WorldPostActorTickHandle;
		FString Status = TEXT("STOPPED");
		FString LastSkipReason = TEXT("not started");
		uint64 FramesObserved = 0;
		uint64 FramesSkipped = 0;
		uint64 NearClipFrames = 0;
		uint64 CameraCrossingFrames = 0;
		uint64 ViewportClippedFrames = 0;
		uint64 GrazingFrames = 0;
		uint64 CloseFrames = 0;
		uint64 ProjectiveInvalidFrames = 0;
		int32 LastEndpointIndex = INDEX_NONE;
		double LastPlaneDistance = -1.0;
		double LastFacingAbsDot = -1.0;
		double LastNearClip = -1.0;
		double MinPlaneDistance = TNumericLimits<double>::Max();
		double MinFacingAbsDot = TNumericLimits<double>::Max();
		bool LastProjectiveValid = false;
		float LastProjectiveQuality = 0.0f;
		InteriorPortalMath::FPortalScreenBounds LastBounds;
	};

	TUniquePtr<FNearGrazingMonitor> GNearGrazingMonitor;

	void StartNearGrazingDiagnostics()
	{
		if (GNearGrazingMonitor && GNearGrazingMonitor->IsRunning())
		{
			UE_LOG(LogTemp, Display, TEXT("PortalNearGrazing: already running."));
			return;
		}
		UWorld* World = FindPlayableWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalNearGrazing: PIE/Game world unavailable."));
			return;
		}
		GNearGrazingMonitor = MakeUnique<FNearGrazingMonitor>();
		if (!GNearGrazingMonitor->Start(World))
		{
			GNearGrazingMonitor.Reset();
		}
	}

	void StopNearGrazingDiagnostics()
	{
		if (GNearGrazingMonitor)
		{
			GNearGrazingMonitor->Stop();
			GNearGrazingMonitor.Reset();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalNearGrazing: not running."));
		}
	}

	void DumpNearGrazingDiagnostics()
	{
		if (GNearGrazingMonitor)
		{
			GNearGrazingMonitor->DumpReport();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("PortalNearGrazing: not running; no live report to dump."));
		}
	}

	FAutoConsoleCommand GStartNearGrazingDiagnosticsCommand(
		TEXT("portal.StartNearGrazingDiagnostics"),
		TEXT("Start STEP 1B.11A close/grazing/projective aperture telemetry."),
		FConsoleCommandDelegate::CreateStatic(&StartNearGrazingDiagnostics));

	FAutoConsoleCommand GStopNearGrazingDiagnosticsCommand(
		TEXT("portal.StopNearGrazingDiagnostics"),
		TEXT("Stop STEP 1B.11A near/grazing telemetry and write the final report."),
		FConsoleCommandDelegate::CreateStatic(&StopNearGrazingDiagnostics));

	FAutoConsoleCommand GDumpNearGrazingDiagnosticsCommand(
		TEXT("portal.DumpNearGrazingDiagnostics"),
		TEXT("Write STEP 1B.11A telemetry to Saved/AutomationReports/PortalNearGrazingDiagnostics.json."),
		FConsoleCommandDelegate::CreateStatic(&DumpNearGrazingDiagnostics));
}
