#include "Phase6UIA2D5TestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5Test
{
	namespace
	{
		bool IsTerminalRecordType(EBattlePresentationRecordType Type)
		{
			return Type == EBattlePresentationRecordType::Victory
				|| Type == EBattlePresentationRecordType::Defeat
				|| Type == EBattlePresentationRecordType::ResolutionFault;
		}

		bool CompareStatusArrays(
			FAutomationTestBase& Test,
			const TArray<FBattleHUDStatusView>& Actual,
			const TArray<FBattleHUDStatusView>& Expected,
			const FString& Context
		)
		{
			bool bOk = Test.TestEqual(
				*FString::Printf(TEXT("%s status count"), *Context),
				Actual.Num(),
				Expected.Num()
			);

			const int32 CommonCount = FMath::Min(Actual.Num(), Expected.Num());
			for (int32 Index = 0; Index < CommonCount; ++Index)
			{
				const FBattleHUDStatusView& A = Actual[Index];
				const FBattleHUDStatusView& E = Expected[Index];
				const FString Item = FString::Printf(TEXT("%s status[%d]"), *Context, Index);

				bOk &= Test.TestEqual(*FString::Printf(TEXT("%s StatusId"), *Item), A.StatusId, E.StatusId);
				bOk &= Test.TestEqual(*FString::Printf(TEXT("%s RuntimeSequence"), *Item), A.RuntimeSequence, E.RuntimeSequence);
				bOk &= Test.TestEqual(*FString::Printf(TEXT("%s Amount"), *Item), A.Amount, E.Amount);
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s DisplayName"), *Item), A.DisplayName.EqualTo(E.DisplayName));
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s Description"), *Item), A.Description.EqualTo(E.Description));
				bOk &= Test.TestEqual(*FString::Printf(TEXT("%s bUseAtlasIcon"), *Item), A.bUseAtlasIcon, E.bUseAtlasIcon);
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s UVOffset"), *Item), A.UVOffset == E.UVOffset);
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s UVScale"), *Item), A.UVScale == E.UVScale);
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s TrimOffset"), *Item), A.TrimOffset == E.TrimOffset);
				bOk &= Test.TestTrue(*FString::Printf(TEXT("%s TrimScale"), *Item), A.TrimScale == E.TrimScale);

				if (Index > 0)
				{
					bOk &= Test.TestTrue(
						*FString::Printf(TEXT("%s RuntimeSequence order"), *Item),
						Actual[Index - 1].RuntimeSequence < A.RuntimeSequence
					);
					bOk &= Test.TestTrue(
						*FString::Printf(TEXT("%s FinalSnapshot RuntimeSequence order"), *Item),
						Expected[Index - 1].RuntimeSequence < E.RuntimeSequence
					);
				}
			}
			return bOk;
		}

		bool CompareCombatantReducerOwnedState(
			FAutomationTestBase& Test,
			const FBattleHUDCombatantView& Actual,
			const FBattleHUDCombatantView& Expected,
			const FString& Context
		)
		{
			bool bOk = true;
			bOk &= Test.TestEqual(*FString::Printf(TEXT("%s HP"), *Context), Actual.HP, Expected.HP);
			bOk &= Test.TestEqual(*FString::Printf(TEXT("%s Block"), *Context), Actual.Block, Expected.Block);
			bOk &= Test.TestEqual(*FString::Printf(TEXT("%s bDead"), *Context), Actual.bDead, Expected.bDead);
			bOk &= CompareStatusArrays(Test, Actual.Statuses, Expected.Statuses, Context);
			return bOk;
		}

		bool CompareHandRuntimeOrder(
			FAutomationTestBase& Test,
			const TArray<FBattleHUDCardView>& Actual,
			const TArray<FBattleHUDCardView>& Expected,
			const FString& Context
		)
		{
			bool bOk = Test.TestEqual(
				*FString::Printf(TEXT("%s hand count"), *Context),
				Actual.Num(),
				Expected.Num()
			);
			const int32 CommonCount = FMath::Min(Actual.Num(), Expected.Num());
			for (int32 Index = 0; Index < CommonCount; ++Index)
			{
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s hand[%d] RuntimeId"), *Context, Index),
					Actual[Index].RuntimeId,
					Expected[Index].RuntimeId
				);
			}
			return bOk;
		}
	}

	FAcceptanceFixture::FAcceptanceFixture()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		if (!IsValid(World))
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
		Enemy = World->SpawnActor<ACombatant>(
			ACombatant::StaticClass(),
			FTransform(FVector(100.0, 0.0, 0.0)),
			SpawnParameters
		);
		Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
		{
			return;
		}

		Player->MaxHP = 100;
		Enemy->MaxHP = 100;
		Player->PresentationId = TEXT("PlayerHero");
		Enemy->PresentationId = TEXT("EnemyPrimary");
		Player->DisplayName = FText::FromString(TEXT("Player"));
		Enemy->DisplayName = FText::FromString(TEXT("Enemy"));

		Battle->Player = Player;
		Battle->Enemy = Enemy;
		Battle->OpeningHandDrawCount = 0;
		Battle->PlayerTurnDrawCount = 0;
		Battle->EnemyTestAttackDamage = 0;
		Battle->bEnableCommittedPresentationRecording = true;

		EnvelopeCaptureHandle = Battle->OnPresentationResolutionReady.AddLambda(
			[this](const FPresentationResolutionEnvelope& Envelope)
			{
				if (!bCaptureAcceptanceEnvelopes)
				{
					return;
				}

				FCapturedEnvelope Capture;
				Capture.Baseline = NextEnvelopeBaseline;
				Capture.Envelope = Envelope;
				CapturedEnvelopes.Add(MoveTemp(Capture));
				NextEnvelopeBaseline = Envelope.FinalSnapshot;
			}
		);
	}

	FAcceptanceFixture::~FAcceptanceFixture()
	{
		bCaptureAcceptanceEnvelopes = false;
		if (IsValid(Controller))
		{
			Controller->Shutdown();
		}
		if (IsValid(ViewModel))
		{
			ViewModel->Shutdown();
		}
		if (IsValid(Battle) && EnvelopeCaptureHandle.IsValid())
		{
			Battle->OnPresentationResolutionReady.Remove(EnvelopeCaptureHandle);
		}
		if (IsValid(World))
		{
			World->DestroyWorld(false);
		}
	}

	bool FAcceptanceFixture::Start(
		const TArray<UCardData*>& Definitions,
		int32 OpeningDrawCount,
		int32 PlayerDrawCount,
		int32 EnemyDamage,
		bool bEnablePresentation
	)
	{
		if (!IsValid(Battle) || !IsValid(World))
		{
			return false;
		}

		bCaptureAcceptanceEnvelopes = false;
		CapturedEnvelopes.Reset();
		Battle->DebugStartingDeck.Reset();
		for (UCardData* Definition : Definitions)
		{
			Battle->DebugStartingDeck.Add(Definition);
		}
		Battle->OpeningHandDrawCount = OpeningDrawCount;
		Battle->PlayerTurnDrawCount = PlayerDrawCount;
		Battle->EnemyTestAttackDamage = EnemyDamage;
		Battle->bEnableCommittedPresentationRecording = bEnablePresentation;
		Battle->StartBattle();
		Flush();

		if (!IsValid(Battle->GetActionQueueForTesting())
			|| Battle->BattleState != EBattleState::PlayerTurn
			|| !Battle->TryGetLatestFrozenPresentationBaseline(NextEnvelopeBaseline))
		{
			return false;
		}

		ViewModel = NewObject<UBattleHUDViewModel>(World);
		Widget = NewObject<UPhase6UIA2D5PlaybackWidget>(World);
		Controller = NewObject<UBattlePresentationController>(World);
		if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller))
		{
			return false;
		}

		Widget->bAcceptAsyncPlayback = true;
		if (!ViewModel->Initialize(Battle, false)
			|| !Controller->Initialize(Battle, ViewModel, Widget))
		{
			return false;
		}

		Widget->ResetCapture();
		bCaptureAcceptanceEnvelopes = true;
		return IsReady();
	}

	bool FAcceptanceFixture::IsReady() const
	{
		return IsValid(World)
			&& IsValid(Player)
			&& IsValid(Enemy)
			&& IsValid(Battle)
			&& IsValid(Battle->GetActionQueueForTesting())
			&& IsValid(Battle->GetDeckRuntimeForTesting())
			&& IsValid(ViewModel)
			&& IsValid(Widget)
			&& IsValid(Controller);
	}

	void FAcceptanceFixture::Flush() const
	{
		if (IsValid(Battle))
		{
			Battle->FlushScheduledReadStateReadyForTesting();
		}
	}

	void FAcceptanceFixture::ResetAcceptanceCapture()
	{
		CapturedEnvelopes.Reset();
		if (IsValid(Widget))
		{
			Widget->ResetCapture();
		}

		FPresentationStateSnapshot LatestBaseline;
		if (IsValid(Battle) && Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline))
		{
			NextEnvelopeBaseline = LatestBaseline;
		}
	}

	bool FAcceptanceFixture::CompleteCurrentPlayback()
	{
		if (!IsValid(Controller) || !Controller->IsWaitingForCompletionForTesting())
		{
			return false;
		}
		const FPresentationPlaybackToken Token = Controller->GetActivePlaybackTokenForTesting();
		Controller->NotifyPresentationFinished(Token);
		return true;
	}

	bool FAcceptanceFixture::DrainPlayback(int32 MaxCompletions)
	{
		if (!IsValid(Controller) || MaxCompletions <= 0)
		{
			return false;
		}

		for (int32 Index = 0; Index < MaxCompletions; ++Index)
		{
			if (Controller->IsWaitingForCompletionForTesting())
			{
				CompleteCurrentPlayback();
				continue;
			}
			if (Controller->GetBacklogCountForTesting() == 0)
			{
				return true;
			}
			return false;
		}
		return !Controller->IsWaitingForCompletionForTesting()
			&& Controller->GetBacklogCountForTesting() == 0;
	}

	const FCapturedEnvelope* FAcceptanceFixture::LastCapturedEnvelope() const
	{
		return CapturedEnvelopes.Num() > 0 ? &CapturedEnvelopes.Last() : nullptr;
	}

	const FCapturedEnvelope* FAcceptanceFixture::FindCapturedEnvelope(int64 ResolutionId) const
	{
		return CapturedEnvelopes.FindByPredicate(
			[ResolutionId](const FCapturedEnvelope& Capture)
			{
				return Capture.Envelope.ResolutionId == ResolutionId;
			}
		);
	}

	bool ReplayEnvelopeWithProductionReducers(
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		FPresentationStateSnapshot& OutReducedSnapshot
	)
	{
		UBattlePresentationController* ReducerHarness = NewObject<UBattlePresentationController>(GetTransientPackage());
		return IsValid(ReducerHarness)
			&& ReducerHarness->ReduceEnvelopeForTesting(Baseline, Envelope, OutReducedSnapshot);
	}

	bool AssertReducerOwnedStateMatchesFinalSnapshot(
		FAutomationTestBase& Test,
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		const FString& Context
	)
	{
		bool bOk = true;
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s baseline BattleId valid"), *Context), Baseline.BattleId > 0);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s envelope BattleId"), *Context), Envelope.BattleId, Baseline.BattleId);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s final BattleId"), *Context), Envelope.FinalSnapshot.BattleId, Envelope.BattleId);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s final revision contract"), *Context),
			Envelope.FinalStateRevision,
			Envelope.FinalSnapshot.StateRevision
		);

		FPresentationStateSnapshot Reduced;
		if (!ReplayEnvelopeWithProductionReducers(Baseline, Envelope, Reduced))
		{
			Test.AddError(FString::Printf(TEXT("%s production reducers rejected the captured Envelope."), *Context));
			return false;
		}

		const bool bHasTerminalRecord = Envelope.Records.ContainsByPredicate(
			[](const FPresentationRecord& Record)
			{
				return IsTerminalRecordType(Record.Type);
			}
		);
		const FPresentationStateSnapshot& Final = Envelope.FinalSnapshot;
		bOk &= CompareCombatantReducerOwnedState(Test, Reduced.Player, Final.Player, Context + TEXT(" Player"));
		bOk &= CompareCombatantReducerOwnedState(Test, Reduced.Enemy, Final.Enemy, Context + TEXT(" Enemy"));
		if (!bHasTerminalRecord)
		{
			bOk &= Test.TestEqual(*FString::Printf(TEXT("%s Energy"), *Context), Reduced.Energy, Final.Energy);
		}
		bOk &= CompareHandRuntimeOrder(Test, Reduced.HandCards, Final.HandCards, Context);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s DrawCount"), *Context), Reduced.DrawCount, Final.DrawCount);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s DiscardCount"), *Context), Reduced.DiscardCount, Final.DiscardCount);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s ExhaustCount"), *Context), Reduced.ExhaustCount, Final.ExhaustCount);

		if (bHasTerminalRecord)
		{
			bOk &= Test.TestEqual(
				*FString::Printf(TEXT("%s terminal BattleState"), *Context),
				static_cast<int32>(Reduced.BattleState),
				static_cast<int32>(Final.BattleState)
			);
			bOk &= Test.TestEqual(
				*FString::Printf(TEXT("%s terminal Outcome"), *Context),
				static_cast<int32>(Reduced.Outcome),
				static_cast<int32>(Final.Outcome)
			);
			bOk &= Test.TestEqual(
				*FString::Printf(TEXT("%s terminal bCanEndTurn"), *Context),
				Reduced.bCanEndTurn,
				Final.bCanEndTurn
			);
		}

		return bOk;
	}

	bool AssertCapturedEnvelopeOrder(
		FAutomationTestBase& Test,
		const TArray<FCapturedEnvelope>& Captures,
		const FString& Context
	)
	{
		if (Captures.Num() == 0)
		{
			Test.AddError(FString::Printf(TEXT("%s captured no Envelopes."), *Context));
			return false;
		}

		bool bOk = true;
		int64 PreviousResolutionId = 0;
		for (int32 Index = 0; Index < Captures.Num(); ++Index)
		{
			const FCapturedEnvelope& Capture = Captures[Index];
			const FString Item = FString::Printf(TEXT("%s Envelope[%d]"), *Context, Index);
			bOk &= Test.TestEqual(*FString::Printf(TEXT("%s BattleId"), *Item), Capture.Envelope.BattleId, Capture.Baseline.BattleId);
			bOk &= Test.TestTrue(*FString::Printf(TEXT("%s ResolutionId positive"), *Item), Capture.Envelope.ResolutionId > 0);
			if (Index > 0)
			{
				bOk &= Test.TestTrue(
					*FString::Printf(TEXT("%s ResolutionId strictly increasing"), *Item),
					Capture.Envelope.ResolutionId > PreviousResolutionId
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s baseline revision follows previous FinalSnapshot"), *Item),
					Capture.Baseline.StateRevision,
					Captures[Index - 1].Envelope.FinalSnapshot.StateRevision
				);
			}
			PreviousResolutionId = Capture.Envelope.ResolutionId;
		}
		return bOk;
	}
}

#endif
