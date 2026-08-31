#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleStatusWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "BattleHUDWidget"

namespace
{
	constexpr float NativePresentationDurationSeconds = 0.5f;
	constexpr float NativeDrawCardStartScale = 0.82f;
	const FVector2D NativeDrawCardFallbackTranslation(-420.0f, 90.0f);
	const FVector2D NativeHandCardFallbackTranslation(-300.0f, 120.0f);
	const FVector2D NativeDiscardCardFallbackTranslation(420.0f, 90.0f);
}

void UBattleHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid =
		CardWidgetClass != nullptr
		&& StatusWidgetClass != nullptr
		&& IsValid(Combatant_PlayerPresentation)
		&& IsValid(Combatant_EnemyPresentation)
		&& IsValid(HB_Hand)
		&& IsValid(Btn_EndTurn)
		&& IsValid(Btn_Confirm)
		&& IsValid(Btn_Cancel)
		&& IsValid(Txt_Feedback)
		&& IsValid(OV_PlayArea)
		&& IsValid(Txt_DamagePresentation)
		&& IsValid(Overlay_Terminal)
		&& IsValid(PB_PlayerHP)
		&& IsValid(Txt_PlayerHP)
		&& IsValid(Txt_PlayerBlock)
		&& IsValid(WB_PlayerStatuses)
		&& IsValid(PB_EnemyHP)
		&& IsValid(Txt_EnemyHP)
		&& IsValid(Txt_EnemyBlock)
		&& IsValid(WB_EnemyStatuses)
		&& IsValid(Txt_DrawCount)
		&& IsValid(Txt_DiscardCount)
		&& IsValid(Txt_ExhaustCount)
		&& IsValid(Txt_Energy)
		&& IsValid(Txt_Outcome);

	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle HUD '%s' has a missing dynamic Widget class or required BindWidget control."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleHUD][Native] Invalid required bindings on '%s'; Native input and playback are disabled."),
			*GetPathName());
	}
}

void UBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bNativeBindingsValid)
	{
		return;
	}

	// These are long-lived UI request bindings. AddUniqueDynamic keeps a
	// repeated construct from accumulating callbacks, while NativeDestruct
	// below owns the matching removal boundary.
	Btn_EndTurn->OnClicked.AddUniqueDynamic(this, &UBattleHUDWidget::HandleEndTurnClicked);
	Btn_Confirm->OnClicked.AddUniqueDynamic(this, &UBattleHUDWidget::HandleConfirmClicked);
	Btn_Cancel->OnClicked.AddUniqueDynamic(this, &UBattleHUDWidget::HandleCancelClicked);

	Combatant_PlayerPresentation->OnTargetRequested.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantTargetRequested);
	Combatant_EnemyPresentation->OnTargetRequested.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantTargetRequested);
	Combatant_PlayerPresentation->OnInspectRequested.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantInspectRequested);
	Combatant_EnemyPresentation->OnInspectRequested.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantInspectRequested);
	Combatant_PlayerPresentation->OnInspectCleared.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantInspectCleared);
	Combatant_EnemyPresentation->OnInspectCleared.AddUniqueDynamic(
		this,
		&UBattleHUDWidget::HandleCombatantInspectCleared);

	bNativeDelegatesBound = true;

	// SetViewModel can arrive before NativeConstruct in the Presenter. Pull the
	// current frozen state once after bindings are ready so the initial Native
	// surface cannot depend on callback ordering.
	RefreshHUDFromViewModel();
}

void UBattleHUDWidget::NativeDestruct()
{
	// R5 owns only local visual state. Destruction must never historical-restore,
	// dispatch the Cancel override, or Notify normal completion. The base class
	// remains responsible for authoritative NotifyWidgetLost catch-up.
	ClearNativePresentationFinishTimer();
	CleanupNativePresentationVisualsOnDestruct();
	ResetNativePresentationOwnership();

	if (bNativeDelegatesBound)
	{
		if (IsValid(Btn_EndTurn))
		{
			Btn_EndTurn->OnClicked.RemoveDynamic(this, &UBattleHUDWidget::HandleEndTurnClicked);
		}
		if (IsValid(Btn_Confirm))
		{
			Btn_Confirm->OnClicked.RemoveDynamic(this, &UBattleHUDWidget::HandleConfirmClicked);
		}
		if (IsValid(Btn_Cancel))
		{
			Btn_Cancel->OnClicked.RemoveDynamic(this, &UBattleHUDWidget::HandleCancelClicked);
		}
		if (IsValid(Combatant_PlayerPresentation))
		{
			Combatant_PlayerPresentation->OnTargetRequested.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantTargetRequested);
			Combatant_PlayerPresentation->OnInspectRequested.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantInspectRequested);
			Combatant_PlayerPresentation->OnInspectCleared.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantInspectCleared);
		}
		if (IsValid(Combatant_EnemyPresentation))
		{
			Combatant_EnemyPresentation->OnTargetRequested.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantTargetRequested);
			Combatant_EnemyPresentation->OnInspectRequested.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantInspectRequested);
			Combatant_EnemyPresentation->OnInspectCleared.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCombatantInspectCleared);
		}
		bNativeDelegatesBound = false;
	}

	// Dynamic Hand-card delegates are owned by the cards. Remove our bindings
	// before the panel releases them so no externally retained formal card can
	// issue a stale request during teardown.
	if (IsValid(HB_Hand))
	{
		for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
		{
			if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
			{
				CardWidget->OnBattleCardRequested.RemoveDynamic(
					this,
					&UBattleHUDWidget::HandleCardRequested);
			}
		}
	}

	Super::NativeDestruct();
}

void UBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateNativeCardAnimation(InDeltaTime);
}

void UBattleHUDWidget::NativeOnBattleHUDViewModelChanged()
{
	// Do not call the base implementation: doing so would execute the Legacy
	// BP_OnViewModelChanged graph on the Native stack.
	RefreshHUDFromViewModel();
}

void UBattleHUDWidget::RefreshHUDFromViewModel()
{
	if (!bNativeBindingsValid || !IsValid(ViewModel))
	{
		return;
	}

	RefreshHand();
	RefreshCombatants();
	RefreshEnergy();
	RefreshPileCounts();
	RefreshInputState();
	RefreshFeedback();
	RefreshEnemyIntent();
	RefreshTerminalFromViewModel();
}

void UBattleHUDWidget::RefreshHand()
{
	if (!IsValid(ViewModel) || !IsValid(HB_Hand) || CardWidgetClass == nullptr)
	{
		return;
	}

	// Formal Hand cards own one dynamic request binding for exactly their Widget
	// lifetime. Explicitly detach old children before rebuild so a stale retained
	// Widget cannot call back into the HUD after it leaves the formal Hand.
	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		if (UBattleCardWidget* ExistingCard = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
		{
			ExistingCard->OnBattleCardRequested.RemoveDynamic(
				this,
				&UBattleHUDWidget::HandleCardRequested);
		}
	}
	HB_Hand->ClearChildren();

	for (const FBattleHUDCardView& CardView : ViewModel->HandCards)
	{
		UBattleCardWidget* CardWidget = CreateWidget<UBattleCardWidget>(
			GetOwningPlayer(),
			CardWidgetClass);
		if (!IsValid(CardWidget))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD][Native] Failed to create formal Hand card RuntimeId=%d CardId=%s."),
				CardView.RuntimeId,
				*CardView.CardId.ToString());
			continue;
		}

		CardWidget->SetCardView(CardView);
		CardWidget->OnBattleCardRequested.AddUniqueDynamic(
			this,
			&UBattleHUDWidget::HandleCardRequested);
		HB_Hand->AddChildToHorizontalBox(CardWidget);
	}
}

