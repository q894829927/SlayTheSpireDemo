#include "BattleCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

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
	}
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
	RefreshFromCardView();
}

void UBattleCardWidget::RefreshFromCardView()
{
	// Keep SetCardView order-independent for tests and future presentation-only
	// creation. NativeOnInitialized still owns the fail-closed runtime contract;
	// this helper simply refuses to touch a partial Designer surface.
	if (!IsValid(Txt_CardName)
		|| !IsValid(Txt_Cost)
		|| !IsValid(Txt_CardDescription)
		|| !IsValid(Txt_CardType)
		|| !IsValid(Img_CardArt))
	{
		return;
	}

	Txt_CardName->SetText(CurrentCardView.DisplayName);
	Txt_Cost->SetText(FText::AsNumber(CurrentCardView.Cost));
	Txt_CardDescription->SetText(CurrentCardView.Description);

	if (const UEnum* CardTypeEnum = StaticEnum<ECardType>())
	{
		Txt_CardType->SetText(
			CardTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(CurrentCardView.CardType)));
	}
	else
	{
		Txt_CardType->SetText(FText::GetEmpty());
	}

	Img_CardArt->SetBrushFromTexture(CurrentCardView.CardArt.Get());
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
