#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "BattleHandFanPanel.generated.h"

// Formal Hand children retain frozen order and identity, including Hidden slots.
// Only layout/hover transforms and paint order belong to this panel.
UCLASS()
class SLAYTHESPIREDEMO_API UBattleHandFanPanel : public UCanvasPanel
{
	GENERATED_BODY()
public:
	void UpdateInteraction(const FVector2D& AbsolutePointer, int32 SelectedRuntimeId, bool bAllowHover, float DeltaTime);
	void LayoutCards();
	// Apply the final slot geometry and fan angles before an incoming card's
	// presentation starts. This keeps the target deterministic while the card
	// itself animates from its source pile.
	void PrepareIncomingCardLayout();
	void SetLayoutParameters(
		const FVector2D& InCardSize,
		float InMaxHorizontalStep,
		float InBaseVerticalOffset,
		float InEdgeVerticalDrop);
	int32 GetHoveredRuntimeId() const { return HoveredRuntimeId; }
	static FVector2D GetFanOffset(
		int32 Index,
		int32 Count,
		float Width,
		float MaxHorizontalStep = 100.0f,
		float BaseVerticalOffset = 24.0f,
		float EdgeVerticalDrop = 36.0f,
		float CardWidth = 150.0f);
	static float GetFanAngle(int32 Index, int32 Count);
protected:
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
private:
	int32 HoveredRuntimeId = INDEX_NONE;
	FVector2D CardSize = FVector2D(150.0f, 210.0f);
	float MaxHorizontalStep = 100.0f;
	float BaseVerticalOffset = 24.0f;
	float EdgeVerticalDrop = 36.0f;
};
