#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "PresentationTypes.h"
#include "PresentationG8Types.h"
#include "BattlePresentationController.generated.h"

class ABattleManager;
class UBattleHUDViewModel;
class UBattleHUDWidgetBase;
struct FBufferedCardIntent;
enum class EBufferedIntentShadowEvaluation : uint8;

UCLASS(Transient, BlueprintType)
class SLAYTHESPIREDEMO_API UBattlePresentationController : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(
		ABattleManager* InBattleManager,
		UBattleHUDViewModel* InViewModel,
		UBattleHUDWidgetBase* InWidget
	);

	void Shutdown();
	void SetWidget(UBattleHUDWidgetBase* InWidget);

	// G8 Presentation-owned authority identity. Ordinary Skip/reconcile keeps the
	// same token; binding/battle/authority replacement invalidates it.
	bool TryGetPresentationSessionToken(FPresentationSessionToken& OutToken) const;
	bool IsCurrentPresentationSession(const FPresentationSessionToken& Token) const;

	// Fast input asks whether authoritative Blocking chronology can currently be
	// collapsed by Skip. Detached DamageNumber cosmetic lifetime is never part of
	// this query after G8-D.
	bool HasSkippablePresentationDelay() const;
	bool TryCaptureFastInputCatchUpTarget(
		FPresentationSessionToken& OutSessionToken,
		int64& OutBattleId,
		int64& OutExpectedCatchUpRevision) const;

	// G9-A exact Presentation-lag authority only. These helpers capture/evaluate
	// an already-sealed future normal player-card surface without replaying input,
	// skipping Presentation or predicting a future Gameplay revision.
	bool TryCaptureBufferedCardTarget(
		int32 RequestedRuntimeId,
		FBufferedCardIntent& OutIntent) const;
	EBufferedIntentShadowEvaluation EvaluateBufferedCardTarget(
		const FBufferedCardIntent& Intent) const;

	// Detached Damage feature control. Disabling it is not an authority/binding
	// replacement, so it keeps the current PresentationSessionToken, retires
	// current-session cosmetics, and routes later Damage through Blocking.
	void SetDetachedDamageG8CEnabled(bool bEnabled);
	bool IsDetachedDamageG8CEnabled() const { return bDetachedDamageG8CEnabled; }

	// Intentionally C++-only. Blueprint completion/skip must pass through
	// UBattleHUDWidgetBase so callback deferral and exact visual cancellation
	// hardening cannot be bypassed by a concrete WBP.
	void NotifyPresentationFinished(const FPresentationPlaybackToken& Token);
	void SkipPresentation();

	// G6 production activation is entered only while the leader SingleRecord is
	// already being offered. The Controller reuses the sealed G2 semantic
	// candidate and asks the Base Widget for an all-or-nothing tracked-unit
	// replacement. False preserves the ordinary G5 SingleRecord path.
	bool TryActivatePresentationGroupG6(
		const FPresentationRecord& LeaderRecord,
		const FPresentationPlaybackToken& OfferedSingleRecordToken);

	// Future members that were already co-presented by an accepted Group still
	// arrive at their normal chronological reducer cursor. Returning true consumes
	// only the visual marker; the caller declines visible Begin so the existing
	// Controller immediate fallback reduces the exact record in order.
	bool ConsumeVisuallyPresentedGroupRecordG6(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& OfferedSingleRecordToken);

	// Exact Group completion from the Base Widget. This is intentionally distinct
	// from the historical G3 dormant Group callback in NotifyPresentationFinished.
	void NotifyPresentationGroupFinishedG6(
		const FPresentationPlaybackToken& Token,
		const TArray<int32>& RecordIndices);

	void NotifyWidgetLost(UBattleHUDWidgetBase* LostWidget);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Presentation", meta = (ClampMin = "0.05"))
	float PlaybackTimeoutSeconds = 3.0f;

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetBacklogCountForTesting() const;
	bool IsWaitingForCompletionForTesting() const;
	FPresentationPlaybackToken GetActivePlaybackTokenForTesting() const;
	EPresentationPlaybackUnitKind GetActivePlaybackUnitKindForTesting() const;
	int64 GetLastCompletedResolutionIdForTesting() const;
	void ExpireActivePlaybackForTesting();
	void ReconcileActiveEnvelopeToFinalSnapshotForTesting();
	bool RebindActivePlaybackAsGroupForTesting(
		const FPresentationGroupTag& Group,
		const TArray<int32>& RecordIndices
	);
	bool TryGetWorkingSnapshotForTesting(FPresentationStateSnapshot& OutSnapshot) const;
	bool ReduceEnvelopeForTesting(
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		FPresentationStateSnapshot& OutReducedSnapshot
	);
	bool TryBuildSemanticPresentationGroupCandidateForTesting(
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		int32 LeaderRecordIndex,
		FPresentationGroupSemanticCandidate& OutCandidate
	);
	int32 GetG6VisuallyPresentedRecordCountForTesting() const;
#endif

protected:
	virtual void BeginDestroy() override;