void UBattleHUDWidget::RefreshCombatants()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	const bool bChoosingTarget =
		ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget
		&& !ViewModel->bInputLocked
		&& ViewModel->Outcome == EBattleHUDOutcome::None;

	auto RefreshOneCombatant =
		[this, bChoosingTarget](
			UBattleHUDCombatantPresentationWidgetBase* Presentation,
			const FBattleHUDCombatantView& Combatant,
			UProgressBar* HPProgress,
			UTextBlock* HPText,
			UTextBlock* BlockText)
		{
			if (!IsValid(Presentation))
			{
				return;
			}

			FBattleHUDTargetView LegalTarget;
			const bool bFoundLegalTarget =
				bChoosingTarget
				&& !Combatant.PresentationId.IsNone()
				&& ViewModel->TryGetLegalTargetByPresentationId(
					Combatant.PresentationId,
					LegalTarget);

			Presentation->SetPresentationData(
				Combatant,
				bChoosingTarget,
				bFoundLegalTarget,
				bFoundLegalTarget ? LegalTarget.TargetId : INDEX_NONE,
				bFoundLegalTarget);

			ApplyNativeCombatantVitals(
				HPProgress,
				HPText,
				BlockText,
				Combatant.HP,
				Combatant.MaxHP,
				Combatant.Block);
		};

	RefreshOneCombatant(
		Combatant_PlayerPresentation,
		ViewModel->Player,
		PB_PlayerHP,
		Txt_PlayerHP,
		Txt_PlayerBlock);
	RefreshOneCombatant(
		Combatant_EnemyPresentation,
		ViewModel->Enemy,
		PB_EnemyHP,
		Txt_EnemyHP,
		Txt_EnemyBlock);
}

void UBattleHUDWidget::RefreshEnergy()
{
	if (IsValid(ViewModel) && IsValid(Txt_Energy))
	{
		Txt_Energy->SetText(FText::Format(
			LOCTEXT("BattleHUDEnergyFormat", "{0}/{1}"),
			FText::AsNumber(ViewModel->Energy),
			FText::AsNumber(ViewModel->MaxEnergy)));
	}
}

void UBattleHUDWidget::RefreshPileCounts()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	if (IsValid(Txt_DrawCount))
	{
		Txt_DrawCount->SetText(FText::AsNumber(ViewModel->DrawCount));
	}
	if (IsValid(Txt_DiscardCount))
	{
		Txt_DiscardCount->SetText(FText::AsNumber(ViewModel->DiscardCount));
	}
	if (IsValid(Txt_ExhaustCount))
	{
		Txt_ExhaustCount->SetText(FText::AsNumber(ViewModel->ExhaustCount));
	}
}

void UBattleHUDWidget::RefreshInputState()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	const bool bTerminalOrUnavailable =
		ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->InteractionState == EBattleHUDInteractionState::Terminal
		|| ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable;
	const bool bInputAvailable =
		!ViewModel->bInputLocked
		&& !bTerminalOrUnavailable
		&& ViewModel->InteractionState != EBattleHUDInteractionState::Resolving;
	const bool bShowConfirm =
		ViewModel->InteractionState == EBattleHUDInteractionState::ReadyToConfirm;
	const bool bShowCancel =
		ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget
		|| bShowConfirm;

	if (IsValid(Btn_EndTurn))
	{
		Btn_EndTurn->SetVisibility(ESlateVisibility::Visible);
		Btn_EndTurn->SetIsEnabled(bInputAvailable && ViewModel->bCanEndTurn);
	}
	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->SetVisibility(
			bShowConfirm ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Confirm->SetIsEnabled(bInputAvailable && bShowConfirm);
	}
	if (IsValid(Btn_Cancel))
	{
		Btn_Cancel->SetVisibility(
			bShowCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Cancel->SetIsEnabled(bInputAvailable && bShowCancel);
	}
}

void UBattleHUDWidget::RefreshFeedback()
{
	if (IsValid(ViewModel) && IsValid(Txt_Feedback))
	{
		Txt_Feedback->SetText(ViewModel->LastFeedback);
		// Legacy keeps the feedback surface visible; an empty frozen message is
		// still a valid idle state and should not alter Designer layout.
		Txt_Feedback->SetVisibility(ESlateVisibility::Visible);
	}
}

