#pragma once

#include "CoreMinimal.h"
#include "Actions/BattleAction.h"
#include "Cards/Effects/CardEffect.h"
#include "Events/BattleTrigger.h"
#include "UI/BattleHUDPresenter.h"
#include "UI/BattleHUDWidgetBase.h"
#include "Phase6UIA2ATestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2AProbeState : public UObject
{
	GENERATED_BODY()

public:
	void RecordContextWriter(const FPresentationRecordWriter& Writer);
	void RecordActionWriter(const FPresentationRecordWriter& Writer);

	bool bContextWriterAvailable = false;
	bool bActionWriterAvailable = false;
	uint64 ContextBattleId = 0;
	uint64 ContextResolutionId = 0;
	uint64 ActionBattleId = 0;
	uint64 ActionResolutionId = 0;
	int32 ActionExecutionCount = 0;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2AProbeAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UPhase6UIA2AProbeState* InState, bool bInAppendRecord);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6UIA2AProbeState> State = nullptr;

	bool bAppendRecord = false;
};

UCLASS(Transient, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2AProbeCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	void Initialize(UPhase6UIA2AProbeState* InState, bool bInAppendRecord = false);
	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildPreviewArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6UIA2AProbeState> State = nullptr;

	bool bAppendRecord = false;
};

UCLASS(Transient, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2AProbeTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	void Initialize(UPhase6UIA2AProbeState* InState);
	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhase6UIA2AProbeState> State = nullptr;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2APlaybackWidget : public UBattleHUDWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool PlayPresentationRecord_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	) override;
	virtual void CancelPresentationRecordPlayback_Implementation() override;

	bool bAcceptAsyncPlayback = true;
	bool bNotifySynchronouslyFromPlay = false;
	int32 PlayCallCount = 0;
	int32 CancelCallCount = 0;
	FPresentationPlaybackToken LastToken;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API APhase6UIA2ATestPresenter : public ABattleHUDPresenter
{
	GENERATED_BODY()

public:
	bool InvokeInitializeHUDForTesting(APlayerController* PlayerController);
	void InvokeShutdownHUDForTesting();
};
