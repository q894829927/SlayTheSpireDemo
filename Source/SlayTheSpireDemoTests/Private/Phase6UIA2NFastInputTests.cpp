#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR5TestTypes.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NFastInputTest
{
	FPresentationPlaybackToken MakeToken()
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = 501;
		Token.ResolutionId = 601;
		Token.PresentationSequence = 1;
		Token.LocalPlaybackGeneration = 1;
		return Token;
	}

	FPresentationRecord MakeSyntheticRecord()
	{
		FPresentationRecord Record;
		Record.BattleId = 501;
		Record.ResolutionId = 601;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::Damage;
		return Record;
	}

	FPresentationResolutionEnvelope MakeControllerDamageEnvelope(
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeFastCardPresentationCatchUpTest,
	"SlayTheSpireDemo.Phase6UIA2N.FastInput.DeferredRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeFastCardPresentationCatchUpTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	using namespace Phase6UIA2NFastInputTest;

	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();
	if (!TestNotNull(TEXT("Battle fixture World created"), Fixture.World)
		|| !TestNotNull(TEXT("Battle fixture Battle created"), Fixture.Battle))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UPhase6UIA2NR5HUDProbe* Probe = NewObject<UPhase6UIA2NR5HUDProbe>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	if (!TestNotNull(TEXT("Fast-input HUD probe created"), Probe)
		|| !TestNotNull(TEXT("Fast-input ViewModel created"), ViewModel)
		|| !TestNotNull(TEXT("Fast-input Controller created"), Controller))
	{
		return false;
	}

	Probe->SetTestWorld(Fixture.World);
	Probe->SetViewModel(ViewModel);
	Probe->SetAcceptSyntheticPlayback(true);
	if (!TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))
		|| !TestTrue(TEXT("Presentation Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Probe)))
	{
		return false;
	}
	Probe->SetPresentationController(Controller);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline)))
	{
		return false;
	}
	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 300;
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeControllerDamageEnvelope(Baseline, ResolutionId));
	if (!TestTrue(TEXT("Controller-owned Damage playback is active"), Probe->IsLocalPresentationActive()))
	{
		return false;
	}
	TestTrue(TEXT("Controller reports an authoritative skippable delay"), Controller->HasSkippablePresentationDelay());

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Rapid card click is accepted as deferred UI intent"), Probe->SelectCard(RuntimeId));
	TestFalse(TEXT("Rapid click uses formal Skip to clear active visual"), Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Rapid click dispatches exactly one visual Cancel"), Probe->CancelDispatchCount, 1);
	TestFalse(TEXT("Skip retires authoritative chronology"), Controller->HasSkippablePresentationDelay());
	TestTrue(TEXT("Rapid click does not immediately show ResolutionBusy"), ViewModel->LastFeedback.IsEmpty());

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Exact next-tick retry selects the intended card"), ViewModel->SelectedCardRuntimeId, RuntimeId);
	TestEqual(TEXT("Untargeted retry reaches ReadyToConfirm"), ViewModel->InteractionState, EBattleHUDInteractionState::ReadyToConfirm);
	TestTrue(TEXT("Successful exact retry keeps feedback clear"), ViewModel->LastFeedback.IsEmpty());

	Probe->SetPresentationController(nullptr);
	Probe->SetViewModel(nullptr);
	Controller->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeFastCardLocalVisualDoesNotSkipTest,
	"SlayTheSpireDemo.Phase6UIA2N.FastInput.LocalVisualAloneDoesNotSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeFastCardLocalVisualDoesNotSkipTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NFastInputTest;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Local-visual test World created"), World)) return false;

	UPhase6UIA2NR5HUDProbe* Probe = NewObject<UPhase6UIA2NR5HUDProbe>(World);
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(World);
	Probe->SetTestWorld(World);
	Probe->SetViewModel(ViewModel);
	Probe->SetAcceptSyntheticPlayback(true);

	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken Token = MakeToken();
	if (!TestTrue(TEXT("Synthetic local visual starts"), Probe->PlayPresentationRecord(Record, Token)))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestFalse(TEXT("Local visual alone is not accepted as a fast catch-up intent"), Probe->SelectCard(42));
	TestTrue(TEXT("Local visual remains active without Controller chronology"), Probe->IsLocalPresentationActive());
	TestEqual(TEXT("No formal Skip/cancel is dispatched for cosmetic/local-only visual"), Probe->CancelDispatchCount, 0);
	TestEqual(TEXT("Normal ViewModel gate reports busy"), ViewModel->LastFeedback.ToString(), FString(TEXT("Battle resolution is still in progress.")));

	Probe->CancelTrackedPresentationPlayback(Token);
	Probe->SetViewModel(nullptr);
	World->DestroyWorld(false);
	return true;
}

#endif
