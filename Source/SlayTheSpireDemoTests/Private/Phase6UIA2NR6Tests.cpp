#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR6TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR6Test
{
	constexpr int64 TestBattleId = 601;
	constexpr int64 TestResolutionId = 602;

	FPresentationPlaybackToken MakeToken(int64 Sequence, int64 Generation = 1)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = TestBattleId;
		Token.ResolutionId = TestResolutionId;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		return Token;
	}

	FPresentationRecord MakeRecord(EBattlePresentationRecordType Type, int64 Sequence)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = Type;
		return Record;
	}

	FPresentationRecord MakeEnergyRecord(int64 Sequence, int32 Before = 3, int32 After = 1)
	{
		FPresentationRecord Record = MakeRecord(
			EBattlePresentationRecordType::EnergyChanged,
			Sequence);
		Record.EnergyChanged.EnergyBefore = Before;
		Record.EnergyChanged.EnergyAfter = After;
		Record.EnergyChanged.Delta = After - Before;
		return Record;
	}

	FPresentationRecord MakeBlockRecord(
		int64 Sequence,
		FName TargetPresentationId,
		int32 Before,
		int32 After,
		EBlockPresentationReason Reason)
	{
		FPresentationRecord Record = MakeRecord(
			EBattlePresentationRecordType::BlockChanged,
			Sequence);
		Record.BlockChanged.SourcePresentationId =
			Reason == EBlockPresentationReason::TurnStartClear
				? NAME_None
				: TargetPresentationId;
		Record.BlockChanged.TargetPresentationId = TargetPresentationId;
		Record.BlockChanged.Reason = Reason;
		Record.BlockChanged.BlockBefore = Before;
		Record.BlockChanged.BlockAfter = After;
		Record.BlockChanged.BlockDelta = After - Before;
		return Record;
	}

	FPresentationRecord MakeShuffleRecord(int64 Sequence, int32 MovedCardCount = 6)
	{
		FPresentationRecord Record = MakeRecord(
			EBattlePresentationRecordType::DeckShuffled,
			Sequence);
		Record.DeckShuffled.MovedCardCount = MovedCardCount;
		Record.DeckShuffled.DrawCountBefore = 0;
		Record.DeckShuffled.DrawCountAfter = MovedCardCount;
		Record.DeckShuffled.DiscardCountBefore = MovedCardCount;
		Record.DeckShuffled.DiscardCountAfter = 0;
		return Record;
	}

	struct FBlockSurface
	{
		USizeBox* Badge = nullptr;
		UOverlay* Overlay = nullptr;
		UTextBlock* Text = nullptr;
	};

	FBlockSurface MakeBlockSurface(UObject* Outer)
	{
		FBlockSurface Surface;
		Surface.Badge = NewObject<USizeBox>(Outer);
		Surface.Overlay = NewObject<UOverlay>(Outer);
		Surface.Text = NewObject<UTextBlock>(Outer);
		Surface.Badge->AddChild(Surface.Overlay);
		Surface.Overlay->AddChild(Surface.Text);
		return Surface;
	}

	struct FProbeFixture
	{
		UWorld* World = nullptr;
		UPhase6UIA2NR6HUDProbe* Probe = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UTextBlock* EnergyText = nullptr;
		FBlockSurface PlayerBlock;
		FBlockSurface EnemyBlock;
		UTextBlock* DrawText = nullptr;
		UTextBlock* DiscardText = nullptr;

		FProbeFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			Probe = NewObject<UPhase6UIA2NR6HUDProbe>(World);
			ViewModel = NewObject<UBattleHUDViewModel>(Probe);
			if (!IsValid(Probe) || !IsValid(ViewModel))
			{
				return;
			}

			Probe->SetTestWorld(World);
			Probe->ViewModel = ViewModel;
			EnergyText = NewObject<UTextBlock>(Probe);
			PlayerBlock = MakeBlockSurface(Probe);
			EnemyBlock = MakeBlockSurface(Probe);
			DrawText = NewObject<UTextBlock>(Probe);
			DiscardText = NewObject<UTextBlock>(Probe);
			Probe->ConfigureR6Surfaces(
				EnergyText,
				PlayerBlock.Text,
				EnemyBlock.Text,
				DrawText,
				DiscardText);

			ViewModel->Energy = 3;
			ViewModel->MaxEnergy = 5;
			ViewModel->Player.PresentationId = TEXT("PlayerPresentation");
			ViewModel->Player.Block = 0;
			ViewModel->Enemy.PresentationId = TEXT("EnemyPresentation");
			ViewModel->Enemy.Block = 4;
			ViewModel->DrawCount = 0;
			ViewModel->DiscardCount = 6;
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
			return IsValid(World)
				&& IsValid(Probe)
				&& IsValid(ViewModel)
				&& IsValid(EnergyText)
				&& IsValid(PlayerBlock.Badge)
				&& IsValid(PlayerBlock.Text)
				&& IsValid(EnemyBlock.Badge)
				&& IsValid(EnemyBlock.Text)
				&& IsValid(DrawText)
				&& IsValid(DiscardText);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR6EnergyPlaybackTest,
	"SlayTheSpireDemo.Phase6UIA2N.R6.Energy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR6EnergyPlaybackTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR6Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R6 Energy fixture."));
		return false;
	}

	const FPresentationRecord RecordA = MakeEnergyRecord(1);
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	const FPresentationPlaybackToken StaleToken = MakeToken(99);
	TestTrue(TEXT("Energy Begin accepts a consistent frozen transition"), Fixture.Probe->PlayPresentationRecord(RecordA, TokenA));
	TestEqual(TEXT("Energy Begin displays frozen After with frozen MaxEnergy"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("1/5")));
	TestTrue(TEXT("Energy Begin owns the exact Token"), Fixture.Probe->ActiveLocalToken() == TokenA);
	TestTrue(TEXT("Energy Begin owns a finish timer"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->InvokeFinishForTesting(StaleToken);
	TestTrue(TEXT("Stale Energy Finish is a no-op"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Stale Energy Finish leaves After visible"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("1/5")));

	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Exact Energy Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Energy Finish clears the local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Energy Finish converges to frozen After"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("1/5")));
	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Duplicate Energy Finish remains a no-op"), Fixture.Probe->IsLocalPresentationActive());
	FTSTicker::GetCoreTicker().Tick(0.0f);

	const FPresentationRecord RecordB = MakeEnergyRecord(2);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	TestTrue(TEXT("Second Energy Begin is accepted"), Fixture.Probe->PlayPresentationRecord(RecordB, TokenB));
	Fixture.Probe->InvokeCancelForTesting(StaleToken);
	TestTrue(TEXT("Wrong-token Energy Cancel cannot clear ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Wrong-token Energy Cancel cannot restore Before"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("1/5")));
	Fixture.Probe->InvokeCancelForTesting(TokenB);
	TestFalse(TEXT("Exact Energy Cancel clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Energy Cancel clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Energy Cancel restores frozen Before"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("3/5")));

	// Direct visual Cancel intentionally leaves the base tracked Token intact.
	// A replacement must therefore dispatch TokenB again; this proves Cancel did
	// not send a normal completion notification.
	const int32 DispatchesAfterDirectCancel = Fixture.Probe->CancelDispatchCount;
	const FPresentationRecord RecordC = MakeEnergyRecord(3);
	const FPresentationPlaybackToken TokenC = MakeToken(3);
	TestTrue(TEXT("Energy playback can restart after Cancel"), Fixture.Probe->PlayPresentationRecord(RecordC, TokenC));
	TestEqual(TEXT("Replacement re-dispatches the uncompleted TokenB"), Fixture.Probe->CancelDispatchCount, DispatchesAfterDirectCancel + 1);
	TestTrue(TEXT("Replacement Cancel carried exact TokenB"), Fixture.Probe->LastCancelDispatchToken == TokenB);
	Fixture.Probe->InvokeFinishForTesting(TokenC);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	const int32 DispatchesAfterNormalFinish = Fixture.Probe->CancelDispatchCount;
	const FPresentationRecord RecordD = MakeEnergyRecord(4);
	const FPresentationPlaybackToken TokenD = MakeToken(4);
	TestTrue(TEXT("Energy playback can restart after Finish"), Fixture.Probe->PlayPresentationRecord(RecordD, TokenD));
	TestEqual(TEXT("Normal Finish cleared base ownership through exact Notify"), Fixture.Probe->CancelDispatchCount, DispatchesAfterNormalFinish);
	Fixture.Probe->InvokeCancelForTesting(TokenD);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR6BlockPlaybackTest,
	"SlayTheSpireDemo.Phase6UIA2N.R6.Block",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR6BlockPlaybackTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR6Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R6 Block fixture."));
		return false;
	}

	Fixture.PlayerBlock.Text->SetText(FText::AsNumber(0));
	Fixture.PlayerBlock.Badge->SetVisibility(ESlateVisibility::Collapsed);
	const FPresentationRecord PlayerGain = MakeBlockRecord(
		1,
		Fixture.ViewModel->Player.PresentationId,
		0,
		7,
		EBlockPresentationReason::Gain);
	const FPresentationPlaybackToken PlayerToken = MakeToken(1);
	TestTrue(TEXT("Player Block Begin resolves the Record TargetPresentationId"), Fixture.Probe->PlayPresentationRecord(PlayerGain, PlayerToken));
	TestEqual(TEXT("Player Block Begin displays frozen After"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("7")));
	TestTrue(TEXT("Positive Player Block shows the complete badge"), Fixture.PlayerBlock.Badge->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	Fixture.Probe->InvokeCancelForTesting(PlayerToken);
	TestEqual(TEXT("Player Block Cancel restores frozen Before"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("0")));
	TestTrue(TEXT("Player Block Cancel collapses the complete zero badge"), Fixture.PlayerBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Player Block Cancel clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Player Block Cancel clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.EnemyBlock.Text->SetText(FText::AsNumber(4));
	Fixture.EnemyBlock.Badge->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FPresentationRecord EnemyClear = MakeBlockRecord(
		2,
		Fixture.ViewModel->Enemy.PresentationId,
		4,
		0,
		EBlockPresentationReason::TurnStartClear);
	const FPresentationPlaybackToken EnemyToken = MakeToken(2);
	TestTrue(TEXT("Enemy Block Begin resolves the Record TargetPresentationId"), Fixture.Probe->PlayPresentationRecord(EnemyClear, EnemyToken));
	TestEqual(TEXT("Enemy Block Begin displays frozen zero After"), Fixture.EnemyBlock.Text->GetText().ToString(), FString(TEXT("0")));
	TestTrue(TEXT("Enemy zero Block collapses the complete badge"), Fixture.EnemyBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	Fixture.Probe->InvokeFinishForTesting(EnemyToken);
	TestEqual(TEXT("Enemy Block Finish retains frozen After"), Fixture.EnemyBlock.Text->GetText().ToString(), FString(TEXT("0")));
	TestTrue(TEXT("Enemy Block Finish retains collapsed zero badge"), Fixture.EnemyBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Enemy Block Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Enemy Block Finish clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());
	FTSTicker::GetCoreTicker().Tick(0.0f);

	const FPresentationRecord EnemyClearCancel = MakeBlockRecord(
		3,
		Fixture.ViewModel->Enemy.PresentationId,
		4,
		0,
		EBlockPresentationReason::TurnStartClear);
	const FPresentationPlaybackToken EnemyCancelToken = MakeToken(3);
	TestTrue(TEXT("Enemy Block can begin again for Cancel coverage"), Fixture.Probe->PlayPresentationRecord(EnemyClearCancel, EnemyCancelToken));
	Fixture.Probe->InvokeCancelForTesting(EnemyCancelToken);
	TestEqual(TEXT("Enemy Block Cancel restores frozen Before"), Fixture.EnemyBlock.Text->GetText().ToString(), FString(TEXT("4")));
	TestTrue(TEXT("Enemy Block Cancel restores the complete positive badge"), Fixture.EnemyBlock.Badge->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR6ShufflePlaybackTest,
	"SlayTheSpireDemo.Phase6UIA2N.R6.Shuffle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR6ShufflePlaybackTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR6Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R6 Shuffle fixture."));
		return false;
	}

	Fixture.DrawText->SetText(FText::AsNumber(0));
	Fixture.DiscardText->SetText(FText::AsNumber(6));
	const FPresentationRecord ShuffleA = MakeShuffleRecord(1);
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	TestTrue(TEXT("Shuffle Begin accepts the frozen pile transition"), Fixture.Probe->PlayPresentationRecord(ShuffleA, TokenA));
	TestEqual(TEXT("Shuffle Begin displays frozen Draw After"), Fixture.DrawText->GetText().ToString(), FString(TEXT("6")));
	TestEqual(TEXT("Shuffle Begin displays frozen Discard After"), Fixture.DiscardText->GetText().ToString(), FString(TEXT("0")));
	Fixture.Probe->InvokeCancelForTesting(TokenA);
	TestEqual(TEXT("Shuffle Cancel restores frozen Draw Before"), Fixture.DrawText->GetText().ToString(), FString(TEXT("0")));
	TestEqual(TEXT("Shuffle Cancel restores frozen Discard Before"), Fixture.DiscardText->GetText().ToString(), FString(TEXT("6")));
	TestFalse(TEXT("Shuffle Cancel clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Shuffle Cancel clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());

	const FPresentationRecord ShuffleB = MakeShuffleRecord(2);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	TestTrue(TEXT("Shuffle can begin again after Cancel"), Fixture.Probe->PlayPresentationRecord(ShuffleB, TokenB));
	Fixture.Probe->InvokeFinishForTesting(TokenB);
	TestEqual(TEXT("Shuffle Finish converges to frozen Draw After"), Fixture.DrawText->GetText().ToString(), FString(TEXT("6")));
	TestEqual(TEXT("Shuffle Finish converges to frozen Discard After"), Fixture.DiscardText->GetText().ToString(), FString(TEXT("0")));
	TestFalse(TEXT("Shuffle Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Shuffle Finish clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR6InvalidBeginTest,
	"SlayTheSpireDemo.Phase6UIA2N.R6.InvalidBegin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR6InvalidBeginTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR6Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R6 invalid-Begin fixture."));
		return false;
	}

	Fixture.EnergyText->SetText(FText::FromString(TEXT("energy-sentinel")));
	Fixture.PlayerBlock.Text->SetText(FText::FromString(TEXT("block-sentinel")));
	Fixture.PlayerBlock.Badge->SetVisibility(ESlateVisibility::Hidden);
	Fixture.DrawText->SetText(FText::FromString(TEXT("draw-sentinel")));
	Fixture.DiscardText->SetText(FText::FromString(TEXT("discard-sentinel")));

	FPresentationRecord InvalidEnergy = MakeEnergyRecord(1);
	InvalidEnergy.EnergyChanged.Delta = 99;
	TestFalse(TEXT("Inconsistent Energy payload returns false"), Fixture.Probe->PlayPresentationRecord(InvalidEnergy, MakeToken(1)));

	FPresentationRecord InvalidBlock = MakeBlockRecord(
		2,
		TEXT("UnknownPresentation"),
		0,
		7,
		EBlockPresentationReason::Gain);
	TestFalse(TEXT("Unknown Block TargetPresentationId returns false"), Fixture.Probe->PlayPresentationRecord(InvalidBlock, MakeToken(2)));

	FPresentationRecord InvalidShuffle = MakeShuffleRecord(3);
	InvalidShuffle.DeckShuffled.DrawCountAfter = 5;
	TestFalse(TEXT("Inconsistent Shuffle counts return false"), Fixture.Probe->PlayPresentationRecord(InvalidShuffle, MakeToken(3)));

	const FPresentationRecord ConsistentEnergy = MakeEnergyRecord(4);
	TestFalse(TEXT("Record/Token sequence mismatch returns false"), Fixture.Probe->PlayPresentationRecord(ConsistentEnergy, MakeToken(44)));

	TestFalse(TEXT("All invalid Begins leave zero local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("All invalid Begins leave zero local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestTrue(TEXT("All invalid Begins reset the local Record type"), Fixture.Probe->ActiveLocalType() == EBattlePresentationRecordType::None);
	TestEqual(TEXT("Invalid Energy leaves its surface unchanged"), Fixture.EnergyText->GetText().ToString(), FString(TEXT("energy-sentinel")));
	TestEqual(TEXT("Invalid Block leaves its text unchanged"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("block-sentinel")));
	TestTrue(TEXT("Invalid Block leaves badge visibility unchanged"), Fixture.PlayerBlock.Badge->GetVisibility() == ESlateVisibility::Hidden);
	TestEqual(TEXT("Invalid Shuffle leaves Draw unchanged"), Fixture.DrawText->GetText().ToString(), FString(TEXT("draw-sentinel")));
	TestEqual(TEXT("Invalid Shuffle leaves Discard unchanged"), Fixture.DiscardText->GetText().ToString(), FString(TEXT("discard-sentinel")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR6DestructCleanupTest,
	"SlayTheSpireDemo.Phase6UIA2N.R6.DestructCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR6DestructCleanupTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR6Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R6 destruction fixture."));
		return false;
	}

	const FPresentationRecord Record = MakeShuffleRecord(1);
	const FPresentationPlaybackToken Token = MakeToken(1);
	TestTrue(TEXT("Destruction fixture begins an R6 presentation"), Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("R6 presentation owns local state before destruction"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("R6 presentation owns a timer before destruction"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("NativeDestruct clears R6 local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("NativeDestruct clears the R6 timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("NativeDestruct does not dispatch visual Cancel"), Fixture.Probe->CancelDispatchCount, 0);
	TestEqual(TEXT("NativeDestruct does not historical-restore Draw"), Fixture.DrawText->GetText().ToString(), FString(TEXT("6")));
	TestEqual(TEXT("NativeDestruct does not historical-restore Discard"), Fixture.DiscardText->GetText().ToString(), FString(TEXT("0")));
	return true;
}

#endif