void UBattleHUDWidget::RefreshEnemyIntent()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	const bool bHasIntent = ViewModel->EnemyIntent.Type != EBattleHUDIntentType::None;
	if (IsValid(EnemyIntentPanel))
	{
		EnemyIntentPanel->SetVisibility(
			bHasIntent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(Txt_EnemyIntent))
	{
		Txt_EnemyIntent->SetText(ViewModel->EnemyIntent.DisplayName);
		Txt_EnemyIntent->SetVisibility(
			bHasIntent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UBattleHUDWidget::RefreshTerminalFromViewModel()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	if (ViewModel->Outcome == EBattleHUDOutcome::None)
	{
		if (IsValid(Overlay_Terminal))
		{
			Overlay_Terminal->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IsValid(Txt_Outcome))
		{
			Txt_Outcome->SetText(FText::GetEmpty());
		}
		return;
	}

	if (IsValid(Txt_Outcome))
	{
		const FText OutcomeText = [&]()
		{
			switch (ViewModel->Outcome)
			{
			case EBattleHUDOutcome::Victory:
				return LOCTEXT("BattleHUDVictory", "胜利");
			case EBattleHUDOutcome::Defeat:
				return LOCTEXT("BattleHUDDefeat", "战斗失败");
			case EBattleHUDOutcome::ResolutionFaulted:
				return LOCTEXT("BattleHUDResolutionFault", "战斗结算异常");
			case EBattleHUDOutcome::None:
			default:
				return FText::GetEmpty();
			}
		}();
		Txt_Outcome->SetText(OutcomeText);
	}

	if (IsValid(Overlay_Terminal))
	{
		Overlay_Terminal->SetVisibility(ESlateVisibility::Visible);
	}
}

void UBattleHUDWidget::HandleCardRequested(int32 RuntimeId)
{
	if (bNativeBindingsValid && RuntimeId != INDEX_NONE)
	{
		SelectCard(RuntimeId);
	}
}

void UBattleHUDWidget::HandleEndTurnClicked()
{
	if (bNativeBindingsValid)
	{
		EndTurn();
	}
}

void UBattleHUDWidget::HandleConfirmClicked()
{
	if (bNativeBindingsValid)
	{
		ConfirmSelectedCard();
	}
}

void UBattleHUDWidget::HandleCancelClicked()
{
	if (bNativeBindingsValid)
	{
		CancelSelection();
	}
}

void UBattleHUDWidget::HandleCombatantTargetRequested(int32 TargetId)
{
	if (bNativeBindingsValid)
	{
		SelectTarget(TargetId);
	}
}

bool UBattleHUDWidget::RefreshStatusTooltip(
	UWidget* StatusTooltip,
	const TArray<FBattleHUDStatusView>& Statuses)
{
	if (!IsValid(StatusTooltip))
	{
		return false;
	}

	// StatusTooltip_Player/Enemy are optional Designer surfaces. Their existing
	// Blueprint contract is RebuildTooltip(TArray<FBattleHUDStatusView>), so the
	// Native HUD forwards the frozen ViewModel array through that single bridge.
	// This does not create, update or remove formal status rows; that lifecycle
	// remains owned by the later R9 migration.
	static const FName RebuildTooltipFunctionName(TEXT("RebuildTooltip"));
	UFunction* RebuildTooltipFunction =
		StatusTooltip->FindFunction(RebuildTooltipFunctionName);
	if (RebuildTooltipFunction == nullptr)
	{
		return false;
	}

	struct FRebuildTooltipParams
	{
		TArray<FBattleHUDStatusView> Statuses;
	};

	FRebuildTooltipParams Params;
	Params.Statuses = Statuses;
	StatusTooltip->ProcessEvent(RebuildTooltipFunction, &Params);
	return true;
}

void UBattleHUDWidget::HandleCombatantInspectRequested(
	UBattleHUDCombatantPresentationWidgetBase* Presentation)
{
	if (!IsValid(Presentation) || !IsValid(ViewModel))
	{
		return;
	}

	UTextBlock* NameText = nullptr;
	UWidget* StatusTooltip = nullptr;
	const FBattleHUDCombatantView* CombatantView = nullptr;
	if (Presentation == Combatant_PlayerPresentation)
	{
		NameText = Txt_PlayerName;
		StatusTooltip = StatusTooltip_Player;
		CombatantView = &ViewModel->Player;
	}
	else if (Presentation == Combatant_EnemyPresentation)
	{
		NameText = Txt_EnemyName;
		StatusTooltip = StatusTooltip_Enemy;
		CombatantView = &ViewModel->Enemy;
	}

	if (IsValid(NameText) && CombatantView != nullptr)
	{
		NameText->SetText(CombatantView->DisplayName);
		NameText->SetVisibility(ESlateVisibility::Visible);
	}
	if (IsValid(StatusTooltip) && CombatantView != nullptr)
	{
		const bool bTooltipRefreshed =
			RefreshStatusTooltip(StatusTooltip, CombatantView->Statuses);
		StatusTooltip->SetVisibility(
			bTooltipRefreshed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UBattleHUDWidget::HandleCombatantInspectCleared(
	UBattleHUDCombatantPresentationWidgetBase* Presentation)
{
	if (!IsValid(Presentation))
	{
		return;
	}

	UTextBlock* NameText = nullptr;
	UWidget* StatusTooltip = nullptr;
	if (Presentation == Combatant_PlayerPresentation)
	{
		NameText = Txt_PlayerName;
		StatusTooltip = StatusTooltip_Player;
	}
	else if (Presentation == Combatant_EnemyPresentation)
	{
		NameText = Txt_EnemyName;
		StatusTooltip = StatusTooltip_Enemy;
	}

	if (IsValid(NameText))
	{
		NameText->SetVisibility(ESlateVisibility::Hidden);
	}
	if (IsValid(StatusTooltip))
	{
		StatusTooltip->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UBattleHUDWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token
)
{
	if (!IsNativeRecordTokenConsistent(Record, Token))
	{
		return false;
	}

	switch (Record.Type)
	{
	case EBattlePresentationRecordType::CardPlayed:
		return BeginNativeCardPlayedPresentation(Record, Token);
	case EBattlePresentationRecordType::CardZoneChanged:
		return BeginNativeCardZoneChangedPresentation(Record, Token);
	case EBattlePresentationRecordType::Damage:
		return BeginNativeDamagePresentation(Record, Token);
	case EBattlePresentationRecordType::EnergyChanged:
		return BeginNativeEnergyChangedPresentation(Record, Token);
	case EBattlePresentationRecordType::BlockChanged:
		return BeginNativeBlockChangedPresentation(Record, Token);
	case EBattlePresentationRecordType::DeckShuffled:
		return BeginNativeDeckShuffledPresentation(Record, Token);
	default:
		// R9+ Records remain on the Controller's immediate-fallback path.
		return false;
	}
}

bool UBattleHUDWidget::BeginNativeCardPlayedPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardPlayedPresentationPayload& Payload = Record.CardPlayed;
	UBattleCardWidget* HistoricalHandCard = nullptr;
	const bool bSourceValid = IsKnownCombatantPresentationId(Payload.SourcePresentationId);
	const bool bTargetValid = Payload.TargetPresentationId.IsNone()
		|| IsKnownCombatantPresentationId(Payload.TargetPresentationId);
	const bool bPayloadValid =
		IsValid(ViewModel)
		&& IsValid(HB_Hand)
		&& IsValid(OV_PlayArea)
		&& CardWidgetClass != nullptr
		&& IsNativeCardSnapshotValid(Payload.Card)
		&& bSourceValid
		&& bTargetValid
		&& Payload.HandIndexBefore >= 0
		&& Payload.PlayAreaIndexAfter == 0
		&& Payload.EnergyBefore >= 0
		&& Payload.EnergyAfter >= 0
		&& Payload.EnergyAfter <= Payload.EnergyBefore
		&& Payload.CostPaid >= 0
		&& Payload.CostPaid == Payload.EnergyBefore - Payload.EnergyAfter
		&& Payload.CostPaid == Payload.Card.Cost
		&& ViewModel->Energy == Payload.EnergyBefore
		&& !NativePlayedCardWidget.IsValid()
		&& !ActiveNativeDrawnCardWidget.IsValid()
		&& OV_PlayArea->GetChildrenCount() == Payload.PlayAreaIndexAfter
		&& FindExactHistoricalHandCard(
			Payload.Card,
			Payload.HandIndexBefore,
			HistoricalHandCard);
	if (!bPayloadValid)
	{
		return false;
	}

	UBattleCardWidget* PresentationCard = CreateNativePresentationCard(Payload.Card);
	if (!IsValid(PresentationCard))
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::CardPlayed;
	ActiveNativeHistoricalHandCardWidget = HistoricalHandCard;
	ActiveNativeHistoricalHandVisibility = HistoricalHandCard->GetVisibility();
	NativePlayedCardWidget = PresentationCard;
	UOverlaySlot* PlayAreaSlot = OV_PlayArea->AddChildToOverlay(PresentationCard);
	if (!IsValid(PlayAreaSlot))
	{
		NativePlayedCardWidget.Reset();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}
	PlayAreaSlot->SetHorizontalAlignment(HAlign_Center);
	PlayAreaSlot->SetVerticalAlignment(VAlign_Center);
	ConfigureNativeCardAnimation(
		PresentationCard,
		HistoricalHandCard,
		nullptr,
		NativeHandCardFallbackTranslation,
		FVector2D::ZeroVector,
		0.88f,
		1.0f,
		0.0f,
		1.0f);
	HistoricalHandCard->SetVisibility(ESlateVisibility::Hidden);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		HistoricalHandCard->SetVisibility(ActiveNativeHistoricalHandVisibility);
		PresentationCard->RemoveFromParent();
		NativePlayedCardWidget.Reset();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativeCardZoneChangedPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	if (!IsNativeCardSnapshotValid(Payload.Card))
	{
		return false;
	}

	if (Payload.FromZone == ECardZone::Hand
		&& Payload.ToZone == ECardZone::DiscardPile)
	{
		return BeginNativeHandToDiscardPresentation(Record, Token);
	}
	if (Payload.FromZone == ECardZone::DrawPile
		&& Payload.ToZone == ECardZone::Hand)
	{
		return BeginNativeDrawToHandPresentation(Record, Token);
	}
	if (Payload.FromZone == ECardZone::PlayArea
		&& (Payload.ToZone == ECardZone::DiscardPile
			|| Payload.ToZone == ECardZone::ExhaustPile
			|| Payload.ToZone == ECardZone::RemovedPile))
	{
		return BeginNativePlayAreaToDestinationPresentation(Record, Token);
	}

	return false;
}

bool UBattleHUDWidget::BeginNativeHandToDiscardPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	UBattleCardWidget* HistoricalHandCard = nullptr;
	if (!IsValid(ViewModel)
		|| !IsValid(OV_PlayArea)
		|| !IsValid(Txt_DiscardCount)
		|| CardWidgetClass == nullptr
		|| Payload.ToIndex != ViewModel->DiscardCount
		|| NativePlayedCardWidget.IsValid()
		|| ActiveNativeZoneCardWidget.IsValid()
		|| !FindExactHistoricalHandCard(Payload.Card, Payload.FromIndex, HistoricalHandCard))
	{
		return false;
	}

	UBattleCardWidget* PresentationCard = CreateNativePresentationCard(Payload.Card);
	if (!IsValid(PresentationCard))
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::HandToDiscard;
	ActiveNativeHistoricalHandCardWidget = HistoricalHandCard;
	ActiveNativeHistoricalHandVisibility = HistoricalHandCard->GetVisibility();
	ActiveNativeZoneCardWidget = PresentationCard;
	UOverlaySlot* DiscardMotionSlot = OV_PlayArea->AddChildToOverlay(PresentationCard);
	if (!IsValid(DiscardMotionSlot))
	{
		ActiveNativeZoneCardWidget.Reset();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}
	DiscardMotionSlot->SetHorizontalAlignment(HAlign_Center);
	DiscardMotionSlot->SetVerticalAlignment(VAlign_Center);
	ConfigureNativeCardAnimation(
		PresentationCard,
		HistoricalHandCard,
		Txt_DiscardCount,
		NativeHandCardFallbackTranslation,
		NativeDiscardCardFallbackTranslation,
		1.0f,
		0.72f,
		1.0f,
		0.15f);
	HistoricalHandCard->SetVisibility(ESlateVisibility::Hidden);
	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		HistoricalHandCard->SetVisibility(ActiveNativeHistoricalHandVisibility);
		PresentationCard->RemoveFromParent();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativeDrawToHandPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	const bool bPayloadValid =
		IsValid(ViewModel)
		&& IsValid(HB_Hand)
		&& IsValid(Txt_DrawCount)
		&& CardWidgetClass != nullptr
		&& ViewModel->DrawCount > 0
		&& Payload.FromIndex == ViewModel->DrawCount - 1
		&& Payload.ToIndex == ViewModel->HandCards.Num()
		&& HB_Hand->GetChildrenCount() == ViewModel->HandCards.Num()
		&& IsRuntimeIdAbsentFromNativeCardVisuals(Payload.Card.RuntimeId);
	if (!bPayloadValid)
	{
		return false;
	}

	UBattleCardWidget* PresentationCard = CreateNativePresentationCard(Payload.Card);
	if (!IsValid(PresentationCard))
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::DrawToHand;
	ActiveNativeDrawnCardWidget = PresentationCard;
	ActiveNativeDrawCountBefore = ViewModel->DrawCount;
	ActiveNativeDrawCountAfter = ViewModel->DrawCount - 1;
	ActiveNativeCardDestinationIndex = Payload.ToIndex;
	if (HB_Hand->AddChild(PresentationCard) == nullptr)
	{
		PresentationCard->RemoveFromParent();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}
	ConfigureNativeCardAnimation(
		PresentationCard,
		Txt_DrawCount,
		nullptr,
		NativeDrawCardFallbackTranslation,
		FVector2D::ZeroVector,
		NativeDrawCardStartScale,
		1.0f,
		0.0f,
		1.0f);
	ApplyNativePileCounts(ActiveNativeDrawCountAfter, ViewModel->DiscardCount);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativePileCounts(ActiveNativeDrawCountBefore, ViewModel->DiscardCount);
		PresentationCard->RemoveFromParent();
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativePlayAreaToDestinationPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	UBattleCardWidget* PlayedCard = NativePlayedCardWidget.Get();
	bool bDestinationIndexValid = false;
	if (IsValid(ViewModel))
	{
		switch (Payload.ToZone)
		{
		case ECardZone::DiscardPile:
			bDestinationIndexValid = Payload.ToIndex == ViewModel->DiscardCount;
			break;
		case ECardZone::ExhaustPile:
			bDestinationIndexValid = Payload.ToIndex == ViewModel->ExhaustCount;
			break;
		case ECardZone::RemovedPile:
			bDestinationIndexValid = Payload.ToIndex >= 0;
			break;
		default:
			break;
		}
	}

	if (!IsValid(PlayedCard)
		|| PlayedCard->GetParent() != OV_PlayArea
		|| Payload.FromIndex != 0
		|| !bDestinationIndexValid
		|| !DoesNativeCardViewMatchSnapshot(PlayedCard->GetCardView(), Payload.Card))
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::PlayAreaToDestination;
	if (Payload.ToZone == ECardZone::DiscardPile)
	{
		ConfigureNativeCardAnimation(
			PlayedCard,
			nullptr,
			Txt_DiscardCount,
			FVector2D::ZeroVector,
			NativeDiscardCardFallbackTranslation,
			1.0f,
			0.72f,
			1.0f,
			0.15f);
	}
	else
	{
		// Exhaust/Removed retire at the PlayArea instead of pretending to enter
		// an unrelated pile. Scale/fade provides the sealed disappearance cue.
		ConfigureNativeCardAnimation(
			PlayedCard,
			nullptr,
			nullptr,
			FVector2D::ZeroVector,
			FVector2D::ZeroVector,
			1.0f,
			0.72f,
			1.0f,
			0.0f);
	}
	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		NormalizeNativeCardTransform(PlayedCard);
		ResetNativeCardRecordState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::IsNativeCardSnapshotValid(
	const FPresentationCardSnapshot& Snapshot) const
{
	const bool bCardTypeValid = Snapshot.CardType == ECardType::Attack
		|| Snapshot.CardType == ECardType::Skill
		|| Snapshot.CardType == ECardType::Power
		|| Snapshot.CardType == ECardType::Status
		|| Snapshot.CardType == ECardType::Curse;
	const bool bTargetTypeValid = Snapshot.TargetType == ECardTargetType::None
		|| Snapshot.TargetType == ECardTargetType::Self
		|| Snapshot.TargetType == ECardTargetType::Enemy;
	return Snapshot.RuntimeId != INDEX_NONE
		&& !Snapshot.CardId.IsNone()
		&& !Snapshot.DisplayName.IsEmpty()
		&& Snapshot.Cost >= 0
		&& bCardTypeValid
		&& bTargetTypeValid;
}

bool UBattleHUDWidget::DoesNativeCardViewMatchSnapshot(
	const FBattleHUDCardView& View,
	const FPresentationCardSnapshot& Snapshot) const
{
	return View.RuntimeId == Snapshot.RuntimeId
		&& View.CardId == Snapshot.CardId
		&& View.DisplayName.EqualTo(Snapshot.DisplayName)
		&& View.Cost == Snapshot.Cost
		&& View.CardType == Snapshot.CardType
		&& View.TargetType == Snapshot.TargetType
		&& View.Description.EqualTo(Snapshot.Description)
		&& View.CardArt.Get() == Snapshot.CardArt.Get();
}

bool UBattleHUDWidget::FindExactHistoricalHandCard(
	const FPresentationCardSnapshot& Snapshot,
	int32 RequiredIndex,
	UBattleCardWidget*& OutCardWidget) const
{
	OutCardWidget = nullptr;
	if (!IsValid(ViewModel)
		|| !IsValid(HB_Hand)
		|| !ViewModel->HandCards.IsValidIndex(RequiredIndex)
		|| ViewModel->HandCards.Num() != HB_Hand->GetChildrenCount()
		|| !IsValid(HB_Hand->GetChildAt(RequiredIndex)))
	{
		return false;
	}

	int32 ViewModelRuntimeMatches = 0;
	for (const FBattleHUDCardView& CardView : ViewModel->HandCards)
	{
		ViewModelRuntimeMatches += CardView.RuntimeId == Snapshot.RuntimeId ? 1 : 0;
	}

	int32 WidgetRuntimeMatches = 0;
	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		const UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
		if (!IsValid(CardWidget))
		{
			return false;
		}
		WidgetRuntimeMatches += CardWidget->GetRuntimeId() == Snapshot.RuntimeId ? 1 : 0;
	}

	UBattleCardWidget* RequiredCard = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(RequiredIndex));
	if (ViewModelRuntimeMatches != 1
		|| WidgetRuntimeMatches != 1
		|| !IsValid(RequiredCard)
		|| !DoesNativeCardViewMatchSnapshot(ViewModel->HandCards[RequiredIndex], Snapshot)
		|| !DoesNativeCardViewMatchSnapshot(RequiredCard->GetCardView(), Snapshot))
	{
		return false;
	}

	OutCardWidget = RequiredCard;
	return true;
}

bool UBattleHUDWidget::IsRuntimeIdAbsentFromNativeCardVisuals(int32 RuntimeId) const
{
	if (RuntimeId == INDEX_NONE
		|| (NativePlayedCardWidget.IsValid()
			&& NativePlayedCardWidget->GetRuntimeId() == RuntimeId)
		|| (ActiveNativeDrawnCardWidget.IsValid()
			&& ActiveNativeDrawnCardWidget->GetRuntimeId() == RuntimeId)
		|| (ActiveNativeZoneCardWidget.IsValid()
			&& ActiveNativeZoneCardWidget->GetRuntimeId() == RuntimeId))
	{
		return false;
	}

	if (IsValid(HB_Hand))
	{
		for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
		{
			const UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
			if (!IsValid(CardWidget) || CardWidget->GetRuntimeId() == RuntimeId)
			{
				return false;
			}
		}
	}

	if (IsValid(ViewModel))
	{
		for (const FBattleHUDCardView& CardView : ViewModel->HandCards)
		{
			if (CardView.RuntimeId == RuntimeId)
			{
				return false;
			}
		}
	}

	return true;
}

UBattleCardWidget* UBattleHUDWidget::CreateNativePresentationCard(
	const FPresentationCardSnapshot& Snapshot) const
{
	if (CardWidgetClass == nullptr)
	{
		return nullptr;
	}

	UBattleCardWidget* CardWidget = nullptr;
	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		CardWidget = CreateWidget<UBattleCardWidget>(OwningPlayer, CardWidgetClass);
	}
	else if (UWorld* World = GetWorld(); IsValid(World))
	{
		CardWidget = CreateWidget<UBattleCardWidget>(World, CardWidgetClass);
	}

	if (!IsValid(CardWidget))
	{
		return nullptr;
	}

	CardWidget->SetCardView(MakePresentationCardView(Snapshot));
	CardWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CardWidget;
}

void UBattleHUDWidget::ConfigureNativeCardAnimation(
	UBattleCardWidget* MovingCard,
	UWidget* StartAnchor,
	UWidget* EndAnchor,
	const FVector2D& FallbackStartTranslation,
	const FVector2D& FallbackEndTranslation,
	float StartScale,
	float EndScale,
	float StartOpacity,
	float EndOpacity)
{
	ActiveNativeMovingCardWidget = MovingCard;
	ActiveNativeCardAnimationStartAnchor = StartAnchor;
	ActiveNativeCardAnimationEndAnchor = EndAnchor;
	ActiveNativeCardAnimationFallbackStart = FallbackStartTranslation;
	ActiveNativeCardAnimationFallbackEnd = FallbackEndTranslation;
	ActiveNativeCardAnimationStartTranslation = FallbackStartTranslation;
	ActiveNativeCardAnimationEndTranslation = FallbackEndTranslation;
	ActiveNativeCardAnimationStartScale = StartScale;
	ActiveNativeCardAnimationEndScale = EndScale;
	ActiveNativeCardAnimationStartOpacity = StartOpacity;
	ActiveNativeCardAnimationEndOpacity = EndOpacity;
	ActiveNativeCardAnimationElapsedSeconds = 0.0f;
	bNativeCardAnimationInitialized = false;

	if (IsValid(MovingCard))
	{
		MovingCard->SetRenderTranslation(FallbackStartTranslation);
		MovingCard->SetRenderScale(FVector2D(StartScale, StartScale));
		MovingCard->SetRenderOpacity(StartOpacity);
	}
}

void UBattleHUDWidget::UpdateNativeCardAnimation(float DeltaSeconds)
{
	if (!bHasActiveNativePresentation)
	{
		return;
	}

	UBattleCardWidget* MovingCard = ActiveNativeMovingCardWidget.Get();
	if (!IsValid(MovingCard))
	{
		return;
	}

	auto ResolveAnchorTranslation =
		[MovingCard](UWidget* Anchor, const FVector2D& Fallback) -> FVector2D
		{
			if (!IsValid(Anchor))
			{
				return Fallback;
			}

			const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
			const FGeometry& CardGeometry = MovingCard->GetCachedGeometry();
			if (AnchorGeometry.GetLocalSize().SizeSquared() <= 0.0f
				|| CardGeometry.GetLocalSize().SizeSquared() <= 0.0f)
			{
				return Fallback;
			}

			const FVector2D AnchorAbsolute = AnchorGeometry.LocalToAbsolute(
				AnchorGeometry.GetLocalSize() * 0.5f);
			const FVector2D CardAbsolute = CardGeometry.LocalToAbsolute(
				CardGeometry.GetLocalSize() * 0.5f);
			const FVector2D AnchorLocal = FVector2D(
				CardGeometry.AbsoluteToLocal(AnchorAbsolute));
			const FVector2D CardLocal = FVector2D(
				CardGeometry.AbsoluteToLocal(CardAbsolute));
			return AnchorLocal - CardLocal;
		};

	if (!bNativeCardAnimationInitialized)
	{
		ActiveNativeCardAnimationStartTranslation = ResolveAnchorTranslation(
			ActiveNativeCardAnimationStartAnchor.Get(),
			ActiveNativeCardAnimationFallbackStart);
		ActiveNativeCardAnimationEndTranslation = ResolveAnchorTranslation(
			ActiveNativeCardAnimationEndAnchor.Get(),
			ActiveNativeCardAnimationFallbackEnd);
		MovingCard->SetRenderTranslation(ActiveNativeCardAnimationStartTranslation);
		bNativeCardAnimationInitialized = true;
	}

	ActiveNativeCardAnimationElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float LinearAlpha = FMath::Clamp(
		ActiveNativeCardAnimationElapsedSeconds / NativePresentationDurationSeconds,
		0.0f,
		1.0f);
	const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, LinearAlpha, 3.0f);
	MovingCard->SetRenderTranslation(FMath::Lerp(
		ActiveNativeCardAnimationStartTranslation,
		ActiveNativeCardAnimationEndTranslation,
		EasedAlpha));
	const float CardScale = FMath::Lerp(
		ActiveNativeCardAnimationStartScale,
		ActiveNativeCardAnimationEndScale,
		EasedAlpha);
	MovingCard->SetRenderScale(FVector2D(CardScale, CardScale));
	MovingCard->SetRenderOpacity(FMath::Lerp(
		ActiveNativeCardAnimationStartOpacity,
		ActiveNativeCardAnimationEndOpacity,
		EasedAlpha));
}

void UBattleHUDWidget::NormalizeNativeCardTransform(UBattleCardWidget* CardWidget) const
{
	if (!IsValid(CardWidget))
	{
		return;
	}

	CardWidget->SetRenderTranslation(FVector2D::ZeroVector);
	CardWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
	CardWidget->SetRenderOpacity(1.0f);
}

bool UBattleHUDWidget::BeginNativeEnergyChangedPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FEnergyChangedPresentationPayload& Payload = Record.EnergyChanged;
	if (!IsValid(ViewModel)
		|| !IsValid(Txt_Energy)
		|| ViewModel->MaxEnergy < 0
		|| Payload.EnergyBefore < 0
		|| Payload.EnergyAfter < 0
		|| Payload.EnergyBefore > ViewModel->MaxEnergy
		|| Payload.EnergyAfter > ViewModel->MaxEnergy
		|| Payload.EnergyBefore == Payload.EnergyAfter
		|| Payload.Delta != Payload.EnergyAfter - Payload.EnergyBefore
		|| ViewModel->Energy != Payload.EnergyBefore)
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeSimplePrimaryBefore = Payload.EnergyBefore;
	ActiveNativeSimplePrimaryAfter = Payload.EnergyAfter;
	ActiveNativeSimpleEnergyMax = ViewModel->MaxEnergy;
	ApplyNativeEnergyValue(ActiveNativeSimplePrimaryAfter, ActiveNativeSimpleEnergyMax);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativeEnergyValue(ActiveNativeSimplePrimaryBefore, ActiveNativeSimpleEnergyMax);
		ResetNativeSimplePresentationState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativeBlockChangedPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FBlockChangedPresentationPayload& Payload = Record.BlockChanged;
	int32 HistoricalBlock = 0;
	UTextBlock* BlockText = ResolveBlockTextForPresentationId(
		Payload.TargetPresentationId,
		HistoricalBlock);
	const bool bSourceIsValid = Payload.SourcePresentationId.IsNone()
		|| IsKnownCombatantPresentationId(Payload.SourcePresentationId);
	const bool bCommonPayloadValid =
		IsValid(BlockText)
		&& bSourceIsValid
		&& Payload.BlockBefore >= 0
		&& Payload.BlockAfter >= 0
		&& Payload.BlockBefore != Payload.BlockAfter
		&& Payload.BlockDelta == Payload.BlockAfter - Payload.BlockBefore
		&& HistoricalBlock == Payload.BlockBefore;

	bool bReasonValid = false;
	if (bCommonPayloadValid)
	{
		switch (Payload.Reason)
		{
		case EBlockPresentationReason::Gain:
			bReasonValid = Payload.BlockDelta > 0;
			break;
		case EBlockPresentationReason::TurnStartClear:
			bReasonValid = Payload.SourcePresentationId.IsNone()
				&& Payload.BlockBefore > 0
				&& Payload.BlockAfter == 0
				&& Payload.BlockDelta == -Payload.BlockBefore;
			break;
		default:
			break;
		}
	}

	if (!bCommonPayloadValid || !bReasonValid)
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeSimpleBlockText = BlockText;
	ActiveNativeSimplePrimaryBefore = Payload.BlockBefore;
	ActiveNativeSimplePrimaryAfter = Payload.BlockAfter;
	ApplyNativeBlockValue(BlockText, ActiveNativeSimplePrimaryAfter);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativeBlockValue(BlockText, ActiveNativeSimplePrimaryBefore);
		ResetNativeSimplePresentationState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativeDeckShuffledPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FDeckShuffledPresentationPayload& Payload = Record.DeckShuffled;
	const bool bCountsValid =
		IsValid(ViewModel)
		&& IsValid(Txt_DrawCount)
		&& IsValid(Txt_DiscardCount)
		&& Payload.MovedCardCount > 0
		&& Payload.DrawCountBefore == 0
		&& Payload.DiscardCountBefore == Payload.MovedCardCount
		&& Payload.DrawCountAfter == Payload.MovedCardCount
		&& Payload.DiscardCountAfter == 0
		&& Payload.DrawCountBefore + Payload.DiscardCountBefore
			== Payload.DrawCountAfter + Payload.DiscardCountAfter
		&& ViewModel->DrawCount == Payload.DrawCountBefore
		&& ViewModel->DiscardCount == Payload.DiscardCountBefore;
	if (!bCountsValid)
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveNativeSimplePrimaryBefore = Payload.DrawCountBefore;
	ActiveNativeSimplePrimaryAfter = Payload.DrawCountAfter;
	ActiveNativeSimpleSecondaryBefore = Payload.DiscardCountBefore;
	ActiveNativeSimpleSecondaryAfter = Payload.DiscardCountAfter;
	ApplyNativePileCounts(
		ActiveNativeSimplePrimaryAfter,
		ActiveNativeSimpleSecondaryAfter);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativePileCounts(
			ActiveNativeSimplePrimaryBefore,
			ActiveNativeSimpleSecondaryBefore);
		ResetNativeSimplePresentationState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::BeginNativeDamagePresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FDamagePresentationPayload& Payload = Record.Damage;
	UBattleHUDCombatantPresentationWidgetBase* TargetPresentation = nullptr;
	UProgressBar* TargetHPProgress = nullptr;
	UTextBlock* TargetHPText = nullptr;
	UTextBlock* TargetBlockText = nullptr;
	const FBattleHUDCombatantView* HistoricalTarget = nullptr;
	const bool bTargetResolved = ResolveDamageTarget(
		Payload.TargetPresentationId,
		TargetPresentation,
		TargetHPProgress,
		TargetHPText,
		TargetBlockText,
		HistoricalTarget);
	const bool bSourceIsValid = Payload.SourcePresentationId.IsNone()
		|| IsKnownCombatantPresentationId(Payload.SourcePresentationId);
	const bool bDamageKindValid = Payload.DamageKind == EDamageKind::Attack
		|| Payload.DamageKind == EDamageKind::Effect;
	const int64 AccountedDamage = static_cast<int64>(Payload.BlockedDamage)
		+ static_cast<int64>(Payload.HPDamage);
	const bool bPayloadValid =
		bTargetResolved
		&& IsValid(Txt_DamagePresentation)
		&& bSourceIsValid
		&& bDamageKindValid
		&& Payload.IncomingDamage > 0
		&& Payload.HPBefore > 0
		&& Payload.HPAfter >= 0
		&& Payload.HPAfter <= Payload.HPBefore
		&& Payload.BlockBefore >= 0
		&& Payload.BlockAfter >= 0
		&& Payload.BlockAfter <= Payload.BlockBefore
		&& Payload.BlockedDamage >= 0
		&& Payload.HPDamage >= 0
		&& Payload.BlockedDamage == Payload.BlockBefore - Payload.BlockAfter
		&& Payload.HPDamage == Payload.HPBefore - Payload.HPAfter
		&& AccountedDamage > 0
		&& AccountedDamage <= static_cast<int64>(Payload.IncomingDamage)
		&& HistoricalTarget != nullptr
		&& HistoricalTarget->MaxHP > 0
		&& Payload.HPBefore <= HistoricalTarget->MaxHP
		&& HistoricalTarget->HP == Payload.HPBefore
		&& HistoricalTarget->Block == Payload.BlockBefore;
	if (!bPayloadValid)
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	ActiveDamageTargetWidget = TargetPresentation;
	ActiveDamageTargetHPProgress = TargetHPProgress;
	ActiveDamageTargetHPText = TargetHPText;
	ActiveDamageTargetBlockText = TargetBlockText;
	ActiveDamageHPBefore = Payload.HPBefore;
	ActiveDamageHPAfter = Payload.HPAfter;
	ActiveDamageBlockBefore = Payload.BlockBefore;
	ActiveDamageBlockAfter = Payload.BlockAfter;
	ActiveDamageMaxHP = HistoricalTarget->MaxHP;

	ApplyNativeCombatantVitals(
		TargetHPProgress,
		TargetHPText,
		TargetBlockText,
		ActiveDamageHPAfter,
		ActiveDamageMaxHP,
		ActiveDamageBlockAfter);
	Txt_DamagePresentation->SetText(FText::AsNumber(Payload.IncomingDamage));
	Txt_DamagePresentation->SetVisibility(ESlateVisibility::HitTestInvisible);
	TargetPresentation->SetRenderOpacity(0.45f);

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativeCombatantVitals(
			TargetHPProgress,
			TargetHPText,
			TargetBlockText,
			ActiveDamageHPBefore,
			ActiveDamageMaxHP,
			ActiveDamageBlockBefore);
		CleanupNativeDamageTransientVisuals();
		ResetNativeDamagePresentationState();
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

bool UBattleHUDWidget::IsNativeRecordTokenConsistent(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token) const
{
	return Record.BattleId > 0
		&& Record.ResolutionId > 0
		&& Record.PresentationSequence > 0
		&& Token.LocalPlaybackGeneration > 0
		&& Token.BattleId == Record.BattleId
		&& Token.ResolutionId == Record.ResolutionId
		&& Token.PresentationSequence == Record.PresentationSequence;
}

bool UBattleHUDWidget::IsKnownCombatantPresentationId(FName PresentationId) const
{
	if (!IsValid(ViewModel) || PresentationId.IsNone())
	{
		return false;
	}

	const bool bMatchesPlayer = ViewModel->Player.PresentationId == PresentationId;
	const bool bMatchesEnemy = ViewModel->Enemy.PresentationId == PresentationId;
	return bMatchesPlayer != bMatchesEnemy;
}

UTextBlock* UBattleHUDWidget::ResolveBlockTextForPresentationId(
	FName PresentationId,
	int32& OutHistoricalBlock) const
{
	OutHistoricalBlock = 0;
	if (!IsValid(ViewModel) || PresentationId.IsNone())
	{
		return nullptr;
	}

	const bool bMatchesPlayer = ViewModel->Player.PresentationId == PresentationId;
	const bool bMatchesEnemy = ViewModel->Enemy.PresentationId == PresentationId;
	if (bMatchesPlayer == bMatchesEnemy)
	{
		return nullptr;
	}

	if (bMatchesPlayer)
	{
		OutHistoricalBlock = ViewModel->Player.Block;
		return Txt_PlayerBlock;
	}

	OutHistoricalBlock = ViewModel->Enemy.Block;
	return Txt_EnemyBlock;
}

bool UBattleHUDWidget::ResolveDamageTarget(
	FName PresentationId,
	UBattleHUDCombatantPresentationWidgetBase*& OutPresentation,
	UProgressBar*& OutHPProgress,
	UTextBlock*& OutHPText,
	UTextBlock*& OutBlockText,
	const FBattleHUDCombatantView*& OutHistoricalView) const
{
	OutPresentation = nullptr;
	OutHPProgress = nullptr;
	OutHPText = nullptr;
	OutBlockText = nullptr;
	OutHistoricalView = nullptr;
	if (!IsValid(ViewModel) || PresentationId.IsNone())
	{
		return false;
	}

	const bool bMatchesPlayer = ViewModel->Player.PresentationId == PresentationId;
	const bool bMatchesEnemy = ViewModel->Enemy.PresentationId == PresentationId;
	if (bMatchesPlayer == bMatchesEnemy)
	{
		return false;
	}

	if (bMatchesPlayer)
	{
		OutPresentation = Combatant_PlayerPresentation;
		OutHPProgress = PB_PlayerHP;
		OutHPText = Txt_PlayerHP;
		OutBlockText = Txt_PlayerBlock;
		OutHistoricalView = &ViewModel->Player;
	}
	else
	{
		OutPresentation = Combatant_EnemyPresentation;
		OutHPProgress = PB_EnemyHP;
		OutHPText = Txt_EnemyHP;
		OutBlockText = Txt_EnemyBlock;
		OutHistoricalView = &ViewModel->Enemy;
	}

	return IsValid(OutPresentation)
		&& IsValid(OutHPProgress)
		&& IsValid(OutHPText)
		&& IsValid(OutBlockText);
}

void UBattleHUDWidget::ApplyNativeEnergyValue(int32 Energy, int32 MaxEnergy)
{
	if (IsValid(Txt_Energy))
	{
		Txt_Energy->SetText(FText::Format(
			LOCTEXT("BattleHUDEnergyFormat", "{0}/{1}"),
			FText::AsNumber(Energy),
			FText::AsNumber(MaxEnergy)));
	}
}

void UBattleHUDWidget::ApplyNativeBlockValue(UTextBlock* BlockText, int32 Block)
{
	if (!IsValid(BlockText))
	{
		return;
	}

	BlockText->SetText(FText::AsNumber(Block));

	// The sealed Designer hierarchy is Txt_*Block -> OV_*Block ->
	// SB_*BlockBadge. Always collapse the complete badge at zero.
	UWidget* BlockBadgeSurface = BlockText;
	if (UPanelWidget* BlockOverlay = BlockText->GetParent())
	{
		BlockBadgeSurface = BlockOverlay;
		if (UPanelWidget* BlockBadge = BlockOverlay->GetParent())
		{
			BlockBadgeSurface = BlockBadge;
		}
	}

	BlockBadgeSurface->SetVisibility(
		Block > 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
}

void UBattleHUDWidget::ApplyNativePileCounts(int32 DrawCount, int32 DiscardCount)
{
	if (IsValid(Txt_DrawCount))
	{
		Txt_DrawCount->SetText(FText::AsNumber(DrawCount));
	}
	if (IsValid(Txt_DiscardCount))
	{
		Txt_DiscardCount->SetText(FText::AsNumber(DiscardCount));
	}
}

void UBattleHUDWidget::ApplyNativeCombatantVitals(
	UProgressBar* HPProgress,
	UTextBlock* HPText,
	UTextBlock* BlockText,
	int32 HP,
	int32 MaxHP,
	int32 Block)
{
	if (IsValid(HPText))
	{
		HPText->SetText(FText::Format(
			LOCTEXT("BattleHUDHPFormat", "{0}/{1}"),
			FText::AsNumber(HP),
			FText::AsNumber(MaxHP)));
	}

	if (IsValid(HPProgress))
	{
		const float Percent = MaxHP > 0
			? FMath::Clamp(
				static_cast<float>(HP) / static_cast<float>(MaxHP),
				0.0f,
				1.0f)
			: 0.0f;
		HPProgress->SetPercent(Percent);
	}

	ApplyNativeBlockValue(BlockText, Block);
}

void UBattleHUDWidget::CleanupNativeDamageTransientVisuals()
{
	if (IsValid(Txt_DamagePresentation))
	{
		Txt_DamagePresentation->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UBattleHUDCombatantPresentationWidgetBase* Target = ActiveDamageTargetWidget.Get())
	{
		Target->SetRenderOpacity(1.0f);
	}
}

void UBattleHUDWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token
)
{
	if (!bHasActiveNativePresentation || Token != ActiveNativePresentationToken)
	{
		return;
	}

	const EBattlePresentationRecordType CancelledType = ActiveNativePresentationType;
	ClearNativePresentationFinishTimer();
	CancelNativePresentationVisual(CancelledType);

	// Exact cancellation abandons the current playback chain. CardPlayed is the
	// only R8 transient intentionally retained across Record boundaries, so a
	// Skip/fail-safe Cancel during a later Damage/Draw/etc. must retire it here
	// instead of leaving a stale card in OV_PlayArea after Controller collapse.
	if (UBattleCardWidget* RetainedPlayedCard = NativePlayedCardWidget.Get())
	{
		RetainedPlayedCard->RemoveFromParent();
	}
	NativePlayedCardWidget.Reset();

	ResetNativePresentationOwnership();
	// Cancellation never notifies normal completion.
}

bool UBattleHUDWidget::CommitNativePresentationOwnership(
	EBattlePresentationRecordType RecordType,
	const FPresentationPlaybackToken& Token
)
{
	if (bHasActiveNativePresentation
		|| NativePresentationFinishTimer.IsValid()
		|| RecordType == EBattlePresentationRecordType::None)
	{
		return false;
	}

	bHasActiveNativePresentation = true;
	ActiveNativePresentationType = RecordType;
	ActiveNativePresentationToken = Token;
	return true;
}

bool UBattleHUDWidget::StartNativePresentationFinishTimer(float DurationSeconds)
{
	if (!bHasActiveNativePresentation
		|| NativePresentationFinishTimer.IsValid()
		|| DurationSeconds <= 0.0f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	// Capture the exact Token by value. A callback from an older presentation
	// can therefore never infer or finish whichever Token happens to be active
	// when the timer eventually fires.
	const FPresentationPlaybackToken ExpectedToken = ActiveNativePresentationToken;
	TWeakObjectPtr<UBattleHUDWidget> WeakThis(this);
	FTimerDelegate FinishDelegate = FTimerDelegate::CreateLambda(
		[WeakThis, ExpectedToken]()
		{
			if (UBattleHUDWidget* Widget = WeakThis.Get())
			{
				Widget->FinishNativePresentation(ExpectedToken);
			}
		});
	World->GetTimerManager().SetTimer(
		NativePresentationFinishTimer,
		FinishDelegate,
		DurationSeconds,
		false);

	return NativePresentationFinishTimer.IsValid();
}

void UBattleHUDWidget::AbortNativePresentationStart()
{
	// A per-Record handler must undo any visible mutation it made before calling
	// this helper. R5 itself creates no visual/transient state, so aborting the
	// kernel leaves zero local side effects and never notifies completion.
	ClearNativePresentationFinishTimer();
	ResetNativePresentationOwnership();
}

void UBattleHUDWidget::FinishNativePresentation(
	const FPresentationPlaybackToken& ExpectedToken
)
{
	if (!bHasActiveNativePresentation
		|| ExpectedToken != ActiveNativePresentationToken)
	{
		return;
	}

	FinishNativePresentationVisual(ActiveNativePresentationType);

	const FPresentationPlaybackToken CompletedToken = ActiveNativePresentationToken;
	ClearNativePresentationFinishTimer();
	ResetNativePresentationOwnership();

	// Keep the existing base deferred bridge. Never call the Controller directly.
	NotifyPresentationFinished(CompletedToken);
}

void UBattleHUDWidget::ClearNativePresentationFinishTimer()
{
	if (NativePresentationFinishTimer.IsValid())
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			World->GetTimerManager().ClearTimer(NativePresentationFinishTimer);
		}
		NativePresentationFinishTimer.Invalidate();
	}
}

