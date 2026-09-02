#include "BattleEventDispatcher.h"

#include "BattleEvent.h"
#include "BattleTrigger.h"
#include "../Actions/BattleAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "../Relics/RelicContainer.h"
#include "../Relics/RelicData.h"
#include "../Relics/RelicInstance.h"
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
		FTriggerRuntimeSource RuntimeSource;
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

bool UBattleEventDispatcher::BindBattleContext(ABattleManager* InBattle)
{
	if (!IsValid(InBattle))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Event] Dispatcher rejected invalid Battle context."));
		return false;
	}

	if (IsValid(BattleContext.Get()) && BattleContext.Get() != InBattle)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[Event] Dispatcher rejected rebinding from Battle %s to Battle %s."),
			*GetNameSafe(BattleContext.Get()),
			*GetNameSafe(InBattle)
		);
		return false;
	}

	BattleContext = InBattle;
	return true;
}

ABattleManager* UBattleEventDispatcher::GetBattleContext() const
{
	return BattleContext.Get();
}

bool UBattleEventDispatcher::Dispatch(
	const FBattleEvent& Event,
	UBattleActionQueue* Queue,
	const TArray<ACombatant*>& Combatants,
	TArray<FTriggerEligibilityRecord>* OutEligibilityTrace,
	const FPresentationRecordWriter* PresentationRecordWriter
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

	const FPresentationRecordWriter ResolvedPresentationWriter = PresentationRecordWriter
		? *PresentationRecordWriter
		: FPresentationRecordWriter{};

	TArray<FTriggerCandidate> Candidates;
	TSet<UObject*> SeenRuntimeSources;

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
			UStatusInstance* Instance = InstancePtr.Get();
			if (!IsValid(Instance) || SeenRuntimeSources.Contains(Instance))
			{
				continue;
			}
			SeenRuntimeSources.Add(Instance);

			const UStatusData* Definition = Instance->GetDefinition();
			if (!IsValid(Definition))
			{
				continue;
			}

			const FTriggerRuntimeSource Source = FTriggerRuntimeSource::FromStatus(Instance);
			for (int32 LocalIndex = 0; LocalIndex < Definition->Triggers.Num(); ++LocalIndex)
			{
				const UBattleTrigger* Trigger = Definition->Triggers[LocalIndex].Get();
				if (!IsValid(Trigger))
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[Event] Status %s contains invalid Trigger at LocalTriggerIndex=%d."),
						*Instance->GetDebugLabel(),
						LocalIndex
					);
					continue;
				}

				const FTriggerContext Context(
					Source,
					Queue,
					BattleContext.Get(),
					ResolvedPresentationWriter
				);
				if (Trigger->CanReact(Event, Context))
				{
					FTriggerCandidate Candidate;
					Candidate.RuntimeSource = Source;
					Candidate.TriggerDefinition = Trigger;
					Candidate.LocalTriggerIndex = LocalIndex;
					Candidates.Add(Candidate);
				}
			}
		}
	}

	if (IsValid(BattleContext.Get()))
	{
		const URelicContainer* RelicContainer = BattleContext->GetPlayerRelicContainer();
		if (IsValid(RelicContainer))
		{
			for (const TObjectPtr<URelicInstance>& InstancePtr : RelicContainer->GetRelics())
			{
				URelicInstance* Instance = InstancePtr.Get();
				if (!IsValid(Instance) || SeenRuntimeSources.Contains(Instance))
				{
					continue;
				}
				SeenRuntimeSources.Add(Instance);

				const URelicData* Definition = Instance->GetDefinition();
				if (!IsValid(Definition))
				{
					continue;
				}

				const FTriggerRuntimeSource Source = FTriggerRuntimeSource::FromRelic(Instance);
				for (int32 LocalIndex = 0; LocalIndex < Definition->Triggers.Num(); ++LocalIndex)
				{
					const UBattleTrigger* Trigger = Definition->Triggers[LocalIndex].Get();
					if (!IsValid(Trigger))
					{
						UE_LOG(
							LogTemp,
							Error,
							TEXT("[Event] Relic %s contains invalid Trigger at LocalTriggerIndex=%d."),
							*Instance->GetDebugLabel(),
							LocalIndex
						);
						continue;
					}

					const FTriggerContext Context(
						Source,
						Queue,
						BattleContext.Get(),
						ResolvedPresentationWriter
					);
					if (Trigger->CanReact(Event, Context))
					{
						FTriggerCandidate Candidate;
						Candidate.RuntimeSource = Source;
						Candidate.TriggerDefinition = Trigger;
						Candidate.LocalTriggerIndex = LocalIndex;
						Candidates.Add(Candidate);
					}
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

			if (A.RuntimeSource.RuntimeSequence != B.RuntimeSource.RuntimeSequence)
			{
				return A.RuntimeSource.RuntimeSequence < B.RuntimeSource.RuntimeSequence;
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
			Record.SourceKind = Candidate.RuntimeSource.Kind;
			Record.SourceId = Candidate.RuntimeSource.SourceId;
			Record.StatusId = Candidate.RuntimeSource.Kind == ETriggerRuntimeSourceKind::Status
				? Candidate.RuntimeSource.SourceId
				: NAME_None;
			Record.Priority = Candidate.TriggerDefinition->Priority;
			Record.RuntimeSequence = Candidate.RuntimeSource.RuntimeSequence;
			Record.LocalTriggerIndex = Candidate.LocalTriggerIndex;
			OutEligibilityTrace->Add(Record);
		}

		const FTriggerContext Context(
			Candidate.RuntimeSource,
			Queue,
			BattleContext.Get(),
			ResolvedPresentationWriter
		);
		TArray<UBattleAction*> LocalBatch;
		Candidate.TriggerDefinition->BuildReactions(Event, Context, LocalBatch);

		if (ResolvedPresentationWriter.IsAvailable())
		{
			for (UBattleAction* Action : LocalBatch)
			{
				if (IsValid(Action))
				{
					Action->SetPresentationRecordWriter(ResolvedPresentationWriter);
				}
			}
		}

		FString LocalFailureReason;
		if (!ValidateLocalReactionBatch(Queue, LocalBatch, LocalFailureReason))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[Event] Trigger batch discarded for SourceId=%s LocalTriggerIndex=%d: %s"),
				*Candidate.RuntimeSource.SourceId.ToString(),
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
