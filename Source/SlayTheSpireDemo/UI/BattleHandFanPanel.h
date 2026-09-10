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
	int32 GetHoveredRuntimeId() const { return HoveredRuntimeId; }
	static FVector2D GetFanOffset(int32 Index, int32 Count, float Width);
	static float GetFanAngle(int32 Index, int32 Count);
protected:
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
private:
	int32 HoveredRuntimeId = INDEX_NONE;
};
