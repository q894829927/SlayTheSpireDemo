#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDSelectionWidget.h"
#include "CardSelectionPresentationTestTypes.generated.h"

class UBattleHUDViewModel;
class UHorizontalBox;
class UOverlay;
class UTextBlock;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UCardSelectionPresentationHUDProbe
	: public UBattleHUDSelectionWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void ConfigureSelectionSurfaces(
		UBattleHUDViewModel* InViewModel,
		UHorizontalBox* InHand,
		UOverlay* InPlayArea,
		UTextBlock* InDrawCount,
		UTextBlock* InDiscardCount,
		UTextBlock* InExhaustCount);
	void InvokeNativeTickForTesting(float DeltaSeconds);
	void BindConfirmButtonForTesting(class UButton* Button);
	void FinishNativeForTesting(const FPresentationPlaybackToken& Token)
	{
		FinishNativePresentation(Token);
	}

	virtual UWorld* GetWorld() const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
