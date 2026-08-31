#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleStatusWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;

/**
 * Native frozen-view boundary for one formal battle Status row.
 *
 * The Widget owns only the supplied FBattleHUDStatusView and Designer rendering.
 * Status lifecycle, exact target identity and committed-history sequencing remain
 * owned by UBattleHUDWidget / Presentation. It never queries Gameplay objects.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Status")
	void SetStatusView(const FBattleHUDStatusView& View);

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Status")
	FBattleHUDStatusView GetStatusView() const { return NativeStatusView; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Status")
	FName GetStatusId() const { return NativeStatusView.StatusId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Status")
	int64 GetRuntimeSequence() const { return NativeStatusView.RuntimeSequence; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_StatusIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_StatusAmount;

private:
	void SetAtlasVector2D(FName ParameterName, const FVector2D& Value);

	// Native-only names intentionally avoid the duplicated Legacy Blueprint's
	// retained StatusView / CurrentStatusView / MID_StatusIcon migration residue.
	UPROPERTY(Transient)
	FBattleHUDStatusView NativeStatusView;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NativeStatusIconMID = nullptr;

	bool bNativeBindingsValid = false;
};
