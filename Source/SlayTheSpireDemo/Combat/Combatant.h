#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combatant.generated.h"

UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API ACombatant : public AActor
{
	GENERATED_BODY()

public:
	ACombatant();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Vitals", meta = (ClampMin = "1"))
	int32 MaxHP = 50;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Vitals")
	int32 HP = 50;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Vitals")
	int32 Block = 0;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void InitializeCombatant();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TakeCombatDamage(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void GainBlock(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearBlock();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const;
};
