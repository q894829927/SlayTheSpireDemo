#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR5TestTypes.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"

namespace Phase6UIA2NR5Test
{
	FPresentationPlaybackToken MakeToken(int64 Sequence, int64 Generation = 1)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = 101;
		Token.ResolutionId = 202;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		return Token;
	}

	FPresentationRecord MakeSyntheticRecord()
	{
		FPresentationRecord Record;
		Record.BattleId = 101;
		Record.ResolutionId = 202;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::Damage;
		return Record;
	}

	struct FProbeFixture
	{
		UWorld* World = nullptr;
		UPhase6UIA2NR5HUDProbe* Probe = nullptr;

		FProbeFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (IsValid(World))
			{
				Probe = NewObject<UPhase6UIA2NR5HUDProbe>(World);
				if (IsValid(Probe))
				{
					Probe->SetTestWorld(World);
				}
			}
		}

		~FProbeFixture()
		{
			if (IsValid(Probe))
			{
				Probe->SkipPresentation();
			}
			FTSTicker::GetCoreTicker().Tick(0.0f);
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool IsValidFixture() const
		{
			return IsValid(World) && IsValid(Probe);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePlaybackUnsupportedAndFailedBeginTest,
	"SlayTheSpireDemo.Phase6UIA2N.R5.UnsupportedAndFailedBegin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativePlaybackUnsupportedAndFailedBeginTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR5Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R5 playback probe fixture."));
		return false;
	}

	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken Token = MakeToken(1);

	Fixture.Probe->SetAcceptSyntheticPlayback(false);
	TestFalse(
		TEXT("Production-unmigrated Record returns false"),
		Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestFalse(TEXT("Unsupported Record leaves no local active ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Unsupported Record leaves no local timer"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->SetAcceptSyntheticPlayback(true);
	Fixture.Probe->SetForceTimerFailure(true);
	TestFalse(
		TEXT("Synthetic Begin rolls back when timer preparation fails"),
		Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestFalse(TEXT("Failed Begin leaves no local active ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Failed Begin leaves no local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(
		TEXT("Failed Begin resets local Record type"),
		Fixture.Probe->ActiveLocalType(),
		EBattlePresentationRecordType::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePlaybackExactCancelTest,
	"SlayTheSpireDemo.Phase6UIA2N.R5.ExactCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativePlaybackExactCancelTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR5Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R5 playback probe fixture."));
		return false;
	}

	Fixture.Probe->SetAcceptSyntheticPlayback(true);
	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	const FPresentationPlaybackToken WrongToken = MakeToken(99);

	TestTrue(TEXT("Synthetic valid Begin is accepted"), Fixture.Probe->PlayPresentationRecord(Record, TokenA));
	TestTrue(TEXT("Accepted Begin establishes local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Accepted Begin owns a finish timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestTrue(TEXT("Accepted Begin preserves exact Token"), Fixture.Probe->ActiveLocalToken() == TokenA);

	Fixture.Probe->InvokeCancelForTesting(WrongToken);
	TestTrue(TEXT("Wrong-token Cancel cannot clear active ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Wrong-token Cancel cannot clear active timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestTrue(TEXT("Wrong-token Cancel cannot replace active Token"), Fixture.Probe->ActiveLocalToken() == TokenA);

	// Public replacement proves the base tracked-token wrapper dispatches exact A
	// before the probe accepts B.
	TestTrue(TEXT("Replacement synthetic Begin is accepted"), Fixture.Probe->PlayPresentationRecord(Record, TokenB));
	TestEqual(TEXT("Wrong-token direct Cancel plus exact replacement Cancel were both dispatched"), Fixture.Probe->CancelDispatchCount, 2);
	TestTrue(TEXT("Base replacement Cancel carries exact old Token"), Fixture.Probe->LastCancelDispatchToken == TokenA);
	TestTrue(TEXT("Replacement establishes the new exact Token"), Fixture.Probe->ActiveLocalToken() == TokenB);
	TestTrue(TEXT("Replacement owns exactly one local timer"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->SkipPresentation();
	TestFalse(TEXT("Explicit Skip clears exact local ownership through Cancel"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Explicit Skip clears local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Explicit Skip adds one exact Cancel dispatch"), Fixture.Probe->CancelDispatchCount, 3);
	TestTrue(TEXT("Skip Cancel carries Token B"), Fixture.Probe->LastCancelDispatchToken == TokenB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePlaybackFinishIsolationTest,
	"SlayTheSpireDemo.Phase6UIA2N.R5.FinishIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativePlaybackFinishIsolationTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR5Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R5 playback probe fixture."));
		return false;
	}

	Fixture.Probe->SetAcceptSyntheticPlayback(true);
	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	const FPresentationPlaybackToken TokenC = MakeToken(3);

	TestTrue(TEXT("Token A Begin is accepted"), Fixture.Probe->PlayPresentationRecord(Record, TokenA));
	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Exact Finish clears local active ownership before Notify"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Finish clears local timer before Notify"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Duplicate Finish remains a no-op"), Fixture.Probe->IsLocalPresentationActive());

	// Notify is deferred by the base. Before that callback runs, establish B.
	TestTrue(TEXT("Token B can start after local A Finish"), Fixture.Probe->PlayPresentationRecord(Record, TokenB));
	TestTrue(TEXT("Token B becomes active"), Fixture.Probe->ActiveLocalToken() == TokenB);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestTrue(TEXT("Deferred old Token A callback cannot clear local Token B"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Deferred old Token A callback cannot replace Token B"), Fixture.Probe->ActiveLocalToken() == TokenB);

	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestTrue(TEXT("Stale Finish cannot clear current Token B"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Stale Finish leaves Token B exact"), Fixture.Probe->ActiveLocalToken() == TokenB);

	// If the old deferred callback had erased the base tracked B token, starting C
	// would not dispatch the exact B cancellation. This checks old/new isolation
	// through the public PlayPresentationRecord ownership wrapper.
	TestTrue(TEXT("Token C replacement is accepted"), Fixture.Probe->PlayPresentationRecord(Record, TokenC));
	TestEqual(TEXT("A replacement and B replacement each dispatch one tracked Cancel"), Fixture.Probe->CancelDispatchCount, 2);
	TestTrue(TEXT("Token C replacement cancelled exact Token B"), Fixture.Probe->LastCancelDispatchToken == TokenB);
	TestTrue(TEXT("Token C is the only local owner"), Fixture.Probe->ActiveLocalToken() == TokenC);

	Fixture.Probe->InvokeFinishForTesting(TokenC);
	Fixture.Probe->InvokeFinishForTesting(TokenC);
	TestFalse(TEXT("Exact then duplicate Token C Finish leaves no local owner"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact then duplicate Token C Finish leaves no timer"), Fixture.Probe->IsLocalFinishTimerSet());
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePlaybackDestructCleanupTest,
	"SlayTheSpireDemo.Phase6UIA2N.R5.DestructCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativePlaybackDestructCleanupTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR5Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R5 playback probe fixture."));
		return false;
	}

	Fixture.Probe->SetAcceptSyntheticPlayback(true);
	const FPresentationRecord Record = MakeSyntheticRecord();
	const FPresentationPlaybackToken Token = MakeToken(1);
	TestTrue(TEXT("Destruct fixture begins one synthetic presentation"), Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("Destruct fixture owns local state before teardown"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Destruct fixture owns timer before teardown"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("NativeDestruct clears local active ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("NativeDestruct clears local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("NativeDestruct does not dispatch visual Cancel"), Fixture.Probe->CancelDispatchCount, 0);
	return true;
}

#endif
