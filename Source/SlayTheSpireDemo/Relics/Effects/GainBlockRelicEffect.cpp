#include "GainBlockRelicEffect.h"

#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/GainBlockAction.h"
#include "../../Combat/Combatant.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UGainBlockRelicEffect::BuildActions(
	const FRelicEffectContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	UBattleActionQueue* Queue = Cast<UBattleActionQueue>(Context.ActionOuter);
	if (!IsValid(Queue)
		|| !IsValid(Context.Owner)
		|| Context.OwnerPresentationId.IsNone()
		|| Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RelicEffect] GainBlock build rejected invalid context/configuration."));
		return false;
	}

	UGainBlockAction* Action = NewObject<UGainBlockAction>(Queue);
	if (!IsValid(Action))
	{
		return false;
	}

	Action->Initialize(Context.Owner, Context.Owner, Amount);
	Action->SetPresentationParticipantIds(
		Context.OwnerPresentationId,
		Context.OwnerPresentationId
	);
	OutActions.Add(Action);
	return true;
}

#if WITH_EDITOR
EDataValidationResult UGainBlockRelicEffect::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;

	if (Amount <= 0)
	{
		Context.AddError(FText::FromString(TEXT("GainBlockRelicEffect Amount must be greater than zero.")));
		bValid = false;
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
