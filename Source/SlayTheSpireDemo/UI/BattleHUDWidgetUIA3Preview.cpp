#include "BattleHUDWidget.h"

#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "BattleImmediatePreviewTextBlock.h"
#include "Components/Overlay.h"

void UBattleHUDWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	// A3 uses dedicated Preview delegates. Inspection remains independently bound
	// by the existing NativeConstruct path and is never the Preview authority.
	if (IsValid(Combatant_PlayerPresentation))
	{
		Combatant_PlayerPresentation->OnPreviewRequested.AddUniqueDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewRequested);
		Combatant_PlayerPresentation->OnPreviewCleared.AddUniqueDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewCleared);
	}
	if (IsValid(Combatant_EnemyPresentation))
	{
		Combatant_EnemyPresentation->OnPreviewRequested.AddUniqueDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewRequested);
		Combatant_EnemyPresentation->OnPreviewCleared.AddUniqueDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewCleared);
	}
}

void UBattleHUDWidget::BeginDestroy()
{
	if (IsValid(Combatant_PlayerPresentation))
	{
		Combatant_PlayerPresentation->OnPreviewRequested.RemoveDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewRequested);
		Combatant_PlayerPresentation->OnPreviewCleared.RemoveDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewCleared);
	}
	if (IsValid(Combatant_EnemyPresentation))
	{
		Combatant_EnemyPresentation->OnPreviewRequested.RemoveDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewRequested);
		Combatant_EnemyPresentation->OnPreviewCleared.RemoveDynamic(
			this,
			&UBattleHUDWidget::HandleCombatantPreviewCleared);
	}

	ReleaseImmediatePreviewSurface();
	Super::BeginDestroy();
}

void UBattleHUDWidget::EnsureImmediatePreviewSurface()
{
	if (!IsValid(ViewModel) || !IsValid(OV_PlayArea))
	{
		return;
	}

	if (!IsValid(ImmediatePreviewText))
	{
		ImmediatePreviewText = NewObject<UBattleImmediatePreviewTextBlock>(
			this,
			TEXT("Txt_ImmediatePreview_Runtime"));
	}

	if (IsValid(ImmediatePreviewText))
	{
		ImmediatePreviewText->AttachToPreview(ViewModel, OV_PlayArea);
	}
}

void UBattleHUDWidget::ReleaseImmediatePreviewSurface()
{
	if (IsValid(ImmediatePreviewText))
	{
		ImmediatePreviewText->DetachFromPreview();
		ImmediatePreviewText = nullptr;
	}
}

void UBattleHUDWidget::HandleCombatantPreviewRequested(int32 TargetId)
{
	if (!bNativeBindingsValid
		|| TargetId == INDEX_NONE
		|| !IsValid(ViewModel)
		|| !IsValid(OV_PlayArea))
	{
		return;
	}

	EnsureImmediatePreviewSurface();
	ViewModel->SetPreviewTargetById(TargetId);
}

void UBattleHUDWidget::HandleCombatantPreviewCleared()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	// The text surface observes the same ViewModel delegate and removes itself
	// synchronously from OV_PlayArea when this clears the A3 DTO.
	ViewModel->ClearPreviewTarget();
}
