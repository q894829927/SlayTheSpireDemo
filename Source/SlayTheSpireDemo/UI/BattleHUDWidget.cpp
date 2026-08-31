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

	bool AreNativeStatusViewsEqual(
		const FBattleHUDStatusView& Left,
		const FBattleHUDStatusView& Right)
	{
		return Left.StatusId == Right.StatusId
			&& Left.RuntimeSequence == Right.RuntimeSequence
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.Description.EqualTo(Right.Description)
			&& Left.Amount == Right.Amount
			&& Left.bUseAtlasIcon == Right.bUseAtlasIcon
			&& Left.UVOffset == Right.UVOffset
			&& Left.UVScale == Right.UVScale
			&& Left.TrimOffset == Right.TrimOffset
			&& Left.TrimScale == Right.TrimScale;
	}

	bool DoesNativeStatusViewMatchBeforePayload(
		const FBattleHUDStatusView& View,
		const FStatusChangedPresentationPayload& Payload)
	{
		return View.StatusId == Payload.StatusId
			&& View.RuntimeSequence == Payload.RuntimeSequence
			&& View.DisplayName.EqualTo(Payload.DisplayName)
			&& View.Description.EqualTo(Payload.DescriptionBefore)
			&& View.Amount == Payload.AmountBefore
			&& View.bUseAtlasIcon == Payload.bUseAtlasIcon
			&& View.UVOffset == Payload.UVOffset
			&& View.UVScale == Payload.UVScale
			&& View.TrimOffset == Payload.TrimOffset
			&& View.TrimScale == Payload.TrimScale;
	}

	FBattleHUDStatusView MakeNativeStatusAfterView(
		const FStatusChangedPresentationPayload& Payload)
	{
		FBattleHUDStatusView View;
		View.StatusId = Payload.StatusId;
		View.RuntimeSequence = Payload.RuntimeSequence;
		View.DisplayName = Payload.DisplayName;
		View.Description = Payload.DescriptionAfter;
		View.Amount = Payload.AmountAfter;
		View.bUseAtlasIcon = Payload.bUseAtlasIcon;
		View.UVOffset = Payload.UVOffset;
		View.UVScale = Payload.UVScale;
		View.TrimOffset = Payload.TrimOffset;
		View.TrimScale = Payload.TrimScale;
		return View;
	}

	bool IsNativeStatusReasonValid(const FStatusChangedPresentationPayload& Payload)
	{
		switch (Payload.Reason)
		{
		case EStatusChangeReason::Applied:
			return Payload.bCreated
				&& !Payload.bRemoved
				&& Payload.AmountBefore == 0
				&& Payload.AmountAfter > 0;
		case EStatusChangeReason::Increased:
			return !Payload.bCreated
				&& !Payload.bRemoved
				&& Payload.AmountBefore > 0
				&& Payload.AmountAfter > Payload.AmountBefore;
		case EStatusChangeReason::Reduced:
		case EStatusChangeReason::TurnEndDecay:
			return !Payload.bCreated
				&& Payload.AmountBefore > Payload.AmountAfter
				&& Payload.AmountAfter >= 0
				&& Payload.bRemoved == (Payload.AmountAfter == 0);
		case EStatusChangeReason::Removed:
			return !Payload.bCreated
				&& Payload.bRemoved
				&& Payload.AmountBefore > 0
				&& Payload.AmountAfter == 0;
		default:
			return false;
		}
	}
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
	RefreshHUDFromViewModel();
}

void UBattleHUDWidget::NativeDestruct()
{
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
	RefreshStatusRows();
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

void UBattleHUDWidget::RefreshStatusRows()
{
	if (!IsValid(ViewModel) || StatusWidgetClass == nullptr)
	{
		return;
	}

	const bool bPlayerOk = RebuildNativeStatusRows(WB_PlayerStatuses, ViewModel->Player.Statuses);
	const bool bEnemyOk = RebuildNativeStatusRows(WB_EnemyStatuses, ViewModel->Enemy.Statuses);
	if (!bPlayerOk || !bEnemyOk)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleHUD][Native] Failed to rebuild formal Native Status rows from frozen ViewModel."));
	}
}

bool UBattleHUDWidget::RebuildNativeStatusRows(
	UWrapBox* Container,
	const TArray<FBattleHUDStatusView>& Statuses)
{
	if (!IsValid(Container) || StatusWidgetClass == nullptr)
	{
		return false;
	}

	Container->ClearChildren();
	for (const FBattleHUDStatusView& StatusView : Statuses)
	{
		UBattleStatusWidget* StatusWidget = CreateNativeStatusWidget(StatusView);
		if (!IsValid(StatusWidget) || Container->AddChildToWrapBox(StatusWidget) == nullptr)
		{
			Container->ClearChildren();
			return false;
		}
	}
	return true;
}

