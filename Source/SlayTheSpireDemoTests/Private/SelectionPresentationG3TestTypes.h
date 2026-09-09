#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidgetBase.h"
#include "SelectionPresentationG3TestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG3PlaybackWidget
	: public UBattleHUDWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	) override;

	virtual bool BeginPresentationGroupPlayback(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token
	) override;

	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token
	) override;

	virtual void CancelPresentationGroupPlayback(
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token
	) override;

	bool bAcceptAsyncRecordPlayback = true;
	bool bAcceptAsyncGroupPlayback = true;
	bool bNotifySynchronouslyFromRecordPlay = false;
	bool bNotifySynchronouslyFromGroupPlay = false;

	int32 RecordPlayCallCount = 0;
	int32 GroupPlayCallCount = 0;
	int32 RecordCancelCallCount = 0;
	int32 GroupCancelCallCount = 0;

	FPresentationPlaybackToken LastRecordToken;
	FPresentationPlaybackToken LastGroupToken;
	FPresentationPlaybackToken LastCancelledRecordToken;
	FPresentationPlaybackToken LastCancelledGroupToken;
	FPresentationGroupTag LastGroup;
	FPresentationGroupTag LastCancelledGroup;
};