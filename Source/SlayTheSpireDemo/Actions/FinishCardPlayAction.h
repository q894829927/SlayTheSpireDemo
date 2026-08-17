#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "FinishCardPlayAction.generated.h"

class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UFinishCardPlayAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, UCardInstance* InCard);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;
};
