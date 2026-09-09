#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "../Presentation/SelectionPresentationMetadata.h"
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

	// Optional G1 context for one direct Selection continuation. This is never
	// propagated by ordinary writer inheritance, so reaction Actions remain
	// ungrouped unless an explicit future contract says otherwise.
	void SetSelectionPresentationActionContext(const FSelectionPresentationActionContext& InContext)
	{
		SelectionPresentationActionContext = InContext;
	}

	void ClearSelectionPresentationActionContext()
	{
		SelectionPresentationActionContext = FSelectionPresentationActionContext{};
	}

	bool TryGetSelectionPresentationGroupForRuntimeId(
		int32 RuntimeId,
		FPresentationGroupTag& OutGroup
	) const
	{
		OutGroup = FPresentationGroupTag{};
		if (RuntimeId == INDEX_NONE || !SelectionPresentationActionContext.IsValid()
			|| !SelectionPresentationActionContext.CanonicalSelectedRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
		OutGroup = SelectionPresentationActionContext.Group;
		return true;
	}

	FOnBattleActionFinished OnFinished;

protected:
	void Finish();

private:
	bool bIsFinished = false;
	FPresentationRecordWriter PresentationRecordWriter;
	FSelectionPresentationActionContext SelectionPresentationActionContext;
};
