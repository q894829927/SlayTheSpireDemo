#pragma once

#include "CoreMinimal.h"
#include "AuthoredContinuation.h"
#include "MoveSelectedHandCardsToDrawPileTopContinuation.generated.h"

class ACombatant;
class UDeckRuntime;

// Stateless authored continuation for moving one or more selected Hand cards to
// DrawPile top. Result order is preserved; each card becomes one exact Action.
UCLASS(NotBlueprintable)
class SLAYTHESPIREDEMO_API UMoveSelectedHandCardsToDrawPileTopContinuation : public UAuthoredContinuation
{
	GENERATED_BODY()

public:
	void Initialize(
		UDeckRuntime* InDeck,
		ACombatant* InPresentationCardSource
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
};
