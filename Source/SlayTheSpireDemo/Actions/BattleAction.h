#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "BattleAction.generated.h"

class UBattleAction;
class UBattleActionQueue;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleActionFinished, UBattleAction*);

UCLASS(Abstract)
class SLAYTHESPIREDEMO_API UBattleAction : public UObject
{
	GENERATED_BODY()

public:
	virtual void Execute(UBattleActionQueue* Queue);

	bool IsFinished() const
	{
		return bIsFinished;
	}

	void SetPresentationRecordWriter(const FPresentationRecordWriter& InWriter)
	{
		PresentationRecordWriter = InWriter;
	}

	const FPresentationRecordWriter& GetPresentationRecordWriter() const
	{
		return PresentationRecordWriter;
	}

	FOnBattleActionFinished OnFinished;

protected:
	void Finish();

private:
	bool bIsFinished = false;
	FPresentationRecordWriter PresentationRecordWriter;
};
