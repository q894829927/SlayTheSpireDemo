#include "BattleCardWidget.h"

#include "CardFaceStyleSet.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	FText GetCardTypeDisplayText(ECardType CardType)
	{
		switch (CardType)
		{
		case ECardType::Attack:
			return NSLOCTEXT("BattleCardWidget", "CardType_Attack", "攻击");
		case ECardType::Skill:
			return NSLOCTEXT("BattleCardWidget", "CardType_Skill", "技能");
		case ECardType::Power:
			return NSLOCTEXT("BattleCardWidget", "CardType_Power", "能力");
		case ECardType::Status:
			return NSLOCTEXT("BattleCardWidget", "CardType_Status", "状态");
		case ECardType::Curse:
			return NSLOCTEXT("BattleCardWidget", "CardType_Curse", "诅咒");
		default:
			return FText::GetEmpty();
		}
	}

	FText GetVisibleCardName(const FBattleHUDCardView& View)
	{
		if (!View.bUpgraded || View.DisplayName.IsEmpty())
		{
			return View.DisplayName;
		}

		return FText::Format(
			NSLOCTEXT("BattleCardWidget", "UpgradedCardNameFormat", "{0}+"),
			View.DisplayName
		);
	}

	const FText& GetVisibleCardDescription(const FBattleHUDCardView& View)
	{
		return View.RichDescription.IsEmpty() ? View.Description : View.RichDescription;
	}

	void ClearImage(UImage* Image, bool bClearCanvasPlacement)
	{
		if (!IsValid(Image))
		{
			return;
		}

		// Clear only the resource object through UMG. Constructing an FSlateBrush
		// directly would make the runtime module depend on SlateCore solely for
		// this presentation cleanup path.
		Image->SetBrushResourceObject(nullptr);
		Image->SetVisibility(ESlateVisibility::Hidden);

		if (bClearCanvasPlacement)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetPosition(FVector2D::ZeroVector);
				CanvasSlot->SetSize(FVector2D::ZeroVector);
			}
		}
	}

	void ApplyTextureRegion(UImage* Image, const FCardFaceTextureRegion& Region)
	{
		if (!IsValid(Image))
		{
			return;
		}

		UTexture2D* Texture = Region.Texture.Get();
		const bool bValidPlacement =
			Region.Placement.Size.X > KINDA_SMALL_NUMBER
			&& Region.Placement.Size.Y > KINDA_SMALL_NUMBER;
		if (!IsValid(Texture) || !bValidPlacement)
		{
			ClearImage(Image, true);
			return;
		}

		Image->SetBrushFromTexture(Texture, false);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetPosition(Region.Placement.Position);
			CanvasSlot->SetSize(Region.Placement.Size);
		}
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UBattleCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid =
		IsValid(Btn_Card)
		&& IsValid(Txt_CardName)
		&& IsValid(Txt_Cost)
		&& IsValid(Txt_CardDescription)
		&& IsValid(Txt_CardType)
		&& IsValid(Img_CardArt);

	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Card '%s' has a missing required BindWidget control."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleCard][Native] Invalid required bindings on '%s'; card input is disabled."),
			*GetPathName());
		return;
	}

	DefaultCardNameColor = Txt_CardName->GetColorAndOpacity();
	bDefaultCardNameColorCaptured = true;
}

void UBattleCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bNativeBindingsValid)
	{
		return;
	}

	Btn_Card->OnClicked.AddUniqueDynamic(this, &UBattleCardWidget::HandleCardClicked);
	bCardDelegateBound = true;

	// CreateWidget constructs the Designer before the HUD supplies its DTO. Keep
	// Construct order irrelevant by refreshing again here; SetCardView also
	// refreshes immediately when the real frozen/current view arrives later.
	RefreshFromCardView();
}

void UBattleCardWidget::NativeDestruct()
{
	if (bCardDelegateBound && IsValid(Btn_Card))
	{
		Btn_Card->OnClicked.RemoveDynamic(this, &UBattleCardWidget::HandleCardClicked);
		bCardDelegateBound = false;
	}

	Super::NativeDestruct();
}

void UBattleCardWidget::SetCardView(const FBattleHUDCardView& View)
{
	CurrentCardView = View;
	bImmediatePreviewApplied = false;
	ImmediatePreviewTone = 0;
	RefreshFromCardView();
}

