#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "BattleTrigger.generated.h"

class ABattleManager;
class ACombatant;
class UBattleAction;
class URelicInstance;
class UStatusInstance;
struct FBattleEvent;

enum class ETriggerRuntimeSourceKind : uint8
{
	Status,
	Relic
};

struct SLAYTHESPIREDEMO_API FTriggerRuntimeSource
{
	ETriggerRuntimeSourceKind Kind = ETriggerRuntimeSourceKind::Status;
	UObject* RuntimeObject = nullptr;
	FName SourceId = NAME_None;
	uint64 RuntimeSequence = 0;
	ACombatant* CombatantOwner = nullptr;

	static FTriggerRuntimeSource FromStatus(UStatusInstance* Instance);
	static FTriggerRuntimeSource FromRelic(URelicInstance* Instance);
};

struct SLAYTHESPIREDEMO_API FTriggerContext
{
public:
	FTriggerContext(
		UStatusInstance* InRuntimeSource,
		UObject* InActionOuter,
		const FPresentationRecordWriter& InPresentationRecordWriter = FPresentationRecordWriter{}
	);

	FTriggerContext(
		UStatusInstance* InRuntimeSource,
		UObject* InActionOuter,
		ABattleManager* InBattle,
		const FPresentationRecordWriter& InPresentationRecordWriter = FPresentationRecordWriter{}
	);

	FTriggerContext(
		const FTriggerRuntimeSource& InRuntimeSource,
		UObject* InActionOuter,
		ABattleManager* InBattle,
		const FPresentationRecordWriter& InPresentationRecordWriter = FPresentationRecordWriter{}
	);

	UObject* GetRuntimeSourceObject() const;
	UStatusInstance* GetRuntimeSource() const;
	URelicInstance* GetRelicSource() const;
	ETriggerRuntimeSourceKind GetSourceKind() const;
	FName GetSourceId() const;
	uint64 GetRuntimeSequence() const;
	ACombatant* GetOwner() const;
	UObject* GetActionOuter() const;
	ABattleManager* GetBattle() const;
	const FPresentationRecordWriter& GetPresentationRecordWriter() const;

private:
	FTriggerRuntimeSource RuntimeSource;
	UObject* ActionOuter = nullptr;
	ABattleManager* Battle = nullptr;
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