UBattleStatusWidget* UBattleHUDWidget::CreateNativeStatusWidget(
	const FBattleHUDStatusView& View) const
{
	if (StatusWidgetClass == nullptr)
	{
		return nullptr;
	}

	UBattleStatusWidget* StatusWidget = nullptr;
	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		StatusWidget = CreateWidget<UBattleStatusWidget>(OwningPlayer, StatusWidgetClass);
	}
	else if (UWorld* World = GetWorld(); IsValid(World))
	{
		StatusWidget = CreateWidget<UBattleStatusWidget>(World, StatusWidgetClass);
	}

	if (IsValid(StatusWidget))
	{
		StatusWidget->SetStatusView(View);
	}
	return StatusWidget;
}

bool UBattleHUDWidget::ResolveNativeStatusTarget(
	FName PresentationId,
	UWrapBox*& OutContainer,
	const FBattleHUDCombatantView*& OutHistoricalView) const
{
	OutContainer = nullptr;
	OutHistoricalView = nullptr;
	if (!IsValid(ViewModel) || PresentationId.IsNone())
	{
		return false;
	}

	const bool bPlayer = ViewModel->Player.PresentationId == PresentationId;
	const bool bEnemy = ViewModel->Enemy.PresentationId == PresentationId;
	if (bPlayer == bEnemy)
	{
		return false;
	}

	if (bPlayer)
	{
		OutContainer = WB_PlayerStatuses;
		OutHistoricalView = &ViewModel->Player;
	}
	else
	{
		OutContainer = WB_EnemyStatuses;
		OutHistoricalView = &ViewModel->Enemy;
	}

	return IsValid(OutContainer) && OutHistoricalView != nullptr;
}

int32 UBattleHUDWidget::CountHistoricalStatusIdentity(
	const TArray<FBattleHUDStatusView>& Statuses,
	FName StatusId,
	int64 RuntimeSequence) const
{
	int32 Count = 0;
	for (const FBattleHUDStatusView& Status : Statuses)
	{
		if (Status.StatusId == StatusId && Status.RuntimeSequence == RuntimeSequence)
		{
			++Count;
		}
	}
	return Count;
}

int32 UBattleHUDWidget::CountNativeStatusWidgetIdentity(
	UWrapBox* Container,
	FName StatusId,
	int64 RuntimeSequence) const
{
	if (!IsValid(Container))
	{
		return 0;
	}

	int32 Count = 0;
	for (int32 Index = 0; Index < Container->GetChildrenCount(); ++Index)
	{
		if (const UBattleStatusWidget* StatusWidget =
			Cast<UBattleStatusWidget>(Container->GetChildAt(Index)))
		{
			if (StatusWidget->GetStatusId() == StatusId
				&& StatusWidget->GetRuntimeSequence() == RuntimeSequence)
			{
				++Count;
			}
		}
	}
	return Count;
}

bool UBattleHUDWidget::FindHistoricalStatusByIdentity(
	const TArray<FBattleHUDStatusView>& Statuses,
	FName StatusId,
	int64 RuntimeSequence,
	const FBattleHUDStatusView*& OutStatus) const
{
	OutStatus = nullptr;
	if (CountHistoricalStatusIdentity(Statuses, StatusId, RuntimeSequence) != 1)
	{
		return false;
	}

	for (const FBattleHUDStatusView& Status : Statuses)
	{
		if (Status.StatusId == StatusId && Status.RuntimeSequence == RuntimeSequence)
		{
			OutStatus = &Status;
			return true;
		}
	}
	return false;
}

