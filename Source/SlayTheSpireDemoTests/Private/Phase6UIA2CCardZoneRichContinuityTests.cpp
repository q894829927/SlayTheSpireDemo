#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Combat/Combatant.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"
#include "Engine/World.h"

namespace Phase6UIA2CCardZoneRichContinuity
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;

		FFixture()
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
				SpawnParameters);
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

			UCardData* CardA = NewObject<UCardData>(World);
			CardA->CardId = TEXT("RichDrawA");
			CardA->DisplayName = FText::FromString(TEXT("Rich Draw A"));
			CardA->Description = FText::FromString(TEXT("Draw continuity A."));
			CardA->BaseCost = 0;
			CardA->TargetType = ECardTargetType::Enemy;

			UCardData* CardB = NewObject<UCardData>(World);
			CardB->CardId = TEXT("RichDrawB");
			CardB->DisplayName = FText::FromString(TEXT("Rich Draw B"));
			CardB->Description = FText::FromString(TEXT("Draw continuity B."));
			CardB->BaseCost = 0;
			CardB->TargetType = ECardTargetType::Enemy;

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->DebugStartingDeck.Reset();
			Battle->DebugStartingDeck.Add(CardA);
			Battle->DebugStartingDeck.Add(CardB);
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller))
			{
				return;
			}

			Widget->bAcceptAsyncPlayback = true;
			Widget->SetViewModel(ViewModel);
			if (!ViewModel->Initialize(Battle, false)
				|| !Controller->Initialize(Battle, ViewModel, Widget))
			{
				return;
			}
			Widget->SetPresentationController(Controller);
		}

		~FFixture()
		{
			if (IsValid(Controller))
			{
				Controller->Shutdown();
			}
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Battle)
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller)
				&& ViewModel->BattleId > 0
				&& ViewModel->HandCards.Num() == 0
				&& ViewModel->DrawCount == 2;
		}
	};

	FPresentationCardSnapshot MakeCardSnapshot(
		int32 RuntimeId,
		FName CardId,
		const FText& RichDescription)
	{
		FPresentationCardSnapshot Snapshot;
		Snapshot.RuntimeId = RuntimeId;
		Snapshot.CardId = CardId;
		Snapshot.DisplayName = FText::FromName(CardId);
		Snapshot.Cost = 0;
		Snapshot.CardType = ECardType::Attack;
		Snapshot.TargetType = ECardTargetType::Enemy;
		Snapshot.Description = FText::FromString(TEXT("Deal damage."));
		Snapshot.RichDescription = RichDescription;
		return Snapshot;
	}

	FBattleHUDCardView MakeFormalFinalHandView(const FPresentationCardSnapshot& Snapshot)
	{
		FBattleHUDCardView View;
		View.RuntimeId = Snapshot.RuntimeId;
		View.CardId = Snapshot.CardId;
		View.DisplayName = Snapshot.DisplayName;
		View.Cost = Snapshot.Cost;
		View.CardType = Snapshot.CardType;
		View.TargetType = Snapshot.TargetType;
		View.Description = Snapshot.Description;
		View.RichDescription = Snapshot.RichDescription;
		View.CardArt = Snapshot.CardArt;
		View.bGameplayPlayable = true;
		return View;
	}
}

