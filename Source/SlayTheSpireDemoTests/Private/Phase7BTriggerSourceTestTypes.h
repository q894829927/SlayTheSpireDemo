#pragma once

#include "CoreMinimal.h"
#include "Actions/BattleAction.h"
#include "Events/BattleTrigger.h"
#include "Phase7BTriggerSourceTestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase7BTestExecutionRecorder : public UObject
{
	GENERATED_BODY()

public:
	void Record(int32 Value);
	const TArray<int32>& GetValues() const;

private:
	TArray<int32> Values;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase7BTestRecordAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UPhase7BTestExecutionRecorder* InRecorder, int32 InValue);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase7BTestExecutionRecorder> Recorder = nullptr;

	int32 Value = 0;
};

UCLASS(Transient, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMOTESTS_API UPhase7BTestSourceTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	void Initialize(
		UPhase7BTestExecutionRecorder* InRecorder,
		int32 InValue,
		ETriggerRuntimeSourceKind InExpectedSourceKind,
		FName InExpectedSourceId
	);

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

private:
	bool MatchesExpectedSource(const FTriggerContext& Context) const;

	UPROPERTY(Transient)
	TObjectPtr<UPhase7BTestExecutionRecorder> Recorder = nullptr;

	int32 Value = 0;
	ETriggerRuntimeSourceKind ExpectedSourceKind = ETriggerRuntimeSourceKind::Status;
	FName ExpectedSourceId = NAME_None;
};