bool UBattleHUDWidget::FindNativeStatusWidgetByIdentity(
	UWrapBox* Container,
	FName StatusId,
	int64 RuntimeSequence,
	UBattleStatusWidget*& OutWidget) const
{
	OutWidget = nullptr;
	if (!IsValid(Container)
		|| CountNativeStatusWidgetIdentity(Container, StatusId, RuntimeSequence) != 1)
	{
		return false;
	}

	for (int32 Index = 0; Index < Container->GetChildrenCount(); ++Index)
	{
		UBattleStatusWidget* StatusWidget = Cast<UBattleStatusWidget>(Container->GetChildAt(Index));
		if (IsValid(StatusWidget)
			&& StatusWidget->GetStatusId() == StatusId
			&& StatusWidget->GetRuntimeSequence() == RuntimeSequence)
		{
			OutWidget = StatusWidget;
			return true;
		}
	}
	return false;
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

	static const FName RebuildTooltipFunctionName(TEXT("RebuildTooltip"));
	UFunction* RebuildTooltipFunction = StatusTooltip->FindFunction(RebuildTooltipFunctionName);
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
		const bool bTooltipRefreshed = RefreshStatusTooltip(StatusTooltip, CombatantView->Statuses);
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
	const FPresentationPlaybackToken& Token)
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
	case EBattlePresentationRecordType::StatusChanged:
		return BeginNativeStatusChangedPresentation(Record, Token);
	default:
		// R10+ Records remain on the Controller's immediate-fallback path.
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
		&& FindExactHistoricalHandCard(Payload.Card, Payload.HandIndexBefore, HistoricalHandCard);
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

	if (Payload.FromZone == ECardZone::Hand && Payload.ToZone == ECardZone::DiscardPile)
	{
		return BeginNativeHandToDiscardPresentation(Record, Token);
	}
	if (Payload.FromZone == ECardZone::DrawPile && Payload.ToZone == ECardZone::Hand)
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
	if (!IsValid(PresentationCard) || !CommitNativePresentationOwnership(Record.Type, Token))
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
	if (!IsValid(PresentationCard) || !CommitNativePresentationOwnership(Record.Type, Token))
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
		|| !DoesNativeCardViewMatchSnapshot(PlayedCard->GetCardView(), Payload.Card)
		|| !CommitNativePresentationOwnership(Record.Type, Token))
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

bool UBattleHUDWidget::IsNativeCardSnapshotValid(const FPresentationCardSnapshot& Snapshot) const
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
		|| (NativePlayedCardWidget.IsValid() && NativePlayedCardWidget->GetRuntimeId() == RuntimeId)
		|| (ActiveNativeDrawnCardWidget.IsValid() && ActiveNativeDrawnCardWidget->GetRuntimeId() == RuntimeId)
		|| (ActiveNativeZoneCardWidget.IsValid() && ActiveNativeZoneCardWidget->GetRuntimeId() == RuntimeId))
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

	auto ResolveAnchorTranslation = [MovingCard](UWidget* Anchor, const FVector2D& Fallback) -> FVector2D
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
		const FVector2D AnchorAbsolute = AnchorGeometry.LocalToAbsolute(AnchorGeometry.GetLocalSize() * 0.5f);
		const FVector2D CardAbsolute = CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize() * 0.5f);
		const FVector2D AnchorLocal = FVector2D(CardGeometry.AbsoluteToLocal(AnchorAbsolute));
		const FVector2D CardLocal = FVector2D(CardGeometry.AbsoluteToLocal(CardAbsolute));
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
	UTextBlock* BlockText = ResolveBlockTextForPresentationId(Payload.TargetPresentationId, HistoricalBlock);
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
	if (!bCommonPayloadValid || !bReasonValid || !CommitNativePresentationOwnership(Record.Type, Token))
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
	if (!bCountsValid || !CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}
	ActiveNativeSimplePrimaryBefore = Payload.DrawCountBefore;
	ActiveNativeSimplePrimaryAfter = Payload.DrawCountAfter;
	ActiveNativeSimpleSecondaryBefore = Payload.DiscardCountBefore;
	ActiveNativeSimpleSecondaryAfter = Payload.DiscardCountAfter;
	ApplyNativePileCounts(ActiveNativeSimplePrimaryAfter, ActiveNativeSimpleSecondaryAfter);
	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		ApplyNativePileCounts(ActiveNativeSimplePrimaryBefore, ActiveNativeSimpleSecondaryBefore);
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
	if (!bPayloadValid || !CommitNativePresentationOwnership(Record.Type, Token))
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

bool UBattleHUDWidget::BeginNativeStatusChangedPresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FStatusChangedPresentationPayload& Payload = Record.StatusChanged;
	UWrapBox* TargetContainer = nullptr;
	const FBattleHUDCombatantView* HistoricalTarget = nullptr;
	const bool bTargetValid = ResolveNativeStatusTarget(
		Payload.TargetPresentationId,
		TargetContainer,
		HistoricalTarget);
	const bool bSourceValid = Payload.SourcePresentationId.IsNone()
		|| IsKnownCombatantPresentationId(Payload.SourcePresentationId);
	const bool bCommonPayloadValid =
		bTargetValid
		&& StatusWidgetClass != nullptr
		&& bSourceValid
		&& !Payload.StatusId.IsNone()
		&& Payload.RuntimeSequence > 0
		&& Payload.AmountBefore >= 0
		&& Payload.AmountAfter >= 0
		&& !Payload.DisplayName.IsEmpty()
		&& !(Payload.bCreated && Payload.bRemoved)
		&& (!Payload.bCreated || Payload.DescriptionBefore.IsEmpty())
		&& (!Payload.bRemoved || Payload.DescriptionAfter.IsEmpty())
		&& IsNativeStatusReasonValid(Payload);
	if (!bCommonPayloadValid || HistoricalTarget == nullptr)
	{
		return false;
	}

	const int32 HistoricalMatchCount = CountHistoricalStatusIdentity(
		HistoricalTarget->Statuses,
		Payload.StatusId,
		Payload.RuntimeSequence);
	const int32 WidgetMatchCount = CountNativeStatusWidgetIdentity(
		TargetContainer,
		Payload.StatusId,
		Payload.RuntimeSequence);
	const FBattleHUDStatusView* HistoricalStatus = nullptr;
	UBattleStatusWidget* ExistingWidget = nullptr;

	if (Payload.bCreated)
	{
		if (HistoricalMatchCount != 0 || WidgetMatchCount != 0)
		{
			return false;
		}
	}
	else
	{
		if (HistoricalMatchCount != 1
			|| WidgetMatchCount != 1
			|| !FindHistoricalStatusByIdentity(
				HistoricalTarget->Statuses,
				Payload.StatusId,
				Payload.RuntimeSequence,
				HistoricalStatus)
			|| !FindNativeStatusWidgetByIdentity(
				TargetContainer,
				Payload.StatusId,
				Payload.RuntimeSequence,
				ExistingWidget)
			|| HistoricalStatus == nullptr
			|| !DoesNativeStatusViewMatchBeforePayload(*HistoricalStatus, Payload)
			|| !AreNativeStatusViewsEqual(ExistingWidget->GetStatusView(), *HistoricalStatus)
			|| ExistingWidget->GetVisibility() == ESlateVisibility::Collapsed)
		{
			return false;
		}
	}

	const FBattleHUDStatusView AfterView = MakeNativeStatusAfterView(Payload);
	UBattleStatusWidget* PreparedCreatedWidget = nullptr;
	if (Payload.bCreated)
	{
		PreparedCreatedWidget = CreateNativeStatusWidget(AfterView);
		if (!IsValid(PreparedCreatedWidget))
		{
			return false;
		}
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	bActiveNativeStatusCreatedTransient = Payload.bCreated;
	if (Payload.bCreated)
	{
		if (TargetContainer->AddChildToWrapBox(PreparedCreatedWidget) == nullptr)
		{
			ResetNativeStatusPresentationState();
			AbortNativePresentationStart();
			return false;
		}
		ActiveNativeStatusPresentationWidget = PreparedCreatedWidget;
	}
	else
	{
		ActiveNativeStatusPresentationWidget = ExistingWidget;
		ActiveNativeStatusBeforeView = ExistingWidget->GetStatusView();
		ActiveNativeStatusBeforeVisibility = ExistingWidget->GetVisibility();
		ExistingWidget->SetStatusView(AfterView);
		if (Payload.bRemoved)
		{
			ExistingWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!StartNativePresentationFinishTimer(NativePresentationDurationSeconds))
	{
		if (bActiveNativeStatusCreatedTransient)
		{
			if (UBattleStatusWidget* StatusWidget = ActiveNativeStatusPresentationWidget.Get())
			{
				StatusWidget->RemoveFromParent();
			}
		}
		else if (UBattleStatusWidget* StatusWidget = ActiveNativeStatusPresentationWidget.Get())
		{
			StatusWidget->SetStatusView(ActiveNativeStatusBeforeView);
			StatusWidget->SetVisibility(ActiveNativeStatusBeforeVisibility);
		}
		ResetNativeStatusPresentationState();
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
		Block > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
			? FMath::Clamp(static_cast<float>(HP) / static_cast<float>(MaxHP), 0.0f, 1.0f)
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
	const FPresentationPlaybackToken& Token)
{
	if (!bHasActiveNativePresentation || Token != ActiveNativePresentationToken)
	{
		return;
	}

	const EBattlePresentationRecordType CancelledType = ActiveNativePresentationType;
	ClearNativePresentationFinishTimer();
	CancelNativePresentationVisual(CancelledType);

	if (UBattleCardWidget* RetainedPlayedCard = NativePlayedCardWidget.Get())
	{
		RetainedPlayedCard->RemoveFromParent();
	}
	NativePlayedCardWidget.Reset();
	ResetNativePresentationOwnership();
}

bool UBattleHUDWidget::CommitNativePresentationOwnership(
	EBattlePresentationRecordType RecordType,
	const FPresentationPlaybackToken& Token)
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
	ClearNativePresentationFinishTimer();
	ResetNativePresentationOwnership();
}

void UBattleHUDWidget::FinishNativePresentation(
	const FPresentationPlaybackToken& ExpectedToken)
{
	if (!bHasActiveNativePresentation || ExpectedToken != ActiveNativePresentationToken)
	{
		return;
	}
	FinishNativePresentationVisual(ActiveNativePresentationType);
	const FPresentationPlaybackToken CompletedToken = ActiveNativePresentationToken;
	ClearNativePresentationFinishTimer();
	ResetNativePresentationOwnership();
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

void UBattleHUDWidget::FinishNativePresentationVisual(EBattlePresentationRecordType RecordType)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		|| RecordType == EBattlePresentationRecordType::CardZoneChanged)
	{
		FinishNativeCardPresentation(RecordType);
		return;
	}
	if (RecordType == EBattlePresentationRecordType::StatusChanged)
	{
		FinishNativeStatusPresentation();
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
		ApplyNativeBlockValue(ActiveNativeSimpleBlockText.Get(), ActiveNativeSimplePrimaryAfter);
		break;
	case EBattlePresentationRecordType::DeckShuffled:
		ApplyNativePileCounts(ActiveNativeSimplePrimaryAfter, ActiveNativeSimpleSecondaryAfter);
		break;
	default:
		break;
	}
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::CancelNativePresentationVisual(EBattlePresentationRecordType RecordType)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		|| RecordType == EBattlePresentationRecordType::CardZoneChanged)
	{
		CancelNativeCardPresentation(RecordType);
		return;
	}
	if (RecordType == EBattlePresentationRecordType::StatusChanged)
	{
		CancelNativeStatusPresentation();
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
		ApplyNativeBlockValue(ActiveNativeSimpleBlockText.Get(), ActiveNativeSimplePrimaryBefore);
		break;
	case EBattlePresentationRecordType::DeckShuffled:
		ApplyNativePileCounts(ActiveNativeSimplePrimaryBefore, ActiveNativeSimpleSecondaryBefore);
		break;
	default:
		break;
	}
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::CleanupNativePresentationVisualsOnDestruct()
{
	CleanupNativeDamageTransientVisuals();
	ResetNativeDamagePresentationState();
	CleanupNativeCardPresentationOnDestruct();
	CleanupNativeStatusPresentationOnDestruct();
	ResetNativeSimplePresentationState();
}

