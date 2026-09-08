#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DeferredHandSelectionAction.generated.h"

class UAuthoredContinuation;
class UDeckRuntime;
class USelectionResolver;

// Execution-time current-Hand candidate provider for one mandatory exact-N
// authored selection. It knows where candidates come from, not how the selected
// cards will be consumed by the continuation.
UCLASS()
class SLAYTHESPIREDEMO_API UDeferredHandSelectionAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		UDeckRuntime* InDeck,
		USelectionResolver* InResolver,
		UAuthoredContinuation* InContinuation,
		int32 InRequestedCount,
		FName InSelectionSource
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USelectionResolver> Resolver = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAuthoredContinuation> Continuation = nullptr;

	int32 RequestedCount = 0;
	FName SelectionSource = NAME_None;
};
