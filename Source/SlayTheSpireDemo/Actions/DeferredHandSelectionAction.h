#pragma once
#include "CoreMinimal.h"
#include "DeferredSelectionAction.h"
#include "DeferredHandSelectionAction.generated.h"
class UDeckRuntime;

// Compatibility adapter only. Production Effects use DeferredSelectionAction.
UCLASS()
class SLAYTHESPIREDEMO_API UDeferredHandSelectionAction : public UDeferredSelectionAction
{
    GENERATED_BODY()
public:
    void Initialize(UDeckRuntime* InDeck, USelectionResolver* InResolver,
        UAuthoredContinuation* InContinuation, int32 InRequestedCount, FName InSelectionSource);
};
