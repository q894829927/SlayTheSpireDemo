#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatantMutationTypes.h"
#include "Combatant.generated.h"

class UStatusContainer;

UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API ACombatant : public AActor
{
	GENERATED_BODY()

public:
	ACombatant();

	// Stable presentation identity for UI inspection/target mapping. It is not a
	// gameplay ordering key and must not be used to determine combat resolution.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Presentation")
	FName PresentationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Presentation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Vitals", meta = (ClampMin = "1"))
	int32 MaxHP = 50;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Vitals")
	int32 HP = 50;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Vitals")
	int32 Block = 0;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void InitializeCombatant();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	FDamageCommitResult TakeCombatDamage(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	FBlockCommitResult GainBlock(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	FBlockCommitResult ClearBlock();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const;

	UStatusContainer* GetStatusContainer() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStatusContainer> StatusContainer = nullptr;
};
