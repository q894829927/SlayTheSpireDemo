#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "BattleTrigger.generated.h"

class ACombatant;
class UBattleAction;
class UStatusInstance;
struct FBattleEvent;

struct SLAYTHESPIREDEMO_API FTriggerContext
{
public:
	FTriggerContext(
		UStatusInstance* InRuntimeSource,
		UObject* InActionOuter,
		const FPresentationRecordWriter& InPresentationRecordWriter = FPresentationRecordWriter{}
	);

	UStatusInstance* GetRuntimeSource() const;
	ACombatant* GetOwner() const;
	UObject* GetActionOuter() const;
	const FPresentationRecordWriter& GetPresentationRecordWriter() const;

private:
	UStatusInstance* RuntimeSource = nullptr;
	ACombatant* Owner = nullptr;
	UObject* ActionOuter = nullptr;
	FPresentationRecordWriter PresentationRecordWriter;
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UBattleTrigger : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	int32 Priority = 0;

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const;
};
