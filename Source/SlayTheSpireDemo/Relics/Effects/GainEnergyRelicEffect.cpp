#include "GainEnergyRelicEffect.h"

#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/GainEnergyAction.h"
#include "../../Battle/BattleManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UGainEnergyRelicEffect::BuildActions(
	const FRelicEffectContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	UBattleActionQueue* Queue = Cast<UBattleActionQueue>(Context.ActionOuter);
	if (!IsValid(Queue) || !IsValid(Context.Battle) || Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RelicEffect] GainEnergy build rejected invalid context/configuration."));
		return false;
	}

	UGainEnergyAction* Action = NewObject<UGainEnergyAction>(Queue);
	if (!IsValid(Action))
	{
		return false;
	}

	Action->Initialize(Context.Battle, Amount);
	OutActions.Add(Action);
	return true;
}

#if WITH_EDITOR
EDataValidationResult UGainEnergyRelicEffect::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;

	if (Amount <= 0)
	{
		Context.AddError(FText::FromString(TEXT("GainEnergyRelicEffect Amount must be greater than zero.")));
		bValid = false;
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
