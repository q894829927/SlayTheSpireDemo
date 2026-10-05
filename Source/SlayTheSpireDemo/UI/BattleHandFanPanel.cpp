#include "BattleHandFanPanel.h"

#include "BattleCardWidget.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

// Private Slate implementation: UMG slots retain object identity even when the
// formal frozen order changes. Never infer that order from Slate insertion.
class SBattleHandFanPanel final : public SPanel
{
public:
	struct FSlot : TSlotBase<FSlot>
	{
		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>) SLATE_SLOT_END_ARGS()
		TWeakObjectPtr<UBattleHandFanSlot> Source;
	};
	SLATE_BEGIN_ARGS(SBattleHandFanPanel) {} SLATE_END_ARGS()
	SBattleHandFanPanel() : Children(this) {}
	void Construct(const FArguments&) {}
	void Add(UBattleHandFanSlot* Source)
	{
		FSlot::FSlotArguments Args(MakeUnique<FSlot>());
		Args.AttachWidget(Source->Content->TakeWidget());
		const int32 Index = Children.AddSlot(MoveTemp(Args));
		Children[Index].Source = Source;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
	void Remove(UBattleHandFanSlot* Source)
	{
		for (int32 Index = 0; Index < Children.Num(); ++Index)
			if (Children[Index].Source.Get() == Source) { Children.RemoveAt(Index); break; }
		Invalidate(EInvalidateWidgetReason::Layout);
	}
	void SetParameters(FVector2D Size, float Step, float Base, float Drop)
	{
		CardSize = Size; MaxStep = Step; BaseOffset = Base; EdgeDrop = Drop;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
	virtual FChildren* GetChildren() override { return &Children; }
	virtual FVector2D ComputeDesiredSize(float) const override { return CardSize; }
	virtual void OnArrangeChildren(const FGeometry& Geometry, FArrangedChildren& Arranged) const override
	{
		TArray<int32> Order;
		for (int32 Index = 0; Index < Children.Num(); ++Index)
			if (Children[Index].Source.IsValid()) Order.Add(Index);
		Order.Sort([this](int32 A, int32 B)
		{
			const UBattleHandFanSlot* Left = Children[A].Source.Get();
			const UBattleHandFanSlot* Right = Children[B].Source.Get();
			return Left->PaintLayer != Right->PaintLayer ? Left->PaintLayer < Right->PaintLayer : Left->FrozenIndex < Right->FrozenIndex;
		});
		for (int32 Index : Order)
		{
			UBattleHandFanSlot* HandSlot = Children[Index].Source.Get();
			FVector2D Size = CardSize;
			FVector2D Position = FVector2D(Geometry.GetLocalSize().X * 0.5f, Geometry.GetLocalSize().Y)
				+ UBattleHandFanPanel::GetFanOffset(HandSlot->FrozenIndex, Children.Num(), Geometry.GetLocalSize().X,
					MaxStep, BaseOffset, EdgeDrop, CardSize.X) - FVector2D(CardSize.X * 0.5f, CardSize.Y);
			if (HandSlot->bGeometryProtected)
			{
				// A just-attached incoming card has no cached geometry yet. Its
				// first real arrangement captures the exact allotted-size target.
				if (!HandSlot->bHasProtectedGeometry)
				{
					HandSlot->ProtectedPosition = Position; HandSlot->ProtectedSize = Size;
					HandSlot->bHasProtectedGeometry = true;
				}
				Position = HandSlot->ProtectedPosition; Size = HandSlot->ProtectedSize;
			}
			Arranged.AddWidget(Geometry.MakeChild(Children[Index].GetWidget(), Size, FSlateLayoutTransform(Position)));
		}
	}
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
	{
		FArrangedChildren Arranged(EVisibility::Visible);
		OnArrangeChildren(Geometry, Arranged);
		const FPaintArgs ChildArgs = Args.WithNewParent(this);
		int32 MaxLayer = Layer;
		for (int32 Index = 0; Index < Arranged.Num(); ++Index)
		{
			const FArrangedWidget& Child = Arranged[Index];
			if (!IsChildWidgetCulled(CullingRect, Child))
				MaxLayer = Child.Widget->Paint(ChildArgs, Child.Geometry, CullingRect, Elements,
					MaxLayer + 1, Style, ShouldBeEnabled(bEnabled));
		}
		return MaxLayer;
	}
private:
	TPanelChildren<FSlot> Children;
	FVector2D CardSize = FVector2D(150, 210);
	float MaxStep = 100;
	float BaseOffset = 24;
	float EdgeDrop = 36;
};

void UBattleHandFanSlot::SetPaintLayer(int32 Layer)
{
	if (PaintLayer == Layer) return;
	PaintLayer = Layer;
	if (Parent) Parent->InvalidateLayoutAndVolatility();
}

TSharedRef<SWidget> UBattleHandFanPanel::RebuildWidget()
{
	MyHandPanel = SNew(SBattleHandFanPanel);
	SynchronizeLayoutParameters();
	for (UPanelSlot* HandSlot : Slots)
		if (HandSlot->Content) MyHandPanel->Add(CastChecked<UBattleHandFanSlot>(HandSlot));
	return MyHandPanel.ToSharedRef();
}

void UBattleHandFanPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyHandPanel.Reset();
}