void UBattleHUDWidget::FinishNativeStatusPresentation()
{
	// Begin already applied the frozen committed After. Normal completion keeps
	// that visual until Controller reduction/refresh takes formal ownership.
	ResetNativeStatusPresentationState();
}

void UBattleHUDWidget::CancelNativeStatusPresentation()
{
	// Never reverse-compute B -> A. The current ViewModel still represents the
	// historical snapshot for the active Record, so rebuild both formal rows from
	// it exactly and discard any presentation-only create/update/remove residue.
	RefreshStatusRows();
	ResetNativeStatusPresentationState();
}

void UBattleHUDWidget::CleanupNativeStatusPresentationOnDestruct()
{
	if (bActiveNativeStatusCreatedTransient)
	{
		if (UBattleStatusWidget* StatusWidget = ActiveNativeStatusPresentationWidget.Get())
		{
			StatusWidget->RemoveFromParent();
		}
	}
	ResetNativeStatusPresentationState();
}

void UBattleHUDWidget::ResetNativeStatusPresentationState()
{
	ActiveNativeStatusPresentationWidget.Reset();
	ActiveNativeStatusBeforeView = FBattleHUDStatusView{};
	ActiveNativeStatusBeforeVisibility = ESlateVisibility::Visible;
	bActiveNativeStatusCreatedTransient = false;
}

void UBattleHUDWidget::FinishNativeCardPresentation(EBattlePresentationRecordType RecordType)
{
	if (RecordType == EBattlePresentationRecordType::CardPlayed
		&& ActiveNativeCardPresentationKind == ENativeCardPresentationKind::CardPlayed)
	{
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

void UBattleHUDWidget::CancelNativeCardPresentation(EBattlePresentationRecordType RecordType)
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
