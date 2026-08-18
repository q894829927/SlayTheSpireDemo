#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Events/BattleTrigger.h"
#include "../Modifiers/Block/BlockModifier.h"
#include "../Modifiers/Damage/DamageModifier.h"
#include "StatusData.generated.h"

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UStatusData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Identity")
	FName StatusId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Modifiers|Damage")
	TArray<TObjectPtr<UDamageModifier>> DamageModifiers;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Modifiers|Block")
	TArray<TObjectPtr<UBlockModifier>> BlockModifiers;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Triggers")
	TArray<TObjectPtr<UBattleTrigger>> Triggers;
};