void UBattleHandFanPanel::SynchronizeLayoutParameters()
{
	if (MyHandPanel) MyHandPanel->SetParameters(CardSize, MaxHorizontalStep, BaseVerticalOffset, EdgeVerticalDrop);
}

void UBattleHandFanPanel::SetLayoutParameters(const FVector2D& InCardSize, float InStep, float InBase, float InDrop)
{
	CardSize = FVector2D(FMath::Max(InCardSize.X, 1.0f), FMath::Max(InCardSize.Y, 1.0f));
	MaxHorizontalStep = FMath::Max(InStep, 0.0f);
	BaseVerticalOffset = InBase;
	EdgeVerticalDrop = FMath::Max(InDrop, 0.0f);
	SynchronizeLayoutParameters();
	InvalidateLayoutAndVolatility();
}

void UBattleHandFanPanel::CommitFrozenOrder()
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		UBattleHandFanSlot* HandSlot = CastChecked<UBattleHandFanSlot>(Slots[Index]);
		HandSlot->FrozenIndex = Index;
		if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(HandSlot->Content))
		{
			Card->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			if (!HandSlot->bGeometryProtected && Card->GetRuntimeId() != HoveredRuntimeId && !InputVisualCards.Contains(Card->GetRuntimeId()))
			{
				FWidgetTransform Transform = Card->GetRenderTransform();
				Transform.Angle = GetFanAngle(Index, Slots.Num());
				Card->SetRenderTransform(Transform);
			}
		}
	}
	if (MyHandPanel) MyHandPanel->Invalidate(EInvalidateWidgetReason::Layout);
}

void UBattleHandFanPanel::OnSlotAdded(UPanelSlot* InSlot)
{
	UBattleHandFanSlot* HandSlot = CastChecked<UBattleHandFanSlot>(InSlot);
	HandSlot->FrozenIndex = Slots.Num() - 1;
	if (MyHandPanel && HandSlot->Content) MyHandPanel->Add(HandSlot);
}

void UBattleHandFanPanel::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (const UBattleCardWidget* Card = Cast<UBattleCardWidget>(InSlot->Content);
		Card && Card->GetRuntimeId() == HoveredRuntimeId) HoveredRuntimeId = INDEX_NONE;
	if (MyHandPanel) MyHandPanel->Remove(CastChecked<UBattleHandFanSlot>(InSlot));
}

bool UBattleHandFanPanel::ProtectCardGeometry(UWidget* Card, const FPresentationPlaybackToken& Token)
{
	UBattleHandFanSlot* HandSlot = Card ? Cast<UBattleHandFanSlot>(Card->Slot) : nullptr;
	if (!HandSlot || HandSlot->Parent != this || !Token.IsValid() || HandSlot->bGeometryProtected) return false;
	HandSlot->GeometryToken = Token;
	HandSlot->bHasProtectedGeometry = false;
	// Capture base geometry without the moving card's render transform.
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	if (!Size.IsNearlyZero())
	{
		TArray<TPair<UWidget*, FGeometry>> Arranged;
		GetArrangedCardGeometries(FGeometry::MakeRoot(Size, FSlateLayoutTransform()), Arranged);
		for (const auto& Entry : Arranged)
			if (Entry.Key == Card)
			{
				HandSlot->ProtectedPosition = FVector2D(Entry.Value.GetAccumulatedLayoutTransform().GetTranslation());
				HandSlot->ProtectedSize = FVector2D(Entry.Value.GetLocalSize());
				HandSlot->bHasProtectedGeometry = true;
				break;
			}
	}
	HandSlot->bGeometryProtected = true;
	return true;
}

void UBattleHandFanPanel::GetArrangedCardGeometries(const FGeometry& AllottedGeometry,
	TArray<TPair<UWidget*, FGeometry>>& OutCards) const
{
	OutCards.Reset();
	if (!MyHandPanel) return;
	FArrangedChildren Arranged(EVisibility::All);
	MyHandPanel->ArrangeChildren(AllottedGeometry, Arranged);
	for (int32 Index = 0; Index < Arranged.Num(); ++Index)
		for (UPanelSlot* HandSlot : Slots)
			if (HandSlot->Content && HandSlot->Content->GetCachedWrappedWidget() == Arranged[Index].Widget)
			{
				OutCards.Emplace(HandSlot->Content.Get(), Arranged[Index].Geometry);
				break;
			}
}

