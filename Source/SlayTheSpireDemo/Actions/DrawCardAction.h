#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DrawCardAction.generated.h"

class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UDrawCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;
};
