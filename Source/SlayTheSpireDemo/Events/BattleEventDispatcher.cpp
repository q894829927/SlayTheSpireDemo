#include "BattleEventDispatcher.h"

#include "BattleEvent.h"
#include "BattleTrigger.h"
#include "../Actions/BattleAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
FOnBattleEventDispatchedForTesting UBattleEventDispatcher::OnEventDispatchedForTesting;
#endif

namespace
{
	struct FTriggerCandidate
	{
		UStatusInstance* RuntimeSource = nullptr;
		const UBattleTrigger* TriggerDefinition = nullptr;
		int32 LocalTriggerIndex = INDEX_NONE;
	};

	bool ValidateLocalReactionBatch(
		UBattleActionQueue* Queue,
		const TArray<UBattleAction*>& Actions,
		FString& OutReason
	)
	{
		OutReason.Reset();
		TSet<UBattleAction*> Seen;
		Seen.Reserve(Actions.Num());

		for (UBattleAction* Action : Actions)
		{
			if (!IsValid(Action))
			{
				OutReason = TEXT("Trigger built an invalid Action.");
				return false;
			}

			if (Action->IsFinished())
			{
				OutReason = FString::Printf(TEXT("Trigger built already-finished Action %s."), *GetNameSafe(Action));
				return false;
			}

			if (Action->GetOuter() != Queue)
			{
				OutReason = FString::Printf(TEXT("Trigger Action %s does not use the target Queue as Outer."), *GetNameSafe(Action));
				return false;
			}

			if (Seen.Contains(Action))
			{
				OutReason = FString::Printf(TEXT("Trigger batch repeats Action %s."), *GetNameSafe(Action));
				return false;
			}
			Seen.Add(Action);
		}

		return true;
	}
}

bool UBattleEventDispatcher::Dispatch(
	const FBattleEvent& Event,
	UBattleActionQueue* Queue,
	const TArray<ACombatant*>& Combatants,
	TArray<FTriggerEligibilityRecord>* OutEligibilityTrace
) const
{
	if (OutEligibilityTrace)
	{
		OutEligibilityTrace->Reset();
	}

	if (!IsValid(Queue) || Queue->IsResolutionFaulted())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Event] Dispatch rejected: invalid or faulted ActionQueue."));
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	OnEventDispatchedForTesting.Broadcast(Event);
#endif

	TArray<FTriggerCandidate> Candidates;
	TSet<UStatusInstance*> SeenRuntimeSources;

	for (ACombatant* Combatant : Combatants)
	{
		if (!IsValid(Combatant))
		{
			continue;
		}

		UStatusContainer* Container = Combatant->GetStatusContainer();
		if (!IsValid(Container))
		{
			continue;
		}

		for (const TObjectPtr<UStatusInstance>& InstancePtr : Container->GetStatuses())
		{
			UStatusInstance* RuntimeSource = InstancePtr.Get();
			if (!IsValid(RuntimeSource) || SeenRuntimeSources.Contains(RuntimeSource))
			{
				continue;
			}
			SeenRuntimeSources.Add(RuntimeSource);

			const UStatusData* Definition = RuntimeSource->GetDefinition();
			if (!IsValid(Definition))
			{
				continue;
			}

			for (int32 LocalIndex = 0; LocalIndex < Definition->Triggers.Num(); ++LocalIndex)
			{
				const UBattleTrigger* Trigger = Definition->Triggers[LocalIndex].Get();
				if (!IsValid(Trigger))
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[Event] Status %s contains invalid Trigger at LocalTriggerIndex=%d."),
						*RuntimeSource->GetDebugLabel(),
						LocalIndex
					);
					continue;
				}

				FTriggerContext Context(RuntimeSource, Queue);
				if (Trigger->CanReact(Event, Context))
				{
					FTriggerCandidate Candidate;
					Candidate.RuntimeSource = RuntimeSource;
					Candidate.TriggerDefinition = Trigger;
					Candidate.LocalTriggerIndex = LocalIndex;
					Candidates.Add(Candidate);
				}
			}
		}
	}

	Candidates.Sort(
		[](const FTriggerCandidate& A, const FTriggerCandidate& B)
		{
			if (A.TriggerDefinition->Priority != B.TriggerDefinition->Priority)
			{
				return A.TriggerDefinition->Priority < B.TriggerDefinition->Priority;
			}

			const uint64 ASequence = A.RuntimeSource->GetRuntimeSequence();
			const uint64 BSequence = B.RuntimeSource->GetRuntimeSequence();
			if (ASequence != BSequence)
			{
				return ASequence < BSequence;
			}

			return A.LocalTriggerIndex < B.LocalTriggerIndex;
		}
	);

	TArray<UBattleAction*> FinalReactionBatch;

	for (const FTriggerCandidate& Candidate : Candidates)
	{
		if (OutEligibilityTrace)
		{
			FTriggerEligibilityRecord Record;
			Record.StatusId = Candidate.RuntimeSource->GetStatusId();
			Record.Priority = Candidate.TriggerDefinition->Priority;
			Record.RuntimeSequence = Candidate.RuntimeSource->GetRuntimeSequence();
			Record.LocalTriggerIndex = Candidate.LocalTriggerIndex;
			OutEligibilityTrace->Add(Record);
		}

		FTriggerContext Context(Candidate.RuntimeSource, Queue);
		TArray<UBattleAction*> LocalBatch;
		Candidate.TriggerDefinition->BuildReactions(Event, Context, LocalBatch);

		FString LocalFailureReason;
		if (!ValidateLocalReactionBatch(Queue, LocalBatch, LocalFailureReason))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[Event] Trigger batch discarded for %s LocalTriggerIndex=%d: %s"),
				*Candidate.RuntimeSource->GetDebugLabel(),
				Candidate.LocalTriggerIndex,
				*LocalFailureReason
			);
			continue;
		}

		FinalReactionBatch.Append(LocalBatch);
	}

	if (FinalReactionBatch.Num() == 0)
	{
		return true;
	}

	if (!Queue->AddBatchToFrontPreserveOrder(FinalReactionBatch))
	{
		const FString Reason = FString::Printf(
			TEXT("Dispatcher failed final atomic reaction insertion. Reactions=%d Candidates=%d."),
			FinalReactionBatch.Num(),
			Candidates.Num()
		);
		Queue->RequestResolutionFault(Reason);
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Event] Dispatch built %d reactions from %d eligible triggers."),
		FinalReactionBatch.Num(),
		Candidates.Num()
	);
	return true;
}
