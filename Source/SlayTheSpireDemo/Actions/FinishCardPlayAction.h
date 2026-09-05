#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "FinishCardPlayAction.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UFinishCardPlayAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard);
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard, ACombatant* InPresentationCardSource);
	void Initialize(
		UDeckRuntime* InDeck,
		UCardInstance* InCard,
		ACombatant* InPresentationCardSource,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);
	virtual void Execute(UBattleActionQueue* Queue) override;

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
};
