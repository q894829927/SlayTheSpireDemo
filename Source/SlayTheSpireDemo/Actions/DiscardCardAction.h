#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DiscardCardAction.generated.h"

class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UDiscardCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, int32 InRuntimeId);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	int32 RuntimeId = INDEX_NONE;
};
