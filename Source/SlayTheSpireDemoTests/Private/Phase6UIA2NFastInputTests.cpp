#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR5TestTypes.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeFastCardPresentationCatchUpTest,
	"SlayTheSpireDemo.Phase6UIA2N.FastInput.DeferredRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeFastCardPresentationCatchUpTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NFastInputTest;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Fast-input test World created"), World))
	{
		return false;
	}

	UPhase6UIA2NR5HUDProbe* Probe = NewObject<UPhase6UIA2NR5HUDProbe>(World);
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(World);
	if (!TestNotNull(TEXT("Fast-input HUD probe created"), Probe)
		|| !TestNotNull(TEXT("Fast-input ViewModel created"), ViewModel))
	{
		World->DestroyWorld(false);
		return false;
	}

	Probe->SetTestWorld(World);
	Probe->SetViewModel(ViewModel);
	Probe->SetAcceptSyntheticPlayback(true);

	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken Token = MakeToken();
	if (!TestTrue(TEXT("Synthetic active presentation starts"), Probe->PlayPresentationRecord(Record, Token)))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Synthetic presentation is active before rapid click"), Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Resolving ViewModel begins with no busy feedback"), ViewModel->LastFeedback.IsEmpty());

	TestTrue(TEXT("Rapid card click is accepted as deferred UI intent"), Probe->SelectCard(42));
	TestFalse(TEXT("Rapid click uses formal Skip to clear the active visual"), Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Rapid click dispatches exactly one visual Cancel"), Probe->CancelDispatchCount, 1);
	TestTrue(TEXT("Rapid click does not immediately show ResolutionBusy"), ViewModel->LastFeedback.IsEmpty());

	// A second click before the deferred retry must only replace pending UI intent;
	// it must not Skip/cancel again or issue a Gameplay request immediately.
	TestTrue(TEXT("Second rapid click replaces pending UI intent"), Probe->SelectCard(43));
	TestEqual(TEXT("Latest-click replacement does not issue another Cancel"), Probe->CancelDispatchCount, 1);
	TestTrue(TEXT("Latest-click replacement still shows no immediate busy feedback"), ViewModel->LastFeedback.IsEmpty());

	// This isolated fixture deliberately remains Resolving and has no caught-up live
	// bindings. The next-tick retry must therefore go back through the unchanged
	// ViewModel request gate and receive the normal authoritative busy rejection.
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(
		TEXT("Deferred retry still preserves authoritative ResolutionBusy rejection"),
		ViewModel->LastFeedback.ToString(),
		FString(TEXT("Battle resolution is still in progress.")));

	Probe->SetViewModel(nullptr);
	World->DestroyWorld(false);
	return true;
}

#endif
