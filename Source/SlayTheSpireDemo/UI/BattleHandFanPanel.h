#pragma once

#include "CoreMinimal.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleHandFanPanel.generated.h"

class SBattleHandFanPanel;

UCLASS()
class SLAYTHESPIREDEMO_API UBattleHandFanSlot : public UPanelSlot
{
	GENERATED_BODY()
public:
	int32 GetFrozenIndex() const { return FrozenIndex; }
	int32 GetPaintLayer() const { return PaintLayer; }
	void SetPaintLayer(int32 Layer);
	bool IsGeometryProtected() const { return bGeometryProtected; }
private:
	friend class UBattleHandFanPanel;
	friend class SBattleHandFanPanel;
	int32 FrozenIndex = 0;
	int32 PaintLayer = 0;
	bool bGeometryProtected = false;
	bool bHasProtectedGeometry = false;
	FPresentationPlaybackToken GeometryToken;
	FVector2D ProtectedPosition = FVector2D::ZeroVector;
	FVector2D ProtectedSize = FVector2D::ZeroVector;
};

// HUD commits membership/order. This panel arranges the current descriptor on
// the first Slate pass; no cached-size layout or interaction Tick is required.
UCLASS()
class SLAYTHESPIREDEMO_API UBattleHandFanPanel : public UPanelWidget
{
	GENERATED_BODY()
public:
	void CommitFrozenOrder();
	void UpdateHoverAffordance(const FVector2D& AbsolutePointer, int32 SelectedRuntimeId, bool bAllowHover, float DeltaTime);
	void SetLayoutParameters(const FVector2D& InCardSize, float InMaxHorizontalStep,
		float InBaseVerticalOffset, float InEdgeVerticalDrop);
	bool ProtectCardGeometry(UWidget* Card, const FPresentationPlaybackToken& Token);
	bool ReleaseCardGeometry(UWidget* Card, const FPresentationPlaybackToken& Token);
	// Read arranged geometry in paint order, including Hidden historical slots.
	// Animation/interaction can inspect the panel without becoming layout writers.
	void GetArrangedCardGeometries(const FGeometry& AllottedGeometry,
		TArray<TPair<UWidget*, FGeometry>>& OutCards) const;
	int32 GetHoveredRuntimeId() const { return HoveredRuntimeId; }
	static FVector2D GetFanOffset(int32 Index, int32 Count, float Width,
		float MaxHorizontalStep = 100.0f, float BaseVerticalOffset = 24.0f,
		float EdgeVerticalDrop = 36.0f, float CardWidth = 150.0f);
	static float GetFanAngle(int32 Index, int32 Count);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual UClass* GetSlotClass() const override { return UBattleHandFanSlot::StaticClass(); }
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
private:
	void SynchronizeLayoutParameters();
	int32 HoveredRuntimeId = INDEX_NONE;
	FVector2D CardSize = FVector2D(150.0f, 210.0f);
	float MaxHorizontalStep = 100.0f;
	float BaseVerticalOffset = 24.0f;
	float EdgeVerticalDrop = 36.0f;
	TSharedPtr<SBattleHandFanPanel> MyHandPanel;
};
