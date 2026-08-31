#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleStatusWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "BattleHUDWidget"

namespace
{
	constexpr float SimpleNativePresentationDurationSeconds = 0.5f;
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

			if (IsValid(HPText))
			{
				HPText->SetText(FText::Format(
					LOCTEXT("BattleHUDHPFormat", "{0}/{1}"),
					FText::AsNumber(Combatant.HP),
					FText::AsNumber(Combatant.MaxHP)));
			}

			if (IsValid(HPProgress))
			{
				const float Percent = Combatant.MaxHP > 0
					? FMath::Clamp(
						static_cast<float>(Combatant.HP) / static_cast<float>(Combatant.MaxHP),
						0.0f,
						1.0f)
					: 0.0f;
				HPProgress->SetPercent(Percent);
			}

			ApplyNativeBlockValue(BlockText, Combatant.Block);
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
	case EBattlePresentationRecordType::EnergyChanged:
		return BeginNativeEnergyChangedPresentation(Record, Token);
	case EBattlePresentationRecordType::BlockChanged:
		return BeginNativeBlockChangedPresentation(Record, Token);
	case EBattlePresentationRecordType::DeckShuffled:
		return BeginNativeDeckShuffledPresentation(Record, Token);
	default:
		// R7+ Records remain on the Controller's immediate-fallback path.
		return false;
	}
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

	if (!StartNativePresentationFinishTimer(SimpleNativePresentationDurationSeconds))
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

	if (!StartNativePresentationFinishTimer(SimpleNativePresentationDurationSeconds))
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

	if (!StartNativePresentationFinishTimer(SimpleNativePresentationDurationSeconds))
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
	switch (RecordType)
	{
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
	switch (RecordType)
	{
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
	// Destruction is local cleanup only: discard frozen visual context without
	// restoring history or notifying normal completion.
	ResetNativeSimplePresentationState();
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

#undef LOCTEXT_NAMESPACE
