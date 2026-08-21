#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "PresentationTypes.h"
#include "BattlePresentationController.generated.h"

class ABattleManager;
class UBattleHUDViewModel;
class UBattleHUDWidgetBase;

UCLASS(Transient, BlueprintType)
class SLAYTHESPIREDEMO_API UBattlePresentationController : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(
		ABattleManager* InBattleManager,
		UBattleHUDViewModel* InViewModel,
		UBattleHUDWidgetBase* InWidget
	);

	void Shutdown();
	void SetWidget(UBattleHUDWidgetBase* InWidget);

	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void NotifyPresentationFinished(const FPresentationPlaybackToken& Token);

	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void SkipPresentation();

	void NotifyWidgetLost(UBattleHUDWidgetBase* LostWidget);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Presentation", meta = (ClampMin = "0.05"))
	float PlaybackTimeoutSeconds = 3.0f;

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetBacklogCountForTesting() const;
	bool IsWaitingForCompletionForTesting() const;
	FPresentationPlaybackToken GetActivePlaybackTokenForTesting() const;
	int64 GetLastCompletedResolutionIdForTesting() const;
	void ExpireActivePlaybackForTesting();
#endif

protected:
	virtual void BeginDestroy() override;

private:
	void HandlePresentationResolutionReady(const FPresentationResolutionEnvelope& Envelope);
	void StartNextEnvelope();
	void StartNextRecord();
	void CompleteActiveRecord();
	void CompleteActiveEnvelope();
	void CollapseToEnvelope(const FPresentationResolutionEnvelope& Envelope);
	void CancelActiveTimeout();
	void ScheduleActiveTimeout();
	bool HandleActiveTimeout(float DeltaTime);
	void AdvancePlaybackGeneration();
	bool IsEnvelopeForCurrentBattle(const FPresentationResolutionEnvelope& Envelope) const;

	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDWidgetBase> Widget = nullptr;

	UPROPERTY(Transient)
	TArray<FPresentationResolutionEnvelope> PlaybackQueue;

	UPROPERTY(Transient)
	FPresentationResolutionEnvelope ActiveEnvelope;

	bool bHasActiveEnvelope = false;
	bool bWaitingForCompletion = false;
	int32 ActiveRecordIndex = INDEX_NONE;
	int64 CurrentBattleId = 0;
	int64 LastQueuedResolutionId = 0;
	int64 LastCompletedResolutionId = 0;
	int64 LocalPlaybackGeneration = 1;
	FPresentationPlaybackToken ActivePlaybackToken;
	FPresentationPlaybackToken ScheduledTimeoutToken;
	FTSTicker::FDelegateHandle PlaybackTimeoutTickerHandle;

	static constexpr int32 MaxPlaybackEnvelopes = 8;
};