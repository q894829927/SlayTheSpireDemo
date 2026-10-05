#pragma once

#include "CoreMinimal.h"
#include "../Presentation/PresentationG9Types.h"

class ABattleManager;
class UBattleHUDWidgetBase;
class UBattleHUDViewModel;
class UBattlePresentationController;
struct FBattleHUDTargetView;

enum class EBufferedPlayerIntentKind : uint8 { None, CardSelection, EndTurn, CardPlay };
enum class EBufferedPlayerIntentEvaluation : uint8 { None, Waiting, Ready, Dropped };

struct SLAYTHESPIREDEMO_API FEndTurnIntentAvailability
{
	FPlayerTurnAuthorityToken Turn;
	bool bCanAcceptEndTurnIntent = false;
	bool bCanExecuteEndTurnNow = false;
};

struct SLAYTHESPIREDEMO_API FBufferedPlayerIntentDecision
{
	EBufferedPlayerIntentKind Kind = EBufferedPlayerIntentKind::None;
	FBufferedCardIntent Card;
	FBufferedEndTurnIntent EndTurn;
	FQueuedCardPlayIntent Play;
};

struct SLAYTHESPIREDEMO_API FEndTurnIntentAcceptance
{
	bool bAccepted = false;
	bool bRetireCardIntent = false;
	bool bRetireFastInputRetry = false;
	bool bCancelTransientSelection = false;
};

// One value-owned input arbiter per HUD: draft + confirmed FIFO + EndTurn.
// It returns decisions; only the HUD submits formal ViewModel Requests.
// Selection-only APIs remain for the isolated G9-A historical shadow fixtures.
class SLAYTHESPIREDEMO_API FBattleHUDBufferedPlayerInput
{
public:
	void Bind(UBattleHUDWidgetBase* InOwner);
	void SetEnabled(bool bInEnabled);
	void Clear();
	void EnableConfirmedPlayQueue() { bConfirmedQueueMode = true; }
	bool BeginCardDraft(int32 RuntimeId);
	bool ConfirmCardDraft(FName TargetPresentationId);
	void CancelCardDraft() { Draft.Reset(); }
	bool CanBeginCardDraft(int32 RuntimeId) const;
	int32 GetDraftRuntimeId() const { return Draft.IsSet() ? Draft->RuntimeId : INDEX_NONE; }
	ECardTargetType GetDraftTargetType() const { return Draft.IsSet() ? Draft->TargetType : ECardTargetType::None; }
	bool TryGetDraftTarget(FName PresentationId, FBattleHUDTargetView& OutTarget) const;
	int32 GetConfirmedPlayCount() const { return ConfirmedPlays.Num(); }
	bool RestoreBusyPlay(const FQueuedCardPlayIntent& Intent);
	void RetireRejectedPlayAttempt() { bAwaitingSubmittedPlay = false; }
	FEndTurnIntentAvailability EvaluateEndTurnAvailability() const;
	FEndTurnIntentAcceptance TryAcceptEndTurn(bool bHasPendingFastInputRetry);
	bool TryCaptureCard(int32 RuntimeId);
	EBufferedPlayerIntentEvaluation EvaluatePending();
	bool TakeReadyIntent(FBufferedPlayerIntentDecision& OutDecision);
	EBufferedPlayerIntentKind GetPendingKind() const { return Pending.Kind; }

private:
	bool HasSafeBinding() const;
	bool TryGetEndTurnAuthority(FEndTurnIntentAvailability& OutAvailability) const;
	bool IsQueuedIdentityCurrent(const FQueuedCardPlayIntent& Intent) const;
	bool IsNormalSubmissionReady() const;

	TWeakObjectPtr<UBattleHUDWidgetBase> Owner;
	TWeakObjectPtr<UBattleHUDViewModel> ViewModel;
	TWeakObjectPtr<ABattleManager> Battle;
	TWeakObjectPtr<UBattlePresentationController> Controller;
	FBufferedPlayerIntentDecision Pending;
	uint64 NextIntentGeneration = 1;
	bool bEnabled = false;
	bool bConfirmedQueueMode = false;
	bool bAwaitingSubmittedPlay = false;
	uint64 BindingGeneration = 1;
	TOptional<FQueuedCardPlayIntent> Draft;
	TArray<FQueuedCardPlayIntent> ConfirmedPlays;
};
