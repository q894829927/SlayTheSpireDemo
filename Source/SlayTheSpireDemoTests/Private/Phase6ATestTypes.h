#pragma once

#include "CoreMinimal.h"
#include "Actions/BattleAction.h"
#include "Events/BattleTrigger.h"
#include "Phase6ATestTypes.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UDeckRuntime;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6ATestExecutionRecorder : public UObject
{
	GENERATED_BODY()

public:
	void Record(int32 Value);
	const TArray<int32>& GetValues() const;

private:
	TArray<int32> Values;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6ATestRecordAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UPhase6ATestExecutionRecorder* InRecorder, int32 InValue);
	void InitializeDeckDrawCount(UPhase6ATestExecutionRecorder* InRecorder, UDeckRuntime* InDeck);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6ATestExecutionRecorder> Recorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	int32 Value = 0;
	bool bRecordDeckDrawCount = false;
};

UCLASS(Transient, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMOTESTS_API UPhase6ATestRecordTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	void Initialize(UPhase6ATestExecutionRecorder* InRecorder, int32 InValue);
	void InitializeForDeckShuffled(UPhase6ATestExecutionRecorder* InRecorder, UDeckRuntime* InDeck);
	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6ATestExecutionRecorder> Recorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	int32 Value = 0;
	bool bReactToDeckShuffled = false;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6ATestEmitTurnEndedAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		UPhase6ATestExecutionRecorder* InRecorder,
		int32 InCommitValue,
		UBattleEventDispatcher* InDispatcher,
		ACombatant* InNestedTurnOwner,
		const TArray<ACombatant*>& InCombatants
	);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6ATestExecutionRecorder> Recorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> Dispatcher = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> NestedTurnOwner = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> Combatants;

	int32 CommitValue = 0;
};

UCLASS(Transient, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMOTESTS_API UPhase6ATestNestedTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	void Initialize(
		UPhase6ATestExecutionRecorder* InRecorder,
		UBattleEventDispatcher* InDispatcher,
		ACombatant* InNestedTurnOwner,
		const TArray<ACombatant*>& InCombatants,
		int32 InEmitValue,
		int32 InSiblingValue
	);
	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6ATestExecutionRecorder> Recorder = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> Dispatcher = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> NestedTurnOwner = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> Combatants;

	int32 EmitValue = 0;
	int32 SiblingValue = 0;
};
