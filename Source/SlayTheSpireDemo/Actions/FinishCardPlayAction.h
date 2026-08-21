#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "FinishCardPlayAction.generated.h"

class ACombatant;
class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UFinishCardPlayAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard);
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard, ACombatant* InPresentationCardSource);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> PresentationCardSource = nullptr;
};
