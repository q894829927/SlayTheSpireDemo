#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "../Deck/DeckMutationTypes.h"
#include "MoveHandCardToDrawPileTopAction.generated.h"

class ACombatant;
class UCardInstance;
class UDeckRuntime;

// Exact authoritative Hand -> DrawPile-top movement. This Action owns only the
// requested exact CardInstance mutation and committed Presentation fact; it does
// not choose cards, shuffle, inspect CardId rules, or know a concrete card.
UCLASS()
class SLAYTHESPIREDEMO_API UMoveHandCardToDrawPileTopAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		UDeckRuntime* InDeck,
		UCardInstance* InCard,
		ACombatant* InPresentationCardSource
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

	const FCardZoneMutationResult& GetCommitResult() const
	{
		return CommitResult;
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> PresentationCardSource = nullptr;

	FCardZoneMutationResult CommitResult;
};
