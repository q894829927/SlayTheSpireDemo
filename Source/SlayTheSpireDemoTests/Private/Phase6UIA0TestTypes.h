#pragma once

#include "CoreMinimal.h"
#include "Actions/BattleAction.h"
#include "Phase6UIA0TestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA0ManualFinishAction : public UBattleAction
{
	GENERATED_BODY()

public:
	virtual void Execute(UBattleActionQueue* Queue) override;
	void CompleteManually();
	bool HasExecuted() const;

private:
	bool bExecuted = false;
};