void UBattleHUDWidget::ResetNativePresentationOwnership()
{
	bHasActiveNativePresentation = false;
	ActiveNativePresentationType = EBattlePresentationRecordType::None;
	ActiveNativePresentationToken = FPresentationPlaybackToken{};
}

void UBattleHUDWidget::FinishNativePresentationVisual(
	EBattlePresentationRecordType RecordType
)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		|| RecordType == EBattlePresentationRecordType::CardZoneChanged)
	{
		FinishNativeCardPresentation(RecordType);
		return;
	}

	switch (RecordType)
	{
	case EBattlePresentationRecordType::Damage:
		ApplyNativeCombatantVitals(
			ActiveDamageTargetHPProgress.Get(),
			ActiveDamageTargetHPText.Get(),
			ActiveDamageTargetBlockText.Get(),
			ActiveDamageHPAfter,
			ActiveDamageMaxHP,
			ActiveDamageBlockAfter);
		CleanupNativeDamageTransientVisuals();
		ResetNativeDamagePresentationState();
		break;
	case EBattlePresentationRecordType::EnergyChanged:
		ApplyNativeEnergyValue(ActiveNativeSimplePrimaryAfter, ActiveNativeSimpleEnergyMax);
		break;
	case EBattlePresentationRecordType::BlockChanged:
		ApplyNativeBlockValue(
			ActiveNativeSimpleBlockText.Get(),
			ActiveNativeSimplePrimaryAfter);
		break;
	case EBattlePresentationRecordType::DeckShuffled:
		ApplyNativePileCounts(
			ActiveNativeSimplePrimaryAfter,
			ActiveNativeSimpleSecondaryAfter);
		break;
	default:
		break;
	}
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::CancelNativePresentationVisual(
	EBattlePresentationRecordType RecordType
)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		|| RecordType == EBattlePresentationRecordType::CardZoneChanged)
	{
		CancelNativeCardPresentation(RecordType);
		return;
	}

	switch (RecordType)
	{
	case EBattlePresentationRecordType::Damage:
		ApplyNativeCombatantVitals(
			ActiveDamageTargetHPProgress.Get(),
			ActiveDamageTargetHPText.Get(),
			ActiveDamageTargetBlockText.Get(),
			ActiveDamageHPBefore,
			ActiveDamageMaxHP,
			ActiveDamageBlockBefore);
		CleanupNativeDamageTransientVisuals();
		ResetNativeDamagePresentationState();
		break;
	case EBattlePresentationRecordType::EnergyChanged:
		ApplyNativeEnergyValue(ActiveNativeSimplePrimaryBefore, ActiveNativeSimpleEnergyMax);
		break;
	case EBattlePresentationRecordType::BlockChanged:
		ApplyNativeBlockValue(
			ActiveNativeSimpleBlockText.Get(),
			ActiveNativeSimplePrimaryBefore);
		break;
	case EBattlePresentationRecordType::DeckShuffled:
		ApplyNativePileCounts(
			ActiveNativeSimplePrimaryBefore,
			ActiveNativeSimpleSecondaryBefore);
		break;
	default:
		break;
	}
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::CleanupNativePresentationVisualsOnDestruct()
{
	// Destruction is local cleanup only. Damage transient feedback is hidden and
	// its target opacity is normalized, but committed/historical vitals are not
	// restored and normal completion is not notified.
	CleanupNativeDamageTransientVisuals();
	ResetNativeDamagePresentationState();
	CleanupNativeCardPresentationOnDestruct();
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::FinishNativeCardPresentation(
	EBattlePresentationRecordType RecordType)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		&& ActiveNativeCardPresentationKind == ENativeCardPresentationKind::CardPlayed)
	{
		// The frozen PlayArea Widget is deliberately retained for the later
		// PlayArea-to-destination Record. The formal historical Hand reference is
		// no longer needed after exact completion.
		NormalizeNativeCardTransform(NativePlayedCardWidget.Get());
		ActiveNativeHistoricalHandCardWidget.Reset();
		ResetNativeCardRecordState();
		return;
	}

	if (RecordType != EBattlePresentationRecordType::CardZoneChanged)
	{
		ResetNativeCardRecordState();
		return;
	}

	switch (ActiveNativeCardPresentationKind)
	{
	case ENativeCardPresentationKind::HandToDiscard:
		// Preserve the hidden committed After until the Controller applies this
		// Record's snapshot. Do not proactively rebuild the Hand.
		if (UBattleCardWidget* HistoricalCard = ActiveNativeHistoricalHandCardWidget.Get())
		{
			HistoricalCard->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UBattleCardWidget* ZoneCard = ActiveNativeZoneCardWidget.Get())
		{
			ZoneCard->RemoveFromParent();
		}
		ActiveNativeHistoricalHandCardWidget.Reset();
		break;
	case ENativeCardPresentationKind::DrawToHand:
		NormalizeNativeCardTransform(ActiveNativeDrawnCardWidget.Get());
		// Keep this one presentation-only card in the Hand panel until the
		// Controller applies this exact Record and RefreshHand replaces it with a
		// formal card. No later draw can begin before the exact Notify boundary.
		ActiveNativeDrawnCardWidget.Reset();
		break;
	case ENativeCardPresentationKind::PlayAreaToDestination:
		if (UBattleCardWidget* PlayedCard = NativePlayedCardWidget.Get())
		{
			PlayedCard->RemoveFromParent();
		}
		NativePlayedCardWidget.Reset();
		break;
	default:
		break;
	}
	ResetNativeCardRecordState();
}

