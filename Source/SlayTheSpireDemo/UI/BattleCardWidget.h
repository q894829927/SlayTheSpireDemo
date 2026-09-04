#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "../Battle/BattleImmediatePreview.h"
#include "BattleCardWidget.generated.h"

class UButton;
class UImage;
class URichTextBlock;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBattleCardRequested,
	int32,
	RuntimeId
);

/**
 * Native card presentation/input boundary for the A2N migration.
 *
 * The Widget owns only the supplied FBattleHUDCardView and optional transient A3
 * card-face Preview presentation. It never queries Gameplay, BattleManager or
 * PresentationController and never treats Preview values as authoritative state.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Card")
	void SetCardView(const FBattleHUDCardView& View);

	// A3 card-face presentation only. The DTO is already resolved by Gameplay.
	// This changes only the visible description/value styling of this card.
	void ApplyImmediatePreview(const FImmediateCardPreview& Preview);
	void ClearImmediatePreview();

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	int32 GetRuntimeId() const { return CurrentCardView.RuntimeId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	FName GetCardId() const { return CurrentCardView.CardId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	FBattleHUDCardView GetCardView() const { return CurrentCardView; }

#if WITH_DEV_AUTOMATION_TESTS
	// Semantic branch probe only. Rendering now comes from per-value RichText tags.
	int8 GetImmediatePreviewToneForTesting() const { return ImmediatePreviewTone; }
#endif

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Card")
	FOnBattleCardRequested OnBattleCardRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshFromCardView();

	UFUNCTION()
	void HandleCardClicked();

	// Presentation-only style. The authored card name text itself never changes
	// when upgraded; the upgraded state is communicated by color.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle HUD|Card|Style")
	FLinearColor UpgradedNameColor = FLinearColor(1.0f, 0.72f, 0.0f, 1.0f);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Card;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CardName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Cost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> Txt_CardDescription;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CardType;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_CardArt;

private:
	// Use a native-only name distinct from the duplicated Legacy Blueprint's
	// retained `CardView` member variable. R4 takes runtime ownership without
	// requiring a binary asset edit solely to remove that inert migration residue.
	UPROPERTY(Transient)
	FBattleHUDCardView CurrentCardView;

	FSlateColor DefaultCardNameColor;
	bool bDefaultCardNameColorCaptured = false;
	bool bNativeBindingsValid = false;
	bool bCardDelegateBound = false;
	bool bImmediatePreviewApplied = false;
	// -1 = at least one decrease and no increase, 0 = neutral, +1 = any increase.
	// This is test-only semantic evidence; RichText may style multiple values independently.
	int8 ImmediatePreviewTone = 0;
};
