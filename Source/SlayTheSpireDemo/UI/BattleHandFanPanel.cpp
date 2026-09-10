#include "BattleHandFanPanel.h"

#include "BattleCardWidget.h"
#include "Components/CanvasPanelSlot.h"

void UBattleHandFanPanel::SetLayoutParameters(
	const FVector2D& InCardSize,
	float InMaxHorizontalStep,
	float InBaseVerticalOffset,
	float InEdgeVerticalDrop)
{
	CardSize = FVector2D(
		FMath::Max(InCardSize.X, 1.0f),
		FMath::Max(InCardSize.Y, 1.0f));
	MaxHorizontalStep = FMath::Max(InMaxHorizontalStep, 0.0f);
	BaseVerticalOffset = InBaseVerticalOffset;
	EdgeVerticalDrop = FMath::Max(InEdgeVerticalDrop, 0.0f);
	LayoutCards();
}

FVector2D UBattleHandFanPanel::GetFanOffset(
	int32 Index,
	int32 Count,
	float Width,
	float MaxHorizontalStep,
	float BaseVerticalOffset,
	float EdgeVerticalDrop,
	float CardWidth)
{
	if (Count <= 0) return FVector2D::ZeroVector;
	const float Centered = Index - (Count - 1) * 0.5f;
	const float Step = Count > 1
		? FMath::Min(MaxHorizontalStep, FMath::Max(Width - (CardWidth + 60.0f), 0.0f) / (Count - 1))
		: 0.0f;
	const float Normalized = Count > 1 ? Centered / ((Count - 1) * 0.5f) : 0.0f;
	return FVector2D(Centered * Step, BaseVerticalOffset + EdgeVerticalDrop * Normalized * Normalized);
}

float UBattleHandFanPanel::GetFanAngle(int32 Index, int32 Count)
{
	const float Half = (Count - 1) * 0.5f;
	return Count > 1 ? (Index - Half) / Half * FMath::Min(18.0f, Half * 5.0f) : 0.0f;
}

void UBattleHandFanPanel::LayoutCards()
{
	const float ActualWidth = GetCachedGeometry().GetLocalSize().X;
	const float Width = ActualWidth > KINDA_SMALL_NUMBER ? ActualWidth : 900.0f;
	for (int32 Index = 0; Index < GetChildrenCount(); ++Index)
	{
		UWidget* Card = GetChildAt(Index);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Card->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetSize(CardSize);
			CanvasSlot->SetPosition(GetFanOffset(
				Index,
				GetChildrenCount(),
				Width,
				MaxHorizontalStep,
				BaseVerticalOffset,
				EdgeVerticalDrop,
				CardSize.X));
			Card->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		}
	}
}

void UBattleHandFanPanel::PrepareIncomingCardLayout()
{
	LayoutCards();
	const int32 FinalCount = GetChildrenCount();
	for (int32 Index = 0; Index < FinalCount; ++Index)
	{
		if (UWidget* Card = GetChildAt(Index))
		{
			FWidgetTransform Transform = Card->GetRenderTransform();
			Transform.Angle = GetFanAngle(Index, FinalCount);
			Card->SetRenderTransform(Transform);
		}
	}
}

void UBattleHandFanPanel::OnSlotAdded(UPanelSlot* InSlot)
{
	Super::OnSlotAdded(InSlot);
	LayoutCards();
}

void UBattleHandFanPanel::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (const UBattleCardWidget* Card = Cast<UBattleCardWidget>(InSlot->Content);
		Card && Card->GetRuntimeId() == HoveredRuntimeId) HoveredRuntimeId = INDEX_NONE;
	Super::OnSlotRemoved(InSlot);
	LayoutCards();
}

void UBattleHandFanPanel::UpdateInteraction(const FVector2D& AbsolutePointer, int32 SelectedRuntimeId, bool bAllowHover, float DeltaTime)
{
	LayoutCards();
	const FGeometry& Geometry = GetCachedGeometry();
	if (Geometry.GetLocalSize().IsNearlyZero()) return;
	const FVector2D Pointer = Geometry.AbsoluteToLocal(AbsolutePointer);
	const int32 PreviousHover = HoveredRuntimeId;
	HoveredRuntimeId = INDEX_NONE;
	float Closest = TNumericLimits<float>::Max();
	// Resting horizontal strips determine which overlapping card is inspected.
	// Enlarging a card therefore cannot steal its neighbour's hover region.
	for (int32 Index = 0; bAllowHover && Index < GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(GetChildAt(Index));
		if (!Card || !Card->IsVisible() || !Card->GetIsEnabled()) continue;
		const FVector2D Bottom = FVector2D(Geometry.GetLocalSize().X * 0.5f, Geometry.GetLocalSize().Y)
			+ GetFanOffset(
				Index,
				GetChildrenCount(),
				Geometry.GetLocalSize().X,
				MaxHorizontalStep,
				BaseVerticalOffset,
				EdgeVerticalDrop,
				CardSize.X);
		const float Distance = FMath::Abs(Pointer.X - Bottom.X);
		if (Distance <= CardSize.X * 0.5f && Pointer.Y >= Bottom.Y - CardSize.Y - 15.0f
			&& Pointer.Y <= Bottom.Y + 15.0f && Distance < Closest)
		{
			Closest = Distance;
			HoveredRuntimeId = Card->GetRuntimeId();
		}
	}
	if (bAllowHover && HoveredRuntimeId == INDEX_NONE)
	{
		for (UWidget* Child : GetAllChildren())
			if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child);
				Card && Card->IsVisible() && Card->GetIsEnabled() && Card->GetRuntimeId() == PreviousHover)
			{
				const FGeometry& CardGeometry = Card->GetCachedGeometry();
				const FVector2D Local = CardGeometry.AbsoluteToLocal(AbsolutePointer);
				if (Local.X >= 0.0 && Local.Y >= 0.0 && Local.X <= CardGeometry.GetLocalSize().X
					&& Local.Y <= CardGeometry.GetLocalSize().Y) HoveredRuntimeId = PreviousHover;
			}
	}
	const float Alpha = 1.0f - FMath::Exp(-18.0f * FMath::Max(DeltaTime, 0.0f));
	for (int32 Index = 0; Index < GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(GetChildAt(Index));
		if (!Card) continue;
		const bool bRaised = Card->IsVisible() && Card->GetIsEnabled()
			&& (Card->GetRuntimeId() == HoveredRuntimeId || Card->GetRuntimeId() == SelectedRuntimeId);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Card->Slot)) CanvasSlot->SetZOrder(bRaised ? 1000 + Index : Index);
		FWidgetTransform Transform = Card->GetRenderTransform();
		Transform.Angle = FMath::Lerp(Transform.Angle, bRaised ? 0.0f : GetFanAngle(Index, GetChildrenCount()), Alpha);
		Transform.Scale = FMath::Lerp(Transform.Scale, FVector2D(bRaised ? 1.35f : 1.0f), Alpha);
		Transform.Translation = FMath::Lerp(Transform.Translation, FVector2D(0.0f, bRaised ? -72.0f : 0.0f), Alpha);
		Card->SetRenderTransform(Transform);
	}
}
