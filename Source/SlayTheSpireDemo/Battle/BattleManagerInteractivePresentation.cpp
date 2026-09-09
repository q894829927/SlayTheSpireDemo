#include "BattleManager.h"

#include "BattleReadSnapshot.h"
#include "../Actions/BattleAction.h"
#include "../Actions/BattleActionQueue.h"

FPresentationRecordWriter ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary(
	const UBattleAction* BoundaryAction
)
{
	FPresentationRecordWriter ContinuationWriter;
	UBattleActionQueue* Queue = ActionQueue.Get();
	if (!IsValid(Queue) || !Queue->IsCurrentAction(BoundaryAction))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] Interactive selection boundary ignored: caller is not the current BattleAction."));
		return ContinuationWriter;
	}

	// A pending player choice is a real player-facing state boundary even though
	// the Gameplay Queue remains intentionally held by the selection chain. Give
	// the frozen pre-selection snapshot its own monotonic revision so the UI can
	// distinguish "before Draw playback" from "Draw visible, choice eligible".
	AdvanceStateRevision();
	ContinuationWriter.BattleId = BattleId;
	ContinuationWriter.SelectionBoundaryRevision = static_cast<int64>(StateRevision);
	ContinuationWriter.DirectOutcomeAcceptance.BindUObject(
		this,
		&ABattleManager::RegisterDirectSelectionOutcomeBoundary
	);
	ScheduleReadStateReadyPublish();

	// Presentation is optional. Gameplay selection continues even when committed
	// history is disabled or already unavailable. The writer still carries the
	// exact boundary and direct-outcome capability for no-history correlation.
	if (!bPresentationAvailable)
	{
		return ContinuationWriter;
	}

	FBattleReadSnapshot ReadSnapshot;
	const bool bBuiltReadSnapshot = Queue->RunReadSnapshotAtCurrentActionBoundary(
		BoundaryAction,
		[this, &ReadSnapshot]()
		{
			return TryBuildPlayerFacingReadSnapshot(ReadSnapshot);
		}
	);
	if (!bBuiltReadSnapshot)
	{
		AbortPresentationResolution();
		MarkPresentationUnavailable(
			TEXT("Could not build the exact interactive pre-selection Presentation read state."));
		return ContinuationWriter;
	}

	// Freeze outside the Queue's read-only override. QueryCardPlayability and
	// bCanEndTurn must still observe the real busy/pending-selection Gameplay
	// boundary rather than pretending normal player commands are available.
	FPresentationStateSnapshot FrozenSnapshot;
	if (!TryFreezePresentationStateSnapshot(ReadSnapshot, FrozenSnapshot))
	{
		AbortPresentationResolution();
		MarkPresentationUnavailable(
			TEXT("Could not freeze the exact interactive pre-selection Presentation snapshot."));
		return ContinuationWriter;
	}

	LatestFrozenPresentationBaseline = FrozenSnapshot;
	LatestFrozenPresentationBaselineResolutionId = LastSealedPresentationResolutionId;
	bHasLatestFrozenPresentationBaseline = true;

	// No-history mode still needs the exact new Hand baseline for selection UI.
	if (!bCommittedPresentationRecordingEnabledForBattle)
	{
		return ContinuationWriter;
	}

	UBattlePresentationRecorder* Recorder = PresentationRecorder.Get();
	if (!IsValid(Recorder) || !Recorder->HasActiveResolution())
	{
		MarkPresentationUnavailable(
			TEXT("Interactive selection boundary reached without an active Presentation Resolution."));
		return ContinuationWriter;
	}

	const EPresentationResolutionOrigin ContinuationOrigin = Recorder->GetActiveOrigin();

	if (!bPresentationAvailable || !Recorder->IsActiveResolutionValid())
	{
		Recorder->AbortResolution();
		if (bPresentationAvailable)
		{
			MarkPresentationUnavailable(
				TEXT("Interactive Presentation Record append failed; the unpublished pre-selection history was discarded."));
		}
		return ContinuationWriter;
	}

	FPresentationResolutionEnvelope Envelope;
	if (!Recorder->SealResolution(FrozenSnapshot, Envelope))
	{
		MarkPresentationUnavailable(
			TEXT("Interactive pre-selection Presentation Resolution seal failed."));
		return ContinuationWriter;
	}

	if (Envelope.BattleId != static_cast<int64>(BattleId)
		|| Envelope.ResolutionId <= 0
		|| static_cast<uint64>(Envelope.ResolutionId) <= LastSealedPresentationResolutionId)
	{
		MarkPresentationUnavailable(
			TEXT("Interactive pre-selection Presentation Resolution identity was invalid or duplicated."));
		return ContinuationWriter;
	}

	LastSealedPresentationResolutionId = static_cast<uint64>(Envelope.ResolutionId);
	LatestFrozenPresentationBaselineResolutionId = LastSealedPresentationResolutionId;
	EnqueuePendingPublicPresentation(MoveTemp(Envelope));

	// Delivery is deferred exactly like an ordinary sealed Resolution. Gameplay
	// does not wait for playback; the ViewModel merely withholds selection input
	// until its displayed revision catches this frozen boundary.
	ScheduleReadStateReadyPublish();

	if (!BeginPresentationResolution(ContinuationOrigin))
	{
		return ContinuationWriter;
	}

	FPresentationRecordWriter RecordedContinuationWriter = GetActivePresentationRecordWriter();
	if (!RecordedContinuationWriter.IsAvailable())
	{
		return ContinuationWriter;
	}

	RecordedContinuationWriter.SelectionBoundaryRevision = ContinuationWriter.SelectionBoundaryRevision;
	RecordedContinuationWriter.DirectOutcomeAcceptance = ContinuationWriter.DirectOutcomeAcceptance;
	return RecordedContinuationWriter;
}
