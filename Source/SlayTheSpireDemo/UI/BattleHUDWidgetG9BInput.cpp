#include "BattleHUDWidgetG9BInput.h"

#include "BattleHUDWidget.h"
#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"
#include "BattleBufferedPlayerIntent.h"
#include "Containers/Ticker.h"

namespace
{
	struct FG9BWidgetWakeState
	{
		TWeakObjectPtr<UBattleHUDViewModel> BoundViewModel;
		FDelegateHandle ViewModelChangedHandle;
		bool bReplayScheduled = false;
	};

	TMap<TWeakObjectPtr<UBattleHUDWidget>, FG9BWidgetWakeState> GWakeStates;
	TSet<TWeakObjectPtr<UBattleHUDWidget>> GFastInputRetirementFences;
	bool bG9BEnabled = true;

	void RemoveWakeState(UBattleHUDWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
		if (FG9BWidgetWakeState* State = GWakeStates.Find(Key))
		{
			if (UBattleHUDViewModel* BoundViewModel = State->BoundViewModel.Get();
				IsValid(BoundViewModel) && State->ViewModelChangedHandle.IsValid())
			{
				BoundViewModel->OnNativeChanged.Remove(State->ViewModelChangedHandle);
			}
		}
		GWakeStates.Remove(Key);
	}

	void PruneDeadState()
	{
		for (auto It = GWakeStates.CreateIterator(); It; ++It)
		{
			if (It.Key().IsValid())
			{
				continue;
			}
			if (UBattleHUDViewModel* BoundViewModel = It.Value().BoundViewModel.Get();
				IsValid(BoundViewModel) && It.Value().ViewModelChangedHandle.IsValid())
			{
				BoundViewModel->OnNativeChanged.Remove(It.Value().ViewModelChangedHandle);
			}
			It.RemoveCurrent();
		}

		for (auto It = GFastInputRetirementFences.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	bool IsNormalCardInputAvailableNow(const UBattleHUDViewModel* ViewModel)
	{
		return IsValid(ViewModel)
			&& ViewModel->Outcome == EBattleHUDOutcome::None
			&& !ViewModel->bInputLocked
			&& ViewModel->InteractionState != EBattleHUDInteractionState::Resolving
			&& ViewModel->InteractionState != EBattleHUDInteractionState::Terminal
			&& ViewModel->InteractionState != EBattleHUDInteractionState::PresentationUnavailable;
	}

	void ProcessScheduledReplay(const TWeakObjectPtr<UBattleHUDWidget>& WeakWidget)
	{
		UBattleHUDWidget* Widget = WeakWidget.Get();
		if (!IsValid(Widget))
		{
			PruneDeadState();
			return;
		}

		FG9BWidgetWakeState* State = GWakeStates.Find(WeakWidget);
		if (State == nullptr)
		{
			return;
		}
		State->bReplayScheduled = false;

		UBattleHUDViewModel* ViewModel = Widget->ViewModel.Get();
		if (!IsValid(ViewModel))
		{
			RemoveWakeState(Widget);
			return;
		}

		ViewModel->RefreshBufferedPlayerIntentG9();
		if (!ViewModel->HasBufferedCardSelectionG9())
		{
			RemoveWakeState(Widget);
		}
	}

	void ScheduleExactReplayCheck(UBattleHUDWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
		FG9BWidgetWakeState* State = GWakeStates.Find(Key);
		if (State == nullptr || State->bReplayScheduled)
		{
			return;
		}
		State->bReplayScheduled = true;

		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[Key](float /*DeltaSeconds*/)
				{
					ProcessScheduledReplay(Key);
					return false;
				}),
			0.0f);
	}

	void BindViewModelWakeup(UBattleHUDWidget* Widget)
	{
		if (!IsValid(Widget) || !IsValid(Widget->ViewModel))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
		FG9BWidgetWakeState& State = GWakeStates.FindOrAdd(Key);
		UBattleHUDViewModel* DesiredViewModel = Widget->ViewModel.Get();
		if (State.BoundViewModel.Get() == DesiredViewModel && State.ViewModelChangedHandle.IsValid())
		{
			return;
		}

		if (UBattleHUDViewModel* Previous = State.BoundViewModel.Get();
			IsValid(Previous) && State.ViewModelChangedHandle.IsValid())
		{
			Previous->OnNativeChanged.Remove(State.ViewModelChangedHandle);
		}
		State.BoundViewModel = DesiredViewModel;
		State.ViewModelChangedHandle.Reset();

		const TWeakObjectPtr<UBattleHUDWidget> WeakWidget(Widget);
		State.ViewModelChangedHandle = DesiredViewModel->OnNativeChanged.AddLambda(
			[WeakWidget](EBattleHUDDirtyFlags /*DirtyFlags*/)
			{
				if (UBattleHUDWidget* CurrentWidget = WeakWidget.Get())
				{
					BattleHUDWidgetG9BInput::RefreshBufferedCardIntent(CurrentWidget);
				}
				else
				{
					PruneDeadState();
				}
			});
	}
}

bool BattleHUDWidgetG9BInput::IsEnabled()
{
	return bG9BEnabled;
}