private:
	enum class EDetachedDamageAttemptResult : uint8
	{
		DeclinedToBlocking,
		Consumed
	};

	void HandlePresentationResolutionReady(const FPresentationResolutionEnvelope& Envelope);
	void HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision);
	void StartNextEnvelope();
	void StartNextRecord();
	void CompleteActiveRecord();
	void CompleteActiveEnvelope();

	// G3 recovery scopes are behaviorally distinct. ActiveEnvelope recovery keeps
	// later queued Envelopes; EntireBacklog collapse is used only for global catch-up.
	void ReconcileActiveEnvelopeToFinalSnapshot();
	void CollapseEntireBacklogToEnvelope(const FPresentationResolutionEnvelope& Envelope);
	void ResetPlaybackState(bool bAdvanceGeneration);
	void CancelActivePlaybackUnit();
	void RetireActivePlaybackForWidgetReplacement(UBattleHUDWidgetBase* ExpectedOldWidget);
	void MarkPresentationResolutionCompletedExact(const FPresentationResolutionEnvelope& Envelope);
	void MarkEntireBacklogCompletedExact(const FPresentationResolutionEnvelope* AdditionalEnvelope);

	void EnsureControllerEpoch();
	bool IsPresentationOwnedMode() const;
	void InvalidatePresentationSession(UBattleHUDWidgetBase* CleanupWidget = nullptr);
	void EstablishPresentationSessionForCurrentBinding();

	// G8 detached Damage transaction.
	EDetachedDamageAttemptResult TryCommitDetachedDamageRecord(
		const FPresentationRecord& Record);
	bool IsDetachedDamageCommitContextCurrent(
		const FPresentationRecord& Record,
		int32 ExpectedRecordIndex,
		UBattleHUDWidgetBase* ExpectedWidget,
		const FPresentationSessionToken& ExpectedSession) const;
	void AdvancePastCommittedDetachedDamageRecord();
	void RefreshInputIfPresentationCaughtUp();
	void CancelCurrentSessionDetachedDamageVisuals();

	void EnterPresentationUnavailableFailSafe();
	void EnterDirectBaselineMode();
	void CancelActiveTimeout();
	void ScheduleActiveTimeout();
	bool HandleActiveTimeout(float DeltaTime);
	void AdvancePlaybackGeneration();
	bool IsEnvelopeForCurrentBattle(const FPresentationResolutionEnvelope& Envelope) const;
	bool ApplyRecordToWorkingSnapshot(const FPresentationRecord& Record);
	void ApplyDisplayedSnapshot(const FPresentationStateSnapshot& Snapshot, bool bRefreshBindings);
	void ResetG6ActiveGroupState();

	// G2 semantic-only preflight. It may inspect only the supplied frozen
	// baseline + sealed Envelope. It deliberately has no Widget/ownership input
	// and does not start playback or mark any Record visually presented.
	bool TryBuildSemanticPresentationGroupCandidate(
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		int32 LeaderRecordIndex,
		FPresentationGroupSemanticCandidate& OutCandidate
	);

	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDWidgetBase> Widget = nullptr;

	UPROPERTY(Transient)
	TArray<FPresentationResolutionEnvelope> PlaybackQueue;

	UPROPERTY(Transient)
	FPresentationResolutionEnvelope ActiveEnvelope;

	UPROPERTY(Transient)
	FPresentationStateSnapshot DisplayedPresentationSnapshot;

	UPROPERTY(Transient)
	FPresentationStateSnapshot WorkingPresentationSnapshot;

	bool bHasActiveEnvelope = false;
	bool bHasDisplayedPresentationSnapshot = false;
	bool bHasWorkingPresentationSnapshot = false;
	bool bWaitingForCompletion = false;
	int32 ActiveRecordIndex = INDEX_NONE;
	int64 CurrentBattleId = 0;
	int64 LastQueuedResolutionId = 0;
	int64 LastCompletedResolutionId = 0;
	int64 LocalPlaybackGeneration = 1;
	FPresentationPlaybackToken ActivePlaybackToken;
	FPresentationPlaybackToken ScheduledTimeoutToken;
	FTSTicker::FDelegateHandle PlaybackTimeoutTickerHandle;

	// G8 session identity is independent from the per-playback generation.
	// ControllerEpoch is minted once per UObject instance and intentionally is not
	// reset by Shutdown/Initialize so replacement Controllers cannot ABA-match.
	int64 ControllerEpoch = 0;
	int64 NextPresentationSessionGeneration = 1;
	FPresentationSessionToken ActivePresentationSessionToken;

	// Detached Damage production switch. G8-D removed compatibility debt from the
	// Controller; no duration/timer state is retained here in G8-E.
	bool bDetachedDamageG8CEnabled = true;

	// G6 visual bookkeeping is scoped to one exact Resolution. It never changes
	// record order; future member indices are merely remembered as already shown
	// so their later chronological cursor can reduce without replaying visuals.
	FPresentationGroupTag ActiveG6Group;
	TArray<int32> ActiveG6GroupRecordIndices;
	int64 G6VisuallyPresentedResolutionId = 0;
	TSet<int32> G6VisuallyPresentedRecordIndices;

	static constexpr int32 MaxPlaybackEnvelopes = 8;
};