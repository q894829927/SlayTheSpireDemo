#include "DeferredHandSelectionAction.h"
#include "../Selection/SelectionCandidateSource.h"

void UDeferredHandSelectionAction::Initialize(UDeckRuntime* InDeck, USelectionResolver* InResolver,
    UAuthoredContinuation* InContinuation, int32 InRequestedCount, FName InSelectionSource)
{
    UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(this);
    Source->Initialize(InDeck);
    UDeferredSelectionAction::Initialize(Source, InResolver, InContinuation, InRequestedCount,
        ESelectionCancelPolicy::Forbidden, InSelectionSource);
}