EG9BCardClickDisposition BattleHUDWidgetG9BInput::TryHandleBufferedCardClick(
	UBattleHUDWidget* Widget,
	int32 RuntimeId)
{
	PruneDeadState();
	if (!bG9BEnabled
		|| !IsValid(Widget)
		|| RuntimeId == INDEX_NONE
		|| !IsValid(Widget->ViewModel))
	{
		return EG9BCardClickDisposition::NotHandled;
	}

	UBattleHUDViewModel* ViewModel = Widget->ViewModel.Get();
	if (ViewModel->HasBufferedEndTurnG9())
	{
		return EG9BCardClickDisposition::Rejected;
	}

	if (IsNormalCardInputAvailableNow(ViewModel))
	{
		if (ViewModel->HasBufferedCardSelectionG9())
		{
			ViewModel->ClearBufferedPlayerIntentG9();
		}
		RemoveWakeState(Widget);
		return EG9BCardClickDisposition::NotHandled;
	}

	UBattlePresentationController* Controller = Widget->PresentationController.Get();
	FBufferedCardIntent Captured;
	if (!IsValid(Controller)
		|| !Controller->TryCaptureBufferedCardTarget(RuntimeId, Captured))
	{
		// Every physical card click re-captures authority. A click outside an exact
		// G9 window retires an older card credential before normal G8 FastInput is
		// considered for this new input.
		if (ViewModel->HasBufferedCardSelectionG9())
		{
			ViewModel->ClearBufferedPlayerIntentG9();
		}
		RemoveWakeState(Widget);
		return EG9BCardClickDisposition::NotHandled;
	}

	if (!ViewModel->StoreBufferedCardSelectionG9(Captured, Controller))
	{
		return ViewModel->HasBufferedEndTurnG9()
			? EG9BCardClickDisposition::Rejected
			: EG9BCardClickDisposition::NotHandled;
	}

	BindViewModelWakeup(Widget);
	return EG9BCardClickDisposition::Buffered;
}

bool BattleHUDWidgetG9BInput::CanAcceptEndTurn(const UBattleHUDWidget* Widget)
{
	return bG9BEnabled
		&& IsValid(Widget)
		&& IsValid(Widget->ViewModel)
		&& Widget->ViewModel->CanAcceptEndTurnIntentG9();
}

bool BattleHUDWidgetG9BInput::TryHandleEndTurn(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (!bG9BEnabled || !IsValid(Widget) || !IsValid(Widget->ViewModel))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = Widget->ViewModel.Get();
	if (!ViewModel->TryAcceptEndTurnIntentG9())
	{
		return false;
	}

	// EndTurn has now been accepted against exact player-turn authority. It owns
	// priority over the older card future work from this input owner.
	RemoveWakeState(Widget);
	MarkFastInputRetiredByEndTurn(Widget);
	ViewModel->FinalizeAcceptedEndTurnIntentG9();
	return true;
}

void BattleHUDWidgetG9BInput::RefreshBufferedCardIntent(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (!bG9BEnabled || !IsValid(Widget) || !IsValid(Widget->ViewModel))
	{
		return;
	}

	if (!Widget->ViewModel->HasBufferedCardSelectionG9())
	{
		RemoveWakeState(Widget);
		return;
	}

	// Defer exact replay/drop outside the synchronous ViewModel multicast stack.
	ScheduleExactReplayCheck(Widget);
}

void BattleHUDWidgetG9BInput::ClearBufferedCardIntent(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (IsValid(Widget) && IsValid(Widget->ViewModel)
		&& Widget->ViewModel->HasBufferedCardSelectionG9())
	{
		Widget->ViewModel->ClearBufferedPlayerIntentG9();
	}
	RemoveWakeState(Widget);
}

bool BattleHUDWidgetG9BInput::HasBufferedCardIntent(const UBattleHUDWidget* Widget)
{
	PruneDeadState();
	return IsValid(Widget)
		&& IsValid(Widget->ViewModel)
		&& Widget->ViewModel->HasBufferedCardSelectionG9();
}

void BattleHUDWidgetG9BInput::MarkFastInputRetiredByEndTurn(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (IsValid(Widget))
	{
		GFastInputRetirementFences.Add(TWeakObjectPtr<UBattleHUDWidget>(Widget));
	}
}

bool BattleHUDWidgetG9BInput::ConsumeFastInputRetirementFence(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (!IsValid(Widget))
	{
		return false;
	}
	return GFastInputRetirementFences.Remove(TWeakObjectPtr<UBattleHUDWidget>(Widget)) > 0;
}

void BattleHUDWidgetG9BInput::ClearFastInputRetirementFence(UBattleHUDWidget* Widget)
{
	PruneDeadState();
	if (IsValid(Widget))
	{
		GFastInputRetirementFences.Remove(TWeakObjectPtr<UBattleHUDWidget>(Widget));
	}
}

void BattleHUDWidgetG9BInput::ClearWidgetState(UBattleHUDWidget* Widget)
{
	if (!IsValid(Widget))
	{
		return;
	}
	if (IsValid(Widget->ViewModel))
	{
		Widget->ViewModel->ClearBufferedPlayerIntentG9();
	}
	RemoveWakeState(Widget);
	ClearFastInputRetirementFence(Widget);
}

#if WITH_DEV_AUTOMATION_TESTS
void BattleHUDWidgetG9BInput::SetEnabledForTesting(bool bEnabled)
{
	bG9BEnabled = bEnabled;
}
#endif
