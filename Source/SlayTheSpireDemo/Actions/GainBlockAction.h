#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "GainBlockAction.generated.h"

class ACombatant;

UCLASS()
class SLAYTHESPIREDEMO_API UGainBlockAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount);
	void SetPresentationParticipantIds(FName InSourcePresentationId, FName InTargetPresentationId);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Source = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Target = nullptr;

	int32 BaseAmount = 0;
	FName SourcePresentationId = NAME_None;
	FName TargetPresentationId = NAME_None;
};
