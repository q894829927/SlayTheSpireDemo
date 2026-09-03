#include "BattleRelicStripWidget.h"

#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetBase.h"
#include "BattleRelicWidget.h"
#include "Components/HorizontalBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UBattleRelicStripWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid = IsValid(HB_Relics) && RelicWidgetClass != nullptr;
	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Relic Strip '%s' is missing HB_Relics or RelicWidgetClass."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleRelicStrip][Native] Invalid required bindings on '%s'."),
			*GetPathName());
	}
}

void UBattleRelicStripWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindToOwningViewModel();
	RefreshRelics();
}

void UBattleRelicStripWidget::NativeDestruct()
{
	UnbindFromViewModel();
	Super::NativeDestruct();
}

void UBattleRelicStripWidget::HandleViewModelChanged()
{
	RefreshRelics();
}

UBattleHUDWidgetBase* UBattleRelicStripWidget::ResolveOwningBattleHUD() const
{
	for (UObject* OuterObject = GetOuter(); OuterObject != nullptr; OuterObject = OuterObject->GetOuter())
	{
		if (UBattleHUDWidgetBase* HUD = Cast<UBattleHUDWidgetBase>(OuterObject))
		{
			return HUD;
		}
	}
	return nullptr;
}

void UBattleRelicStripWidget::BindToOwningViewModel()
{
	UnbindFromViewModel();

	UBattleHUDWidgetBase* HUD = ResolveOwningBattleHUD();
	if (!IsValid(HUD) || !IsValid(HUD->ViewModel))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicStrip][Native] '%s' could not resolve the owning Battle HUD ViewModel."),
			*GetPathName());
		return;
	}

	BoundViewModel = HUD->ViewModel;
	BoundViewModel->OnChanged.AddUniqueDynamic(
		this,
		&UBattleRelicStripWidget::HandleViewModelChanged);
}

void UBattleRelicStripWidget::UnbindFromViewModel()
{
	if (IsValid(BoundViewModel))
	{
		BoundViewModel->OnChanged.RemoveDynamic(
			this,
			&UBattleRelicStripWidget::HandleViewModelChanged);
	}
	BoundViewModel = nullptr;
}

bool UBattleRelicStripWidget::CanReuseCurrentWidgets() const
{
	if (!bNativeBindingsValid || !IsValid(BoundViewModel) || !IsValid(HB_Relics))
	{
		return false;
	}

	const TArray<FBattleHUDRelicView>& Relics = BoundViewModel->Player.Relics;
	if (HB_Relics->GetChildrenCount() != Relics.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Relics.Num(); ++Index)
	{
		const UBattleRelicWidget* Widget =
			Cast<UBattleRelicWidget>(HB_Relics->GetChildAt(Index));
		if (!IsValid(Widget)
			|| Widget->GetRelicId() != Relics[Index].RelicId
			|| Widget->GetRuntimeSequence() != Relics[Index].RuntimeSequence)
		{
			return false;
		}
	}
	return true;
}

UBattleRelicWidget* UBattleRelicStripWidget::CreateRelicWidget() const
{
	if (RelicWidgetClass == nullptr)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UBattleRelicWidget>(OwningPlayer, RelicWidgetClass);
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		return CreateWidget<UBattleRelicWidget>(World, RelicWidgetClass);
	}
	return nullptr;
}

void UBattleRelicStripWidget::RefreshRelics()
{
	if (!bNativeBindingsValid || !IsValid(BoundViewModel) || !IsValid(HB_Relics))
	{
		return;
	}

	const TArray<FBattleHUDRelicView>& Relics = BoundViewModel->Player.Relics;
	if (CanReuseCurrentWidgets())
	{
		for (int32 Index = 0; Index < Relics.Num(); ++Index)
		{
			if (UBattleRelicWidget* Widget =
				Cast<UBattleRelicWidget>(HB_Relics->GetChildAt(Index)))
			{
				Widget->SetRelicView(Relics[Index]);
			}
		}
		return;
	}

	HB_Relics->ClearChildren();
	for (const FBattleHUDRelicView& RelicView : Relics)
	{
		UBattleRelicWidget* Widget = CreateRelicWidget();
		if (!IsValid(Widget))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleRelicStrip][Native] Failed to create Relic Widget for RelicId=%s."),
				*RelicView.RelicId.ToString());
			HB_Relics->ClearChildren();
			return;
		}

		Widget->SetRelicView(RelicView);
		HB_Relics->AddChildToHorizontalBox(Widget);
	}
}
