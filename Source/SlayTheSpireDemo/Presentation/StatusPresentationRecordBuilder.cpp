#include "StatusPresentationRecordBuilder.h"

#include "BattlePresentationRecorder.h"
#include "PresentationTypes.h"
#include "../Battle/BattleManager.h"
#include "../Battle/BattleTextResolver.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"

namespace
{
	bool ValidateReason(const FStatusMutationResult& Mutation, EStatusChangeReason Reason)
	{
		switch (Reason)
		{
		case EStatusChangeReason::Applied:
			return Mutation.bCreated
				&& !Mutation.bRemoved
				&& Mutation.AmountBefore == 0
				&& Mutation.AmountAfter > 0;

		case EStatusChangeReason::Increased:
			return !Mutation.bCreated
				&& !Mutation.bRemoved
				&& Mutation.AmountAfter > Mutation.AmountBefore;

		case EStatusChangeReason::Reduced:
		case EStatusChangeReason::TurnEndDecay:
			return !Mutation.bCreated
				&& Mutation.AmountBefore > Mutation.AmountAfter
				&& Mutation.AmountAfter >= 0
				&& Mutation.bRemoved == (Mutation.AmountAfter == 0);

		case EStatusChangeReason::Removed:
			return !Mutation.bCreated
				&& Mutation.bRemoved
				&& Mutation.AmountBefore > 0
				&& Mutation.AmountAfter == 0;

		default:
			return false;
		}
	}

	bool ResolveParticipantIds(
		ABattleManager* Battle,
		ACombatant* Source,
		ACombatant* Target,
		FName& OutSourceId,
		FName& OutTargetId
	)
	{
		OutSourceId = NAME_None;
		OutTargetId = NAME_None;

		if (!IsValid(Battle) || !IsValid(Target))
		{
			return false;
		}

		// A genuinely absent Source is the only case that may freeze NAME_None.
		// A supplied-but-invalid UObject must not be silently reclassified as a
		// system/anonymous source after Gameplay has committed.
		if (Source != nullptr)
		{
			if (!IsValid(Source)
				|| !Battle->TryResolveCombatantPresentationId(Source, OutSourceId)
				|| OutSourceId.IsNone())
			{
				return false;
			}
		}

		return Battle->TryResolveCombatantPresentationId(Target, OutTargetId)
			&& !OutTargetId.IsNone();
	}

	bool ValidateMutationIdentity(
		ACombatant* Target,
		const UStatusInstance* ExpectedPreMutationInstance,
		const FStatusMutationResult& Mutation
	)
	{
		if (!Mutation.IsCommitted()
			|| Mutation.StatusId.IsNone()
			|| Mutation.RuntimeSequence == 0
			|| Mutation.RuntimeSequence > static_cast<uint64>(MAX_int64)
			|| !IsValid(Mutation.EffectiveInstance)
			|| !IsValid(Mutation.EffectiveDefinition)
			|| (Mutation.bCreated && Mutation.bRemoved))
		{
			return false;
		}

		if (Mutation.EffectiveInstance->GetOwner() != Target
			|| Mutation.EffectiveInstance->GetStatusId() != Mutation.StatusId
			|| Mutation.EffectiveInstance->GetRuntimeSequence() != Mutation.RuntimeSequence
			|| Mutation.EffectiveInstance->GetDefinition() != Mutation.EffectiveDefinition
			|| Mutation.EffectiveDefinition->StatusId != Mutation.StatusId)
		{
			return false;
		}

		if (Mutation.bCreated)
		{
			return ExpectedPreMutationInstance == nullptr
				&& Mutation.AmountBefore == 0
				&& Mutation.AmountAfter > 0
				&& Mutation.EffectiveInstance->GetAmount() == Mutation.AmountAfter;
		}

		if (!IsValid(ExpectedPreMutationInstance)
			|| Mutation.EffectiveInstance != ExpectedPreMutationInstance
			|| ExpectedPreMutationInstance->GetOwner() != Target
			|| ExpectedPreMutationInstance->GetStatusId() != Mutation.StatusId
			|| ExpectedPreMutationInstance->GetRuntimeSequence() != Mutation.RuntimeSequence
			|| ExpectedPreMutationInstance->GetDefinition() != Mutation.EffectiveDefinition)
		{
			return false;
		}

		if (Mutation.bRemoved)
		{
			return Mutation.AmountBefore > 0 && Mutation.AmountAfter == 0;
		}

		return Mutation.AmountBefore > 0
			&& Mutation.AmountAfter > 0
			&& Mutation.AmountBefore != Mutation.AmountAfter
			&& Mutation.EffectiveInstance->GetAmount() == Mutation.AmountAfter;
	}
}

