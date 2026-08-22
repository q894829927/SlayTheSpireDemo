#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidgetBase.h"
#include "Phase6UIA2D5TestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2D5PlaybackWidget : public UBattleHUDWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	) override;

	void ResetCapture();

	bool bAcceptAsyncPlayback = true;
	int32 PlayCallCount = 0;
	TArray<FPresentationRecord> PlayedRecords;
	TArray<FPresentationPlaybackToken> PlayedTokens;
};
