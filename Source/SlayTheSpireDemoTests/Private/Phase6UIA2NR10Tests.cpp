#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR10TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR10Test
{
	constexpr int64 TestBattleId = 1001;
	constexpr int64 TestResolutionId = 1002;
	const FName PlayerPresentationId(TEXT("PlayerPresentation"));
	const FName EnemyPresentationId(TEXT("EnemyPresentation"));

	FPresentationPlaybackToken MakeToken(int64 Sequence, int64 Generation = 1)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = TestBattleId;
		Token.ResolutionId = TestResolutionId;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		return Token;
	}

	FPresentationRecord MakeTerminalRecord(
		int64 Sequence,
		EBattlePresentationRecordType Type)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = Type;
		if (Type == EBattlePresentationRecordType::Victory)
		{
			Record.Terminal.WinnerPresentationId = PlayerPresentationId;
			Record.Terminal.DefeatedPresentationId = EnemyPresentationId;
		}
		else if (Type == EBattlePresentationRecordType::Defeat)
		{
			Record.Terminal.WinnerPresentationId = EnemyPresentationId;
			Record.Terminal.DefeatedPresentationId = PlayerPresentationId;
		}
		else if (Type == EBattlePresentationRecordType::ResolutionFault)
		{
			Record.ResolutionFault.Reason = TEXT("R10 deterministic fault");
			Record.ResolutionFault.ExecutedActionCount = 3;
			Record.ResolutionFault.LastActionName = TEXT("TestAction");
		}
		return Record;
	}

	struct FFixture
	{
		UWorld* World = nullptr;
		UPhase6UIA2NR10HUDProbe* Probe = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UOverlay* TerminalOverlay = nullptr;
		UTextBlock* OutcomeText = nullptr;
		UTextBlock* FeedbackText = nullptr;
		UButton* EndTurnButton = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}
			Probe = NewObject<UPhase6UIA2NR10HUDProbe>(World);
			ViewModel = IsValid(Probe) ? NewObject<UBattleHUDViewModel>(Probe) : nullptr;
			TerminalOverlay = IsValid(Probe) ? NewObject<UOverlay>(Probe) : nullptr;
			OutcomeText = IsValid(Probe) ? NewObject<UTextBlock>(Probe) : nullptr;
			FeedbackText = IsValid(Probe) ? NewObject<UTextBlock>(Probe) : nullptr;
			EndTurnButton = IsValid(Probe) ? NewObject<UButton>(Probe) : nullptr;
			if (!IsValid(Probe) || !IsValid(ViewModel) || !IsValid(TerminalOverlay)
				|| !IsValid(OutcomeText) || !IsValid(FeedbackText) || !IsValid(EndTurnButton))
			{
				return;
			}

			Probe->SetTestWorld(World);
			Probe->SetViewModelForTesting(ViewModel);
			Probe->ConfigureTerminalSurfaces(
				TerminalOverlay,
				OutcomeText,
				FeedbackText,
				EndTurnButton);
			ViewModel->BattleId = TestBattleId;
			ViewModel->Player.PresentationId = PlayerPresentationId;
			ViewModel->Enemy.PresentationId = EnemyPresentationId;
			ViewModel->Player.HP = 80;
			ViewModel->Player.MaxHP = 80;
			ViewModel->Enemy.HP = 100;
			ViewModel->Enemy.MaxHP = 100;
			ViewModel->Outcome = EBattleHUDOutcome::None;
			ViewModel->InteractionState = EBattleHUDInteractionState::Resolving;
			ViewModel->bInputLocked = true;
			TerminalOverlay->SetVisibility(ESlateVisibility::Collapsed);
			OutcomeText->SetText(FText::GetEmpty());
		}

		~FFixture()
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
			return IsValid(World) && IsValid(Probe) && IsValid(ViewModel)
				&& IsValid(TerminalOverlay) && IsValid(OutcomeText)
				&& IsValid(FeedbackText) && IsValid(EndTurnButton);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR10VictoryOrderingTest,
	"SlayTheSpireDemo.Phase6UIA2N.R10.Terminal.VictoryOrderingAndFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR10VictoryOrderingTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR10Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R10 fixture."));
		return false;
	}

	const FPresentationRecord Victory = MakeTerminalRecord(1, EBattlePresentationRecordType::Victory);
	const FPresentationPlaybackToken Token = MakeToken(1);

	TestFalse(TEXT("Victory cannot appear before lethal Enemy state is historical"),
		Fixture.Probe->InvokeBeginDirectForTesting(Victory, Token));
	TestFalse(TEXT("Rejected early Victory owns no local presentation"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Rejected early Victory owns no timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Rejected early Victory leaves terminal hidden"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Collapsed);

	Fixture.ViewModel->Enemy.HP = 0;
	Fixture.ViewModel->Enemy.bDead = true;
	TestTrue(TEXT("Victory begins only after lethal Enemy state is visible historically"),
		Fixture.Probe->InvokeBeginDirectForTesting(Victory, Token));
	TestEqual(TEXT("Victory owns exact Record type"),
		Fixture.Probe->ActiveLocalType(), EBattlePresentationRecordType::Victory);
	TestTrue(TEXT("Victory owns exact token"), Fixture.Probe->ActiveLocalToken() == Token);
	TestEqual(TEXT("Victory terminal is visible"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Victory text is frozen formal surface"),
		Fixture.OutcomeText->GetText().ToString(), FString(TEXT("胜利")));
	TestEqual(TEXT("Terminal Begin never mutates historical ViewModel Outcome"),
		Fixture.ViewModel->Outcome, EBattleHUDOutcome::None);

	FPresentationPlaybackToken StaleToken = Token;
	StaleToken.LocalPlaybackGeneration = 77;
	Fixture.Probe->InvokeFinishForTesting(StaleToken);
	TestTrue(TEXT("Stale terminal Finish is no-op"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Stale Finish keeps terminal visual"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);

	Fixture.Probe->InvokeFinishForTesting(Token);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestFalse(TEXT("Exact terminal Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact terminal Finish clears timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Victory Finish retains committed visual"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR10DefeatCancelTest,
	"SlayTheSpireDemo.Phase6UIA2N.R10.Terminal.DefeatCancelHistoricalRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR10DefeatCancelTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR10Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R10 fixture."));
		return false;
	}

	Fixture.ViewModel->Player.HP = 0;
	Fixture.ViewModel->Player.bDead = true;
	const FPresentationRecord Defeat = MakeTerminalRecord(2, EBattlePresentationRecordType::Defeat);
	const FPresentationPlaybackToken Token = MakeToken(2);
	TestTrue(TEXT("Defeat begins from historical lethal Player state"),
		Fixture.Probe->InvokeBeginDirectForTesting(Defeat, Token));
	TestEqual(TEXT("Defeat text"),
		Fixture.OutcomeText->GetText().ToString(), FString(TEXT("战斗失败")));

	FPresentationPlaybackToken WrongToken = Token;
	WrongToken.LocalPlaybackGeneration = 8;
	Fixture.Probe->InvokeCancelForTesting(WrongToken);
	TestTrue(TEXT("Wrong-token terminal Cancel is no-op"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Wrong-token Cancel keeps Defeat visible"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);

	Fixture.Probe->InvokeCancelForTesting(Token);
	TestFalse(TEXT("Exact terminal Cancel clears ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact terminal Cancel clears timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Cancel restores historical non-terminal surface"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Exact Cancel clears historical Outcome text"), Fixture.OutcomeText->GetText().IsEmpty());
	TestEqual(TEXT("Cancel never mutates historical ViewModel Outcome"),
		Fixture.ViewModel->Outcome, EBattleHUDOutcome::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR10ResolutionFaultTest,
	"SlayTheSpireDemo.Phase6UIA2N.R10.Terminal.ResolutionFaultValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR10ResolutionFaultTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR10Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R10 fixture."));
		return false;
	}

	FPresentationRecord Invalid = MakeTerminalRecord(3, EBattlePresentationRecordType::ResolutionFault);
	Invalid.ResolutionFault.Reason = FText::GetEmpty();
	TestFalse(TEXT("Empty ResolutionFault reason is rejected"),
		Fixture.Probe->InvokeBeginDirectForTesting(Invalid, MakeToken(3)));
	TestFalse(TEXT("Invalid fault owns no timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Invalid fault leaves terminal hidden"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Collapsed);

	Invalid = MakeTerminalRecord(4, EBattlePresentationRecordType::ResolutionFault);
	Invalid.ResolutionFault.ExecutedActionCount = -1;
	TestFalse(TEXT("Negative executed action count is rejected"),
		Fixture.Probe->InvokeBeginDirectForTesting(Invalid, MakeToken(4)));
	TestFalse(TEXT("Second invalid fault still owns no local state"), Fixture.Probe->IsLocalPresentationActive());

	const FPresentationRecord Valid = MakeTerminalRecord(5, EBattlePresentationRecordType::ResolutionFault);
	const FPresentationPlaybackToken Token = MakeToken(5);
	TestTrue(TEXT("Valid isolated ResolutionFault begins"),
		Fixture.Probe->InvokeBeginDirectForTesting(Valid, Token));
	TestEqual(TEXT("ResolutionFault text"),
		Fixture.OutcomeText->GetText().ToString(), FString(TEXT("战斗结算异常")));
	TestEqual(TEXT("Fault Begin does not manufacture Gameplay outcome"),
		Fixture.ViewModel->Outcome, EBattleHUDOutcome::None);
	Fixture.Probe->InvokeFinishForTesting(Token);
	TestFalse(TEXT("Fault exact Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR10PresentationUnavailableTest,
	"SlayTheSpireDemo.Phase6UIA2N.R10.PresentationUnavailable.SeparateAndLocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR10PresentationUnavailableTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR10Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R10 fixture."));
		return false;
	}

	Fixture.ViewModel->Outcome = EBattleHUDOutcome::ResolutionFaulted;
	Fixture.Probe->RefreshTerminalForTesting();
	TestEqual(TEXT("Precondition: fault terminal is visible"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);

	Fixture.ViewModel->EnterPresentationUnavailable(
		FText::FromString(TEXT("TEST_PRESENTATION_UNAVAILABLE")));
	Fixture.Probe->RefreshAvailabilityForTesting();
	Fixture.Probe->RefreshInputForTesting();

	TestEqual(TEXT("PresentationUnavailable has dedicated interaction state"),
		Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	TestEqual(TEXT("PresentationUnavailable is not Gameplay ResolutionFault"),
		Fixture.ViewModel->Outcome, EBattleHUDOutcome::None);
	TestEqual(TEXT("PresentationUnavailable hides terminal overlay"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("PresentationUnavailable clears terminal outcome text"),
		Fixture.OutcomeText->GetText().IsEmpty());
	TestEqual(TEXT("PresentationUnavailable renders ViewModel reason as feedback"),
		Fixture.FeedbackText->GetText().ToString(), FString(TEXT("TEST_PRESENTATION_UNAVAILABLE")));
	TestFalse(TEXT("PresentationUnavailable keeps EndTurn disabled"), Fixture.EndTurnButton->GetIsEnabled());

	Fixture.ViewModel->Enemy.HP = 0;
	Fixture.ViewModel->Enemy.bDead = true;
	const FPresentationRecord Victory = MakeTerminalRecord(6, EBattlePresentationRecordType::Victory);
	TestFalse(TEXT("PresentationUnavailable cannot be treated as a terminal Record owner"),
		Fixture.Probe->InvokeBeginDirectForTesting(Victory, MakeToken(6)));
	TestFalse(TEXT("Unavailable terminal rejection owns no timer"), Fixture.Probe->IsLocalFinishTimerSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR10InvalidIdentityDestructTest,
	"SlayTheSpireDemo.Phase6UIA2N.R10.Terminal.InvalidIdentityAndDestruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR10InvalidIdentityDestructTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR10Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R10 fixture."));
		return false;
	}

	Fixture.ViewModel->Enemy.HP = 0;
	Fixture.ViewModel->Enemy.bDead = true;
	FPresentationRecord InvalidVictory = MakeTerminalRecord(7, EBattlePresentationRecordType::Victory);
	InvalidVictory.Terminal.WinnerPresentationId = EnemyPresentationId;
	InvalidVictory.Terminal.DefeatedPresentationId = PlayerPresentationId;
	TestFalse(TEXT("Victory with reversed terminal identities is rejected"),
		Fixture.Probe->InvokeBeginDirectForTesting(InvalidVictory, MakeToken(7)));
	TestEqual(TEXT("Invalid identity leaves terminal hidden"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Collapsed);

	const FPresentationRecord Victory = MakeTerminalRecord(8, EBattlePresentationRecordType::Victory);
	const FPresentationPlaybackToken Token = MakeToken(8);
	TestTrue(TEXT("Valid Victory begins after invalid attempt"),
		Fixture.Probe->InvokeBeginDirectForTesting(Victory, Token));
	TestTrue(TEXT("Valid terminal owns timer before destruction"), Fixture.Probe->IsLocalFinishTimerSet());
	Fixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("NativeDestruct clears terminal local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("NativeDestruct clears terminal timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Destruction does not historical-restore the departing terminal surface"),
		Fixture.TerminalOverlay->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

#endif
