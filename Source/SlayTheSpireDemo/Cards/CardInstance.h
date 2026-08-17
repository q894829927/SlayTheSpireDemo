#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardTypes.h"
#include "CardInstance.generated.h"

class UCardData;

UCLASS()
class SLAYTHESPIREDEMO_API UCardInstance : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UCardData* InDefinition, int32 InRuntimeId);

	const UCardData* GetDefinition() const;
	int32 GetRuntimeId() const;
	FName GetCardId() const;
	int32 GetCurrentCost() const;
	ECardTargetType GetTargetType() const;
	ECardDestination ResolveDestination() const;
	FString GetDebugLabel() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCardData> Definition = nullptr;

	UPROPERTY(Transient)
	int32 RuntimeId = INDEX_NONE;
};
