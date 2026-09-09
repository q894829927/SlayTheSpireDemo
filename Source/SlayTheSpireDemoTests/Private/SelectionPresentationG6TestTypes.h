#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidgetBase.h"
#include "SelectionPresentationG6TestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG6ControllerWidget
	: public UBattleHUDWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) override;

	virtual bool BeginPresentationGroupPlayback(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token) override;

	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token) override;

	virtual void CancelPresentationGroupPlayback(
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token) override;

	void CompleteAcceptedGroup();
	void CompleteLastSingleRecord();

	bool bAcceptGroupPlayback = true;
	bool bAcceptFallbackSingleRecordPlayback = false;

	int32 RecordPlayCallCount = 0;
	int32 GroupPlayCallCount = 0;
	int32 RecordCancelCallCount = 0;
	int32 GroupCancelCallCount = 0;
	int32 AlreadyPresentedConsumeCount = 0;

	TArray<int32> HandCountsAtRecordOffer;
	TArray<int32> GroupRecordIndicesObserved;
	FPresentationPlaybackToken LastRecordToken;
	FPresentationPlaybackToken LastGroupToken;
	FPresentationGroupTag LastGroup;
};
