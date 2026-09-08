#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "RandomSelectionAction.h"
#include "DeferredSelectionAction.generated.h"

class USelectionCandidateSource;
class USelectionResolver;
class UAuthoredContinuation;

enum class EDeferredSelectionMode : uint8 { Player, Random };
enum class ESelectionCountPolicy : uint8 { ExactNClampToAvailable };
DECLARE_DELEGATE_RetVal_OneParam(FPresentationRecordWriter, FSelectionInteractiveBoundaryAccess, const UBattleAction*);

// Captures current candidates once, then dispatches to Player or Random execution.
UCLASS()
class SLAYTHESPIREDEMO_API UDeferredSelectionAction : public UBattleAction
{
	GENERATED_BODY()
public:
	void Initialize(USelectionCandidateSource* InSource, USelectionResolver* InResolver,
		UAuthoredContinuation* InContinuation, int32 InCount, ESelectionCancelPolicy InCancelPolicy,
		FName InSelectionSource, EDeferredSelectionMode InMode = EDeferredSelectionMode::Player,
		FSelectionInteractiveBoundaryAccess InBoundary = FSelectionInteractiveBoundaryAccess(),
		FSelectionRandomIndexChooser InRandom = FSelectionRandomIndexChooser());
	virtual void Execute(UBattleActionQueue* Queue) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<USelectionCandidateSource> CandidateSource;
	UPROPERTY(Transient)
	TObjectPtr<USelectionResolver> Resolver;
	UPROPERTY(Transient)
	TObjectPtr<UAuthoredContinuation> Continuation;
	int32 RequestedCount = 0;
	ESelectionCountPolicy CountPolicy = ESelectionCountPolicy::ExactNClampToAvailable;
	ESelectionCancelPolicy CancelPolicy = ESelectionCancelPolicy::Forbidden;
	EDeferredSelectionMode Mode = EDeferredSelectionMode::Player;
	FName SelectionSource;
	FSelectionInteractiveBoundaryAccess BoundaryAccess;
	FSelectionRandomIndexChooser RandomChooser;
};
