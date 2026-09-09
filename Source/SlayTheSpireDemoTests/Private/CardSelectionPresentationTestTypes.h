#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDSelectionWidget.h"
#include "UI/BattleHUDViewModel.h"
#include "Cards/Effects/CardEffect.h"
#include "CardSelectionPresentationTestTypes.generated.h"

class UBattleHUDViewModel;
class UHorizontalBox;
class UOverlay;
class UTextBlock;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionNoDestinationEffectProbe : public UCardEffect
{
	GENERATED_BODY()
public:
	virtual void BuildActions(const FCardPlayContext& Context, TArray<UBattleAction*>& OutActions) const override;
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const override {}
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override {}
	virtual void BuildPreviewArguments(const FCardEffectPreviewContext& Context, FPreviewTextArgumentBuilder& OutArguments) const override {}
	virtual FText GetAutoDescriptionFormat(const FCardEffectPreviewContext& Context) const override
	{
		return NSLOCTEXT("G5Tests", "NoDestination", "选择一张手牌（无后续效果）。");
	}
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationSubmitProbe : public UBattleHUDViewModel
{
	GENERATED_BODY()
public:
	bool bReject = false;
	TFunction<void()> DuringSubmit;
	TFunction<void()> AfterSubmit;
	virtual bool SubmitPendingCardSelectionByRuntimeIds(const TArray<int32>& Ids) override
	{
		if (DuringSubmit) DuringSubmit();
		const bool bResult = !bReject && Super::SubmitPendingCardSelectionByRuntimeIds(Ids);
		if (AfterSubmit) AfterSubmit();
		return bResult;
	}
};

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
	void ConfigureSelectionCanvasForTesting(UBattleHUDViewModel* InViewModel);
	void DisableSyntheticSelectionGeometryForTesting() { bSyntheticSelectionGeometry = false; }
	void DestructSelectionForTesting() { NativeDestruct(); }
	UHorizontalBox* GetHandForTesting() const { return HB_Hand; }
	UOverlay* GetPlayAreaForTesting() const { return OV_PlayArea; }
	void FinishNativeForTesting(const FPresentationPlaybackToken& Token)
	{
		FinishNativePresentation(Token);
	}

	virtual UWorld* GetWorld() const override;

protected:
	virtual FVector2D GetSelectionAreaLayoutSize() const override
	{
		return bSyntheticSelectionGeometry ? FVector2D(1280.0f, 720.0f) : Super::GetSelectionAreaLayoutSize();
	}

private:
	bool bSyntheticSelectionGeometry = false;
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
