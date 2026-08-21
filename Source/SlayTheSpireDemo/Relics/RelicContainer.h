#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RelicRuntimeTypes.h"
#include "RelicContainer.generated.h"

class ABattleManager;
class URelicData;
class URelicInstance;

UCLASS()
class SLAYTHESPIREDEMO_API URelicContainer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABattleManager* InBattle);
	void Reset();

	FRelicAddResult AddRelic(URelicData* Definition);
	const URelicInstance* FindRelicById(FName RelicId) const;
	bool ContainsRelic(FName RelicId) const;
	bool ContainsRelicInstance(const URelicInstance* Instance) const;
	const TArray<TObjectPtr<URelicInstance>>& GetRelics() const;
	ABattleManager* GetBattle() const;

private:
	URelicInstance* FindMutableRelicById(FName RelicId) const;

	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URelicInstance>> Relics;
};
