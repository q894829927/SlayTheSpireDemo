#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "PresentationTypes.h"
#include "BattlePresentationController.generated.h"

class ABattleManager;
class UBattleHUDViewModel;
class UBattleHUDWidgetBase;

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
	void MarkPresentationResolutionCompletedExact(const FPresentationResolutionEnvelope& Envelope);
	void MarkEntireBacklogCompletedExact(const FPresentationResolutionEnvelope* AdditionalEnvelope);

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

	// G6 visual bookkeeping is scoped to one exact Resolution. It never changes
	// record order; future member indices are merely remembered as already shown
	// so their later chronological cursor can reduce without replaying visuals.
	FPresentationGroupTag ActiveG6Group;
	TArray<int32> ActiveG6GroupRecordIndices;
	int64 G6VisuallyPresentedResolutionId = 0;
	TSet<int32> G6VisuallyPresentedRecordIndices;

	static constexpr int32 MaxPlaybackEnvelopes = 8;
};
