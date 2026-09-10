#include "BattleTargetingArrowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

UBattleTargetingArrowWidget::UBattleTargetingArrowWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Head(TEXT("/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleArrow.T_reticleArrow"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Body(TEXT("/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleBlock.T_reticleBlock"));
	ArrowTexture = Head.Object;
	SegmentTexture = Body.Object;
}

bool UBattleTargetingArrowWidget::ShouldShow(const FBattleHUDCardView& Card, EBattleHUDInteractionState State, bool bInputLocked, bool bPendingSelection)
{
	return Card.RuntimeId != INDEX_NONE && Card.CardType == ECardType::Attack
		&& Card.TargetType == ECardTargetType::Enemy && State == EBattleHUDInteractionState::ChoosingTarget
		&& !bInputLocked && !bPendingSelection;
}

bool UBattleTargetingArrowWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	// Native-only visual construction must also work without a LocalPlayer.
	// UE may omit NativeOnInitialized in that case (headless/direct HUD hosts).
	if (ArrowCanvas) return true;
	ArrowCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = ArrowCanvas;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		UImage* Segment = WidgetTree->ConstructWidget<UImage>();
		Segment->SetBrushFromTexture(SegmentTexture);
		Segment->SetVisibility(ESlateVisibility::HitTestInvisible);
		Segment->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		ArrowCanvas->AddChildToCanvas(Segment)->SetAlignment(FVector2D(0.5f, 0.5f));
		Segments.Add(Segment);
	}
	ArrowHead = WidgetTree->ConstructWidget<UImage>();
	ArrowHead->SetBrushFromTexture(ArrowTexture);
	ArrowHead->SetVisibility(ESlateVisibility::HitTestInvisible);
	// The source texture contains transparent padding; anchor its actual tip.
	ArrowHead->SetRenderTransformPivot(FVector2D(0.5f, 0.41f));
	UCanvasPanelSlot* HeadSlot = ArrowCanvas->AddChildToCanvas(ArrowHead);
	HeadSlot->SetAlignment(FVector2D(0.5f, 0.41f));
	HeadSlot->SetSize(FVector2D(180.0f));
	return true;
}

void UBattleTargetingArrowWidget::SetAim(const FVector2D& Start, const FVector2D& End, bool bLegalEnemy)
{
	if (!ArrowHead || !ArrowTexture || !SegmentTexture) return;
	const FLinearColor Color = bLegalEnemy ? FLinearColor(0.9f, 0.035f, 0.075f, 1.0f) : FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
	const float Distance = FVector2D::Distance(Start, End);
	const FVector2D P1 = Start + FVector2D(0.0f, -FMath::Clamp(Distance * 0.65f, 80.0f, 280.0f));
	const FVector2D P2 = End + FVector2D((End.X >= Start.X ? -1.0f : 1.0f) * FMath::Min(Distance * 0.25f, 100.0f), -35.0f);
	auto Point = [&](float T)
	{
		const float U = 1.0f - T;
		return Start * U * U * U + P1 * 3.0f * U * U * T + P2 * 3.0f * U * T * T + End * T * T * T;
	};
	// Sample arc length so the segments stay evenly spaced on a curved arrow.
	TArray<FVector2D, TInlineAllocator<65>> Points;
	TArray<float, TInlineAllocator<65>> Lengths;
	float Length = 0.0f;
	for (int32 I = 0; I <= 64; ++I)
	{
		const FVector2D P = Point(I / 64.0f);
		if (I > 0) Length += FVector2D::Distance(P, Points.Last());
		Points.Add(P);
		Lengths.Add(Length);
	}
	const int32 Count = FMath::Clamp(FMath::FloorToInt(Length / 22.0f), 3, Segments.Num());
	for (int32 I = 0; I < Segments.Num(); ++I)
	{
		UImage* Segment = Segments[I];
		Segment->SetVisibility(I < Count ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (I >= Count) continue;
		const float Along = FMath::Max(0.0f, Length - 48.0f) * I / FMath::Max(Count - 1, 1);
		int32 Sample = 1;
		while (Sample < 64 && Lengths[Sample] < Along) ++Sample;
		const float Fraction = (Along - Lengths[Sample - 1]) / FMath::Max(Lengths[Sample] - Lengths[Sample - 1], KINDA_SMALL_NUMBER);
		const FVector2D Position = FMath::Lerp(Points[Sample - 1], Points[Sample], Fraction);
		const FVector2D Tangent = Points[Sample] - Points[Sample - 1];
		const float Scale = FMath::Lerp(0.7f, 1.0f, I / static_cast<float>(Count));
		UCanvasPanelSlot* CanvasSlot = CastChecked<UCanvasPanelSlot>(Segment->Slot);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(FVector2D(64.0f, 49.0f) * Scale);
		Segment->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)) + 90.0f);
		Segment->SetColorAndOpacity(Color);
	}
	const FVector2D Tangent = End - P2;
	CastChecked<UCanvasPanelSlot>(ArrowHead->Slot)->SetPosition(End);
	ArrowHead->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)) + 90.0f);
	ArrowHead->SetColorAndOpacity(Color);
}