FText StatusPresentation::FreezeDescription(const UStatusInstance* Instance)
{
	return IsValid(Instance)
		? FBattleTextResolver::ResolveStatusDescription(Instance)
		: FText::GetEmpty();
}

bool StatusPresentation::AppendCommittedChange(
	const FPresentationRecordWriter& Writer,
	ABattleManager* Battle,
	ACombatant* Source,
	ACombatant* Target,
	const UStatusInstance* ExpectedPreMutationInstance,
	const FStatusMutationResult& Mutation,
	EStatusChangeReason Reason,
	const FText& DescriptionBefore,
	const FText& DescriptionAfter
)
{
	if (!Writer.IsAvailable())
	{
		return true;
	}

	FName SourcePresentationId = NAME_None;
	FName TargetPresentationId = NAME_None;
	if (!ResolveParticipantIds(Battle, Source, Target, SourcePresentationId, TargetPresentationId)
		|| !ValidateMutationIdentity(Target, ExpectedPreMutationInstance, Mutation)
		|| !ValidateReason(Mutation, Reason))
	{
		Writer.InvalidateCurrentResolution();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] Status commit could not build a trustworthy identity/reason payload."));
		return false;
	}

	if ((Mutation.bCreated && !DescriptionBefore.IsEmpty())
		|| (Mutation.bRemoved && !DescriptionAfter.IsEmpty()))
	{
		Writer.InvalidateCurrentResolution();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] Status commit violated frozen before/after description boundary semantics."));
		return false;
	}

	const UStatusData* Definition = Mutation.EffectiveDefinition;
	FPresentationRecord Record;
	Record.Type = EBattlePresentationRecordType::StatusChanged;
	Record.StatusChanged.SourcePresentationId = SourcePresentationId;
	Record.StatusChanged.TargetPresentationId = TargetPresentationId;
	Record.StatusChanged.StatusId = Mutation.StatusId;
	Record.StatusChanged.RuntimeSequence = static_cast<int64>(Mutation.RuntimeSequence);
	Record.StatusChanged.AmountBefore = Mutation.AmountBefore;
	Record.StatusChanged.AmountAfter = Mutation.AmountAfter;
	Record.StatusChanged.bCreated = Mutation.bCreated;
	Record.StatusChanged.bRemoved = Mutation.bRemoved;
	Record.StatusChanged.Reason = Reason;
	Record.StatusChanged.DisplayName = Definition->DisplayName.IsEmpty()
		? FText::FromName(Mutation.StatusId)
		: Definition->DisplayName;
	Record.StatusChanged.DescriptionBefore = DescriptionBefore;
	Record.StatusChanged.DescriptionAfter = DescriptionAfter;
	Record.StatusChanged.bUseAtlasIcon = Definition->IconRegion.bUseAtlasIcon;
	Record.StatusChanged.UVOffset = Definition->IconRegion.UVOffset;
	Record.StatusChanged.UVScale = Definition->IconRegion.UVScale;
	Record.StatusChanged.TrimOffset = Definition->IconRegion.TrimOffset;
	Record.StatusChanged.TrimScale = Definition->IconRegion.TrimScale;

	if (!Writer.Append(MoveTemp(Record)))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] StatusChanged record append failed; Gameplay status commit remains authoritative."));
		return false;
	}

	return true;
}
