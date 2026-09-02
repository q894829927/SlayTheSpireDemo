#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "BattleImmediatePreviewTextBlock.h"
#include "Components/HorizontalBox.h"

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
	if (!IsValid(ViewModel)
		|| !IsValid(HB_Hand)
		|| !ViewModel->bHasImmediatePreview
		|| ViewModel->SelectedCardRuntimeId == INDEX_NONE)
	{
		return;
	}

	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
		if (IsValid(CardWidget)
			&& CardWidget->GetRuntimeId() == ViewModel->SelectedCardRuntimeId)
		{
			CardWidget->ApplyImmediatePreview(ViewModel->ImmediatePreview);
			return;
		}
	}
}

void UBattleHUDWidget::ReleaseImmediatePreviewSurface()
{
	// A3 no longer owns OV_PlayArea. Clear any selected-card face override only.
	if (IsValid(HB_Hand))
	{
		for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
		{
			if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
			{
				CardWidget->ClearImmediatePreview();
			}
		}
	}

	// Compatibility cleanup for an object created by an older hot-reloaded A3-5
	// implementation. New code never creates this standalone surface.
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
		|| !IsValid(HB_Hand))
	{
		return;
	}

	// SetPreviewTargetById may broadcast and rebuild the formal Hand. Apply the
	// card-face override only after that broadcast returns so it lands on the
	// current Hand widget instance rather than a stale pre-refresh card.
	if (ViewModel->SetPreviewTargetById(TargetId))
	{
		EnsureImmediatePreviewSurface();
	}
	else
	{
		ReleaseImmediatePreviewSurface();
	}
}

void UBattleHUDWidget::HandleCombatantPreviewCleared()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	// Restore the selected card face before clearing transient Preview state.
	// Neither step touches OV_PlayArea; committed A2 playback keeps exclusive
	// ownership of that container.
	ReleaseImmediatePreviewSurface();
	ViewModel->ClearPreviewTarget();
}