void UBattleCardWidget::ApplyImmediatePreview(const FImmediateCardPreview& Preview)
{
	const FText& PreviewDescription = Preview.CardFaceRichDescription.IsEmpty()
		? Preview.CardFaceDescription
		: Preview.CardFaceRichDescription;
	if (!IsValid(Txt_CardDescription)
		|| Preview.CardRuntimeId == INDEX_NONE
		|| Preview.CardRuntimeId != CurrentCardView.RuntimeId
		|| !Preview.Validation.bAllowed
		|| PreviewDescription.IsEmpty())
	{
		ClearImmediatePreview();
		return;
	}

	bool bHasIncrease = false;
	bool bHasDecrease = false;
	for (const FImmediatePreviewOperation& Operation : Preview.Operations)
	{
		if (Operation.ResolvedAmount > Operation.BaseAmount)
		{
			bHasIncrease = true;
		}
		else if (Operation.ResolvedAmount < Operation.BaseAmount)
		{
			bHasDecrease = true;
		}
	}

	// RichText style tags are already attached to the exact semantic values by
	// BattleTextResolver. The Widget does not parse numbers or recolor the sentence.
	Txt_CardDescription->SetText(PreviewDescription);
	ImmediatePreviewTone = bHasIncrease ? 1 : (bHasDecrease ? -1 : 0);
	bImmediatePreviewApplied = true;
}

void UBattleCardWidget::ClearImmediatePreview()
{
	if (!bImmediatePreviewApplied)
	{
		ImmediatePreviewTone = 0;
		return;
	}
	bImmediatePreviewApplied = false;
	ImmediatePreviewTone = 0;

	if (IsValid(Txt_CardDescription))
	{
		Txt_CardDescription->SetText(GetVisibleCardDescription(CurrentCardView));
	}
}

void UBattleCardWidget::RefreshFromCardView()
{
	// Keep SetCardView order-independent for tests and future presentation-only
	// creation. NativeOnInitialized still owns the fail-closed runtime contract;
	// this helper simply refuses to touch a partial core Designer surface.
	if (!IsValid(Txt_CardName)
		|| !IsValid(Txt_Cost)
		|| !IsValid(Txt_CardDescription)
		|| !IsValid(Txt_CardType)
		|| !IsValid(Img_CardArt))
	{
		return;
	}

	// Gameplay keeps the authored DisplayName stable. Only the presentation layer
	// appends '+' for an upgraded runtime instance.
	Txt_CardName->SetText(GetVisibleCardName(CurrentCardView));
	if (bDefaultCardNameColorCaptured)
	{
		Txt_CardName->SetColorAndOpacity(
			CurrentCardView.bUpgraded
				? FSlateColor(UpgradedNameColor)
				: DefaultCardNameColor);
	}

	Txt_Cost->SetText(FText::AsNumber(CurrentCardView.Cost));
	Txt_CardDescription->SetText(GetVisibleCardDescription(CurrentCardView));
	ImmediatePreviewTone = 0;

	// Card type is player-facing localized UI text. Do not expose the native enum
	// DisplayName (Attack/Skill/...) directly or overwrite Designer Chinese labels
	// with editor-facing English metadata during a native refresh.
	Txt_CardType->SetText(GetCardTypeDisplayText(CurrentCardView.CardType));

	RefreshCardArtwork();
	RefreshVisualStyle();
}

void UBattleCardWidget::RefreshCardArtwork()
{
	if (!IsValid(Img_CardArt))
	{
		return;
	}

	UTexture2D* CardArt = CurrentCardView.CardArt.Get();
	if (!IsValid(CardArt))
	{
		ClearImage(Img_CardArt, false);
		return;
	}

	Img_CardArt->SetBrushFromTexture(CardArt, false);
	Img_CardArt->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBattleCardWidget::RefreshVisualStyle()
{
	if (!IsValid(CardFaceStyleSet))
	{
		ClearDecorativeVisualStyle();
		return;
	}

	const FResolvedCardFaceStyle Resolved = ResolveCardFaceStyle(
		CurrentCardView.CardColor,
		CurrentCardView.CardType,
		CurrentCardView.Rarity,
		CardFaceStyleSet->Config);

	ApplyTextureRegion(Img_CardBackground, Resolved.BackgroundRegion);
	ApplyTextureRegion(Img_CardFrame, Resolved.FrameRegion);
	ApplyTextureRegion(Img_CardBanner, Resolved.BannerRegion);
	ApplyTextureRegion(Img_CostOrb, Resolved.CostOrbRegion);
}

void UBattleCardWidget::ClearDecorativeVisualStyle()
{
	ClearImage(Img_CardBackground, true);
	ClearImage(Img_CardFrame, true);
	ClearImage(Img_CardBanner, true);
	ClearImage(Img_CostOrb, true);
}

void UBattleCardWidget::HandleCardClicked()
{
	// Formal Hand cards are allowed to request selection even when their current
	// frozen bGameplayPlayable value is false; the formal ViewModel request path
	// owns the authoritative playability check and feedback. Presentation-only
	// cards are instead never bound by the HUD and are HitTestInvisible when they
	// are introduced by later playback phases.
	if (CurrentCardView.RuntimeId != INDEX_NONE)
	{
		OnBattleCardRequested.Broadcast(CurrentCardView.RuntimeId);
	}
}
