#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleTargetingArrowWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;

// Private, hit-test-invisible aiming affordance; never submits a request.
UCLASS()
class SLAYTHESPIREDEMO_API UBattleTargetingArrowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UBattleTargetingArrowWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;
	void SetAim(const FVector2D& Start, const FVector2D& End, bool bLegalEnemy);
	static bool ShouldShow(const FBattleHUDCardView& Card, EBattleHUDInteractionState State, bool bInputLocked, bool bPendingSelection);
private:
	UPROPERTY() TObjectPtr<UTexture2D> ArrowTexture;
	UPROPERTY() TObjectPtr<UTexture2D> SegmentTexture;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ArrowCanvas;
	UPROPERTY(Transient) TObjectPtr<UImage> ArrowHead;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Segments;
};