void UBattleHUDWidget::CancelNativeCardPresentation(
	EBattlePresentationRecordType RecordType)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		&& ActiveNativeCardPresentationKind == ENativeCardPresentationKind::CardPlayed)
	{
		if (UBattleCardWidget* HistoricalCard = ActiveNativeHistoricalHandCardWidget.Get())
		{
			HistoricalCard->SetVisibility(ActiveNativeHistoricalHandVisibility);
		}
		if (UBattleCardWidget* PlayedCard = NativePlayedCardWidget.Get())
		{
			PlayedCard->RemoveFromParent();
		}
		NativePlayedCardWidget.Reset();
		ResetNativeCardRecordState();
		return;
	}

	if (RecordType == EBattlePresentationRecordType::CardZoneChanged)
	{
		switch (ActiveNativeCardPresentationKind)
		{
		case ENativeCardPresentationKind::HandToDiscard:
			if (UBattleCardWidget* HistoricalCard = ActiveNativeHistoricalHandCardWidget.Get())
			{
				HistoricalCard->SetVisibility(ActiveNativeHistoricalHandVisibility);
			}
			if (UBattleCardWidget* ZoneCard = ActiveNativeZoneCardWidget.Get())
			{
				ZoneCard->RemoveFromParent();
			}
			break;
		case ENativeCardPresentationKind::DrawToHand:
			if (IsValid(ViewModel))
			{
				ApplyNativePileCounts(ActiveNativeDrawCountBefore, ViewModel->DiscardCount);
			}
			if (UBattleCardWidget* DrawnCard = ActiveNativeDrawnCardWidget.Get())
			{
				DrawnCard->RemoveFromParent();
			}
			break;
		case ENativeCardPresentationKind::PlayAreaToDestination:
			// A collapse/skip cancellation owns local transient cleanup. It never
			// notifies completion and must not leak the prior CardPlayed visual.
			if (UBattleCardWidget* PlayedCard = NativePlayedCardWidget.Get())
			{
				PlayedCard->RemoveFromParent();
			}
			NativePlayedCardWidget.Reset();
			break;
		default:
			break;
		}
	}
	ResetNativeCardRecordState();
}

