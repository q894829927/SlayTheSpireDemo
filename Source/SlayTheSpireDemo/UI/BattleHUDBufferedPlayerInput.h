#pragma once

#include "CoreMinimal.h"
#include "../Presentation/PresentationG9Types.h"

class ABattleManager;
class UBattleHUDWidgetBase;
class UBattleHUDViewModel;
class UBattlePresentationController;

enum class EBufferedPlayerIntentKind : uint8 { None, CardSelection, EndTurn };
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
};

struct SLAYTHESPIREDEMO_API FEndTurnIntentAcceptance
{
	bool bAccepted = false;
	bool bRetireCardIntent = false;
	bool bRetireFastInputRetry = false;
	bool bCancelTransientSelection = false;
};

// One value-owned input arbiter per HUD. G9-A invokes it only in Automation;
// it returns shadow decisions and never calls SelectCard/Request/Skip/Cancel.
class SLAYTHESPIREDEMO_API FBattleHUDBufferedPlayerInput
{
public:
	void Bind(UBattleHUDWidgetBase* InOwner);
	void SetEnabled(bool bInEnabled);
	void Clear();
	FEndTurnIntentAvailability EvaluateEndTurnAvailability() const;
	FEndTurnIntentAcceptance TryAcceptEndTurn(bool bHasPendingFastInputRetry);
	bool TryCaptureCard(int32 RuntimeId);
	EBufferedPlayerIntentEvaluation EvaluatePending();
	bool TakeReadyIntent(FBufferedPlayerIntentDecision& OutDecision);
	EBufferedPlayerIntentKind GetPendingKind() const { return Pending.Kind; }

private:
	bool HasSafeBinding() const;
	bool TryGetEndTurnAuthority(FEndTurnIntentAvailability& OutAvailability) const;

	TWeakObjectPtr<UBattleHUDWidgetBase> Owner;
	TWeakObjectPtr<UBattleHUDViewModel> ViewModel;
	TWeakObjectPtr<ABattleManager> Battle;
	TWeakObjectPtr<UBattlePresentationController> Controller;
	FBufferedPlayerIntentDecision Pending;
	uint64 NextIntentGeneration = 1;
	bool bEnabled = false;
};
