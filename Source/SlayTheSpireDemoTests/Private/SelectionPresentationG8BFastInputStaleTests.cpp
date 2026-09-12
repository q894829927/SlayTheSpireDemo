#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR5TestTypes.h"
#include "Containers/Ticker.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8BFastInputStaleTest
{
	FPresentationResolutionEnvelope MakeDamageEnvelope(
		const FPresentationStateSnapshot& Baseline,
		int64 ResolutionId)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;

		FPresentationRecord Record;
		Record.BattleId = Baseline.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::Damage;
		Record.Damage.SourcePresentationId = Baseline.Enemy.PresentationId;
		Record.Damage.TargetPresentationId = Baseline.Player.PresentationId;
		Record.Damage.DamageKind = EDamageKind::Attack;
		Record.Damage.IncomingDamage = 1;
		Record.Damage.HPBefore = Baseline.Player.HP;
		Record.Damage.HPAfter = FMath::Max(0, Baseline.Player.HP - 1);
		Record.Damage.BlockBefore = Baseline.Player.Block;
		Record.Damage.BlockAfter = Baseline.Player.Block;
		Record.Damage.BlockedDamage = 0;
		Record.Damage.HPDamage = Record.Damage.HPBefore - Record.Damage.HPAfter;
		Envelope.Records.Add(Record);
		Envelope.FinalSnapshot.Player.HP = Record.Damage.HPAfter;
		Envelope.FinalSnapshot.Player.bDead = Record.Damage.HPAfter <= 0;
		return Envelope;
	}

	bool BeginDeferredClick(
		FAutomationTestBase& Test,
		Phase6UIA1Test::FHUDTestFixture& Fixture,
		UBattleHUDViewModel*& OutViewModel,
		UPhase6UIA2NR5HUDProbe*& OutProbe,
		UBattlePresentationController*& OutController,
		FPresentationStateSnapshot& OutBaseline,
		int32& OutRuntimeId)
	{
		OutViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		OutProbe = NewObject<UPhase6UIA2NR5HUDProbe>(Fixture.World);
		OutController = NewObject<UBattlePresentationController>(Fixture.World);
		if (!Test.TestNotNull(TEXT("ViewModel created"), OutViewModel)
			|| !Test.TestNotNull(TEXT("Probe created"), OutProbe)
			|| !Test.TestNotNull(TEXT("Controller created"), OutController))
		{
			return false;
		}

		if (!Test.TestTrue(TEXT("Presentation-owned ViewModel initializes"), OutViewModel->Initialize(Fixture.Battle, true)))
		{
			return false;
		}
		OutProbe->SetTestWorld(Fixture.World);
		OutProbe->SetViewModel(OutViewModel);
		OutProbe->SetAcceptSyntheticPlayback(true);
		if (!Test.TestTrue(TEXT("Controller initializes"), OutController->Initialize(Fixture.Battle, OutViewModel, OutProbe)))
		{
			return false;
		}
		OutProbe->SetPresentationController(OutController);

		if (!Test.TestTrue(TEXT("Baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(OutBaseline)))
		{
			return false;
		}

		// A real Gameplay mutation locks the Presentation-owned VM before the
		// historical playback catches up. The synthetic envelope below deliberately
		// reuses the current revision, so recreate that pre-catch-up surface before
		// starting playback; doing it afterwards would itself cancel the tracked
		// visual through the sealed Base-widget VM-change contract.
		OutViewModel->ApplyPresentationSnapshot(OutBaseline, true);
		if (!Test.TestEqual(
			TEXT("Synthetic catch-up surface is resolving"),
			OutViewModel->InteractionState,
			EBattleHUDInteractionState::Resolving)
			|| !Test.TestTrue(TEXT("Synthetic catch-up surface is input locked"), OutViewModel->bInputLocked))
		{
			return false;
		}

		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 400;
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeDamageEnvelope(OutBaseline, ResolutionId));
		if (!Test.TestTrue(TEXT("Controller-owned visual is active"), OutProbe->IsLocalPresentationActive()))
		{
			return false;
		}

		OutRuntimeId = OutViewModel->HandCards.Num() > 0
			? OutViewModel->HandCards[0].RuntimeId
			: INDEX_NONE;
		if (!Test.TestTrue(TEXT("RuntimeId exists"), OutRuntimeId != INDEX_NONE)
			|| !Test.TestTrue(TEXT("Physical click becomes deferred intent"), OutProbe->SelectCard(OutRuntimeId)))
		{
			return false;
		}
		Test.TestEqual(TEXT("No card selected before next-tick retry"), OutViewModel->SelectedCardRuntimeId, INDEX_NONE);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BFastInputSessionStaleTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.FastInput.StaleSessionDropsRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BFastInputSessionStaleTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	using namespace SelectionPresentationG8BFastInputStaleTest;
	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();

	UBattleHUDViewModel* ViewModel = nullptr;
	UPhase6UIA2NR5HUDProbe* Probe = nullptr;
	UBattlePresentationController* Controller = nullptr;
	FPresentationStateSnapshot Baseline;
	int32 RuntimeId = INDEX_NONE;
	if (!BeginDeferredClick(*this, Fixture, ViewModel, Probe, Controller, Baseline, RuntimeId)) return false;

	UPhase6UIA2NR5HUDProbe* ReplacementWidget = NewObject<UPhase6UIA2NR5HUDProbe>(Fixture.World);
	ReplacementWidget->SetTestWorld(Fixture.World);
	Controller->SetWidget(ReplacementWidget);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Binding replacement makes old deferred click stale"), ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("No stale retry changes normal surface"), ViewModel->InteractionState, EBattleHUDInteractionState::Idle);

	Probe->SetPresentationController(nullptr);
	Probe->SetViewModel(nullptr);
	Controller->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BFastInputRevisionStaleTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.FastInput.StaleRevisionDropsRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BFastInputRevisionStaleTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	using namespace SelectionPresentationG8BFastInputStaleTest;
	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();

	UBattleHUDViewModel* ViewModel = nullptr;
	UPhase6UIA2NR5HUDProbe* Probe = nullptr;
	UBattlePresentationController* Controller = nullptr;
	FPresentationStateSnapshot Baseline;
	int32 RuntimeId = INDEX_NONE;
	if (!BeginDeferredClick(*this, Fixture, ViewModel, Probe, Controller, Baseline, RuntimeId)) return false;

	FPresentationStateSnapshot NewDisplayedSurface = Baseline;
	NewDisplayedSurface.StateRevision = Baseline.StateRevision + 1;
	ViewModel->ApplyPresentationSnapshot(NewDisplayedSurface, true);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	TestEqual(TEXT("Revision mismatch drops deferred click"), ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Synthetic newer surface remains resolving/locked"), ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);

	Probe->SetPresentationController(nullptr);
	Probe->SetViewModel(nullptr);
	Controller->Shutdown();
	return true;
}

#endif