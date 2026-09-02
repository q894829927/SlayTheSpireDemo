#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RelicInstance.generated.h"

class ABattleManager;
class URelicContainer;
class URelicData;

UCLASS()
class SLAYTHESPIREDEMO_API URelicInstance : public UObject
{
	GENERATED_BODY()

public:
	URelicData* GetDefinition() const;
	FName GetRelicId() const;
	uint64 GetRuntimeSequence() const;
	ABattleManager* GetBattle() const;
	FString GetDebugLabel() const;

private:
	friend class URelicContainer;

	void Initialize(URelicData* InDefinition, ABattleManager* InBattle, uint64 InRuntimeSequence);

	UPROPERTY(Transient)
	TObjectPtr<URelicData> Definition = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	uint64 RuntimeSequence = 0;
};
