#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DiscardCardAction.generated.h"

class ACombatant;
class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UDiscardCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard);
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard, ACombatant* InPresentationCardSource);
	void SetTurnEndDiscardPresentationGroup(const FPresentationGroupTag& Group, int32 RuntimeId);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	FPresentationGroupTag TurnEndDiscardGroup;
	int32 TurnEndDiscardRuntimeId = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> PresentationCardSource = nullptr;
};
