#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "ShuffleDeckAction.generated.h"

class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UShuffleDeckAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;
};
