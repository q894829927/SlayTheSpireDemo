#include "SelectionPresentationG0TestTypes.h"

#include "Components/HorizontalBox.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

void USelectionPresentationG0HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void USelectionPresentationG0HUDProbe::ConfigureFormalHandForTesting(
	UBattleHUDViewModel* InViewModel,
	UHorizontalBox* InHand)
{
	HB_Hand = InHand;
	CardWidgetClass = USelectionPresentationG0CardProbe::StaticClass();
	SetViewModel(InViewModel);
}

int32 USelectionPresentationG0HUDProbe::GetFormalHandChildCountForTesting() const
{
	return IsValid(HB_Hand) ? HB_Hand->GetChildrenCount() : INDEX_NONE;
}

UBattleCardWidget* USelectionPresentationG0HUDProbe::FindFormalHandCardForTesting(
	int32 RuntimeId) const
{
	if (!IsValid(HB_Hand))
	{
		return nullptr;
	}
	for (UWidget* Child : HB_Hand->GetAllChildren())
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child);
		if (IsValid(Card) && Card->GetRuntimeId() == RuntimeId)
		{
			return Card;
		}
	}
	return nullptr;
}

UWorld* USelectionPresentationG0HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}