using namespace Phase6UIA2CCardZoneRichContinuity;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CCardZoneWorkingSnapshotRichContinuityTest,
	"SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged.WorkingSnapshotRichContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CCardZoneWorkingSnapshotRichContinuityTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestTrue(TEXT("Controller continuity fixture is ready"), Fixture.IsReady()))
	{
		return false;
	}

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(
		TEXT("Battle exposes a frozen baseline"),
		Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline)))
	{
		return false;
	}

	// These are Strength-style resolved payloads, but the contract assertions below
	// intentionally depend only on non-empty/distinct frozen text and exact record
	// equality rather than a hard-coded localized sentence.
	const FPresentationCardSnapshot CardA = MakeCardSnapshot(
		1001,
		TEXT("RichDrawA"),
		FText::FromString(TEXT("Deal <PreviewIncrease>8</> damage.")));
	const FPresentationCardSnapshot CardB = MakeCardSnapshot(
		1002,
		TEXT("RichDrawB"),
		FText::FromString(TEXT("Deal <PreviewIncrease>11</> damage.")));

	TestFalse(TEXT("Record A RichDescription is non-empty"), CardA.RichDescription.IsEmpty());
	TestFalse(TEXT("Record B RichDescription is non-empty"), CardB.RichDescription.IsEmpty());
	TestFalse(TEXT("The two frozen RichDescriptions are distinct"), CardA.RichDescription.EqualTo(CardB.RichDescription));

	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = Baseline.BattleId;
	Envelope.ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
	Envelope.Origin = EPresentationResolutionOrigin::System;
	Envelope.FinalStateRevision = Baseline.StateRevision;

	FPresentationRecord DrawA;
	DrawA.BattleId = Envelope.BattleId;
	DrawA.ResolutionId = Envelope.ResolutionId;
	DrawA.PresentationSequence = 1;
	DrawA.Type = EBattlePresentationRecordType::CardZoneChanged;
	DrawA.CardZoneChanged.Card = CardA;
	DrawA.CardZoneChanged.FromZone = ECardZone::DrawPile;
	DrawA.CardZoneChanged.ToZone = ECardZone::Hand;
	DrawA.CardZoneChanged.FromIndex = 1;
	DrawA.CardZoneChanged.ToIndex = 0;
	Envelope.Records.Add(DrawA);

	FPresentationRecord DrawB;
	DrawB.BattleId = Envelope.BattleId;
	DrawB.ResolutionId = Envelope.ResolutionId;
	DrawB.PresentationSequence = 2;
	DrawB.Type = EBattlePresentationRecordType::CardZoneChanged;
	DrawB.CardZoneChanged.Card = CardB;
	DrawB.CardZoneChanged.FromZone = ECardZone::DrawPile;
	DrawB.CardZoneChanged.ToZone = ECardZone::Hand;
	DrawB.CardZoneChanged.FromIndex = 0;
	DrawB.CardZoneChanged.ToIndex = 1;
	Envelope.Records.Add(DrawB);

	Envelope.FinalSnapshot = Baseline;
	Envelope.FinalSnapshot.DrawCount = 0;
	Envelope.FinalSnapshot.HandCards.Reset();
	Envelope.FinalSnapshot.HandCards.Add(MakeFormalFinalHandView(CardA));
	Envelope.FinalSnapshot.HandCards.Add(MakeFormalFinalHandView(CardB));

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestTrue(TEXT("First Draw enters async playback"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Only first Draw has been offered initially"), Fixture.Widget->PlayCallCount, 1);

	const FPresentationPlaybackToken FirstToken = Fixture.Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("First token belongs to Draw A"), FirstToken.PresentationSequence, static_cast<int64>(1));
	// Call the Controller directly: this test targets reducer state, not Widget
	// completion deferral. Widget-level CoreTicker forwarding has separate coverage.
	Fixture.Controller->NotifyPresentationFinished(FirstToken);

	TestTrue(TEXT("Second Draw is now the active playback"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Second Draw has been offered"), Fixture.Widget->PlayCallCount, 2);
	const FPresentationPlaybackToken SecondToken = Fixture.Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("Second token belongs to Draw B"), SecondToken.PresentationSequence, static_cast<int64>(2));

	if (!TestEqual(TEXT("Stage B Working Hand contains only completed Draw A"), Fixture.ViewModel->HandCards.Num(), 1))
	{
		return false;
	}
	const FBattleHUDCardView& WorkingA = Fixture.ViewModel->HandCards[0];
	TestEqual(TEXT("Working A keeps exact RuntimeId"), WorkingA.RuntimeId, CardA.RuntimeId);
	TestTrue(TEXT("Working A RichDescription equals Record A"), WorkingA.RichDescription.EqualTo(CardA.RichDescription));
	TestFalse(TEXT("Working A RichDescription did not degrade to empty"), WorkingA.RichDescription.IsEmpty());

	Fixture.Controller->NotifyPresentationFinished(SecondToken);
	TestFalse(TEXT("Envelope completes after second Draw"), Fixture.Controller->IsWaitingForCompletionForTesting());
	if (!TestEqual(TEXT("FinalSnapshot Hand contains both Draws"), Fixture.ViewModel->HandCards.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("Final A retains Record A RichDescription"), Fixture.ViewModel->HandCards[0].RichDescription.EqualTo(CardA.RichDescription));
	TestTrue(TEXT("Final B retains Record B RichDescription"), Fixture.ViewModel->HandCards[1].RichDescription.EqualTo(CardB.RichDescription));
	TestFalse(TEXT("Final B RichDescription remains non-empty"), Fixture.ViewModel->HandCards[1].RichDescription.IsEmpty());
	return true;
}

#endif
