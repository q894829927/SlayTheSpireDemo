#pragma once

#include "CoreMinimal.h"
#include "AuthoredContinuation.h"
#include "ExhaustSelectedContinuation.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UCardInstance;
class UDeckRuntime;

// Concrete authored Continuation for the "select one or more Hand cards, then
// exhaust them" composition. It is immutable/stateless: it holds only stable
// battle-scoped wiring captured at BuildActions time, never resolution-local
// mutable state.
//
// BuildNextActions validates all selected objects first, then builds one
// UExhaustCardAction per selected UCardInstance in result order. It does not
// decide what happens after the exhaust batch; later authored card Effects keep
// that responsibility.
UCLASS(NotBlueprintable)
class SLAYTHESPIREDEMO_API UExhaustSelectedContinuation : public UAuthoredContinuation
{
	GENERATED_BODY()

public:
	void Initialize(
		UDeckRuntime* InDeck,
		ACombatant* InPresentationCardSource,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);

	virtual bool BuildNextActions(
		const FSelectionResult& Result,
		UBattleActionQueue* Queue,
		TArray<UBattleAction*>& OutActions
	) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> PresentationCardSource = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> EventCombatants;
};