bool UBattleHandFanPanel::ReleaseCardGeometry(UWidget* Card, const FPresentationPlaybackToken& Token)
{
	UBattleHandFanSlot* HandSlot = Card ? Cast<UBattleHandFanSlot>(Card->Slot) : nullptr;
	if (!HandSlot || HandSlot->Parent != this || !HandSlot->bGeometryProtected || HandSlot->GeometryToken != Token) return false;
	HandSlot->bGeometryProtected = HandSlot->bHasProtectedGeometry = false;
	HandSlot->GeometryToken = FPresentationPlaybackToken{};
	if (MyHandPanel) MyHandPanel->Invalidate(EInvalidateWidgetReason::Layout);
	return true;
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

void UBattleHandFanPanel::UpdateHoverAffordance(const FVector2D& AbsolutePointer, int32 SelectedRuntimeId, bool bAllowHover, float DeltaTime)
{
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
		if (!Card || !Card->IsVisible() || !Card->GetIsEnabled() || InputVisualCards.Contains(Card->GetRuntimeId()) || CastChecked<UBattleHandFanSlot>(Card->Slot)->IsGeometryProtected()) continue;
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
				Card && Card->IsVisible() && Card->GetIsEnabled() && !InputVisualCards.Contains(Card->GetRuntimeId()) && !CastChecked<UBattleHandFanSlot>(Card->Slot)->IsGeometryProtected() && Card->GetRuntimeId() == PreviousHover)
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
		if (!Card || !Card->IsVisible() || !Card->GetIsEnabled() || InputVisualCards.Contains(Card->GetRuntimeId()) || CastChecked<UBattleHandFanSlot>(Card->Slot)->IsGeometryProtected()) continue;
		const bool bRaised = Card->IsVisible() && Card->GetIsEnabled()
			&& (Card->GetRuntimeId() == HoveredRuntimeId || Card->GetRuntimeId() == SelectedRuntimeId);
		CastChecked<UBattleHandFanSlot>(Card->Slot)->SetPaintLayer(bRaised ? 1 : 0);
		FWidgetTransform Transform = Card->GetRenderTransform();
		Transform.Angle = FMath::Lerp(Transform.Angle, bRaised ? 0.0f : GetFanAngle(Index, GetChildrenCount()), Alpha);
		Transform.Scale = FMath::Lerp(Transform.Scale, FVector2D(bRaised ? 1.35f : 1.0f), Alpha);
		Transform.Translation = FMath::Lerp(Transform.Translation, FVector2D(0.0f, bRaised ? -72.0f : 0.0f), Alpha);
		Card->SetRenderTransform(Transform);
	}
}

void UBattleHandFanPanel::SetInputVisualCards(const TSet<int32>& RuntimeIds)
{
	for (int32 Index = 0; Index < GetChildrenCount(); ++Index)
		if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(GetChildAt(Index));
			Card && InputVisualCards.Contains(Card->GetRuntimeId()) && !RuntimeIds.Contains(Card->GetRuntimeId()))
		{
			UBattleHandFanSlot* HandSlot = CastChecked<UBattleHandFanSlot>(Card->Slot);
			if (!HandSlot->IsGeometryProtected())
			{
				FWidgetTransform Rest;
				Rest.Angle = GetFanAngle(Index, GetChildrenCount());
				Card->SetRenderTransform(Rest);
				HandSlot->SetPaintLayer(0);
				if (Card->GetVisibility() == ESlateVisibility::HitTestInvisible) Card->SetVisibility(ESlateVisibility::Visible);
			}
		}
	InputVisualCards = RuntimeIds;
}

bool UBattleHandFanPanel::GetCardVisualGeometry(UWidget* Card, FGeometry& OutGeometry) const
{
	if (GetCachedGeometry().GetLocalSize().IsNearlyZero()) return false;
	TArray<TPair<UWidget*, FGeometry>> Arranged;
	GetArrangedCardGeometries(GetCachedGeometry(), Arranged);
	for (const auto& Entry : Arranged)
		if (Entry.Key == Card) { OutGeometry = Entry.Value; return true; }
	return false;
}

bool UBattleHandFanPanel::MoveInputVisualTo(int32 RuntimeId, const FVector2D& AbsoluteCenter)
{
	if (!InputVisualCards.Contains(RuntimeId)) return false;
	for (int32 Index = 0; Index < GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(GetChildAt(Index));
		if (!Card || Card->GetRuntimeId() != RuntimeId || !Card->IsVisible()
			|| CastChecked<UBattleHandFanSlot>(Card->Slot)->IsGeometryProtected()) continue;
		const FGeometry& ParentGeometry = GetCachedGeometry();
		if (ParentGeometry.GetLocalSize().IsNearlyZero()) return false;
		const FVector2D BaseBottom = FVector2D(ParentGeometry.GetLocalSize().X * 0.5f, ParentGeometry.GetLocalSize().Y)
			+ GetFanOffset(Index, GetChildrenCount(), ParentGeometry.GetLocalSize().X,
				MaxHorizontalStep, BaseVerticalOffset, EdgeVerticalDrop, CardSize.X);
		FWidgetTransform Transform;
		Transform.Scale = FVector2D(1.15f);
		// Hand cards pivot at bottom-center. Align the *rendered* center to the
		// pointer without touching the slot or feeding back cached transforms.
		Transform.Translation = FVector2D(ParentGeometry.AbsoluteToLocal(AbsoluteCenter))
			- BaseBottom + FVector2D(0, CardSize.Y * Transform.Scale.Y * 0.5f);
		Card->SetRenderTransform(Transform);
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		CastChecked<UBattleHandFanSlot>(Card->Slot)->SetPaintLayer(2);
		return true;
	}
	return false;
}
