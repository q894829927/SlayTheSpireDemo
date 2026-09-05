#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "../Deck/DeckMutationTypes.h"
#include "ExhaustCardAction.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UExhaustCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		UDeckRuntime* InDeck,
		UCardInstance* InCard,
		ACombatant* InPresentationCardSource,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
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

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> EventCombatants;

	FCardZoneMutationResult CommitResult;
};