void UBattleHUDWidget::CleanupNativeCardPresentationOnDestruct()
{
	// NativeDestruct is local cleanup only: presentation transients are removed,
	// but a hidden/collapsed historical formal Hand Widget is never restored and
	// normal completion is never notified.
	if (UBattleCardWidget* DrawnCard = ActiveNativeDrawnCardWidget.Get())
	{
		DrawnCard->RemoveFromParent();
	}
	if (UBattleCardWidget* ZoneCard = ActiveNativeZoneCardWidget.Get())
	{
		ZoneCard->RemoveFromParent();
	}
	if (UBattleCardWidget* PlayedCard = NativePlayedCardWidget.Get())
	{
		PlayedCard->RemoveFromParent();
	}
	NativePlayedCardWidget.Reset();
	ResetNativeCardRecordState();
}

void UBattleHUDWidget::ResetNativeCardRecordState()
{
	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::None;
	ActiveNativeHistoricalHandCardWidget.Reset();
	ActiveNativeDrawnCardWidget.Reset();
	ActiveNativeZoneCardWidget.Reset();
	ActiveNativeMovingCardWidget.Reset();
	ActiveNativeCardAnimationStartAnchor.Reset();
	ActiveNativeCardAnimationEndAnchor.Reset();
	ActiveNativeHistoricalHandVisibility = ESlateVisibility::Visible;
	ActiveNativeDrawCountBefore = 0;
	ActiveNativeDrawCountAfter = 0;
	ActiveNativeCardDestinationIndex = INDEX_NONE;
	ActiveNativeCardAnimationElapsedSeconds = 0.0f;
	ActiveNativeCardAnimationStartTranslation = FVector2D::ZeroVector;
	ActiveNativeCardAnimationEndTranslation = FVector2D::ZeroVector;
	ActiveNativeCardAnimationFallbackStart = FVector2D::ZeroVector;
	ActiveNativeCardAnimationFallbackEnd = FVector2D::ZeroVector;
	ActiveNativeCardAnimationStartScale = 1.0f;
	ActiveNativeCardAnimationEndScale = 1.0f;
	ActiveNativeCardAnimationStartOpacity = 1.0f;
	ActiveNativeCardAnimationEndOpacity = 1.0f;
	bNativeCardAnimationInitialized = false;
}

void UBattleHUDWidget::ResetNativeSimplePresentationState()
{
	ActiveNativeSimpleBlockText.Reset();
	ActiveNativeSimplePrimaryBefore = 0;
	ActiveNativeSimplePrimaryAfter = 0;
	ActiveNativeSimpleSecondaryBefore = 0;
	ActiveNativeSimpleSecondaryAfter = 0;
	ActiveNativeSimpleEnergyMax = 0;
}

void UBattleHUDWidget::ResetNativeDamagePresentationState()
{
	ActiveDamageTargetWidget.Reset();
	ActiveDamageTargetHPProgress.Reset();
	ActiveDamageTargetHPText.Reset();
	ActiveDamageTargetBlockText.Reset();
	ActiveDamageHPBefore = 0;
	ActiveDamageHPAfter = 0;
	ActiveDamageBlockBefore = 0;
	ActiveDamageBlockAfter = 0;
	ActiveDamageMaxHP = 0;
}

#undef LOCTEXT_NAMESPACE
