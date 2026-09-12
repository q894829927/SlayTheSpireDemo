#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationDamageTiming.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8CTest
{
	using namespace Phase6UIA1Test;

	struct FPresentationFixture
	{
		FHUDTestFixture BattleFixture;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2NR5HUDProbe* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;

		FPresentationFixture()
			: BattleFixture(ECardTargetType::None, 0, 0)
		{
			BattleFixture.DrainInitialReady();
			if (!IsValid(BattleFixture.World) || !IsValid(BattleFixture.Battle))
			{
				return;
			}

			ViewModel = NewObject<UBattleHUDViewModel>(BattleFixture.World);
			Widget = NewObject<UPhase6UIA2NR5HUDProbe>(BattleFixture.World);
			Controller = NewObject<UBattlePresentationController>(BattleFixture.World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller))
			{
				return;
			}

			Widget->SetTestWorld(BattleFixture.World);
			Widget->SetViewModel(ViewModel);
			Widget->SetAcceptDetachedDamage(true);
			if (!ViewModel->Initialize(BattleFixture.Battle, true)
				|| !Controller->Initialize(BattleFixture.Battle, ViewModel, Widget))
			{
				return;
			}
			Widget->SetPresentationController(Controller);
		}

		~FPresentationFixture()
		{
			if (IsValid(Widget))
			{
				Widget->SetPresentationController(nullptr);
				Widget->SetViewModel(nullptr);
			}
			if (IsValid(Controller)) Controller->Shutdown();
			if (IsValid(ViewModel)) ViewModel->Shutdown();
		}

		bool IsReady() const
		{
			return IsValid(BattleFixture.World)
				&& IsValid(BattleFixture.Battle)
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller);
		}

		bool GetBaseline(FPresentationStateSnapshot& OutBaseline) const
		{
			return IsValid(BattleFixture.Battle)
				&& BattleFixture.Battle->TryGetLatestFrozenPresentationBaseline(OutBaseline);
		}

		int64 NextResolutionId(int64 Offset = 100) const
		{
			return IsValid(BattleFixture.Battle)
				? static_cast<int64>(BattleFixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + Offset
				: Offset;
		}
	};

	FPresentationRecord MakeDamageRecord(
		const FPresentationStateSnapshot& Before,
		int64 ResolutionId,
		int64 Sequence,
		int32 HPDamage)
	{
		FPresentationRecord Record;
		Record.BattleId = Before.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::Damage;
		Record.Damage.SourcePresentationId = Before.Enemy.PresentationId;
		Record.Damage.TargetPresentationId = Before.Player.PresentationId;
		Record.Damage.DamageKind = EDamageKind::Attack;
		Record.Damage.IncomingDamage = HPDamage;
		Record.Damage.HPBefore = Before.Player.HP;
		Record.Damage.HPAfter = FMath::Max(0, Before.Player.HP - HPDamage);
		Record.Damage.BlockBefore = Before.Player.Block;
		Record.Damage.BlockAfter = Before.Player.Block;
		Record.Damage.BlockedDamage = 0;
		Record.Damage.HPDamage = Record.Damage.HPBefore - Record.Damage.HPAfter;
		return Record;
	}

	FPresentationResolutionEnvelope MakeDamageEnvelope(
		const FPresentationStateSnapshot& Before,
		int64 ResolutionId,
		int32 HPDamage)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Before.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Before.StateRevision;
		Envelope.FinalSnapshot = Before;
		FPresentationRecord Record = MakeDamageRecord(Before, ResolutionId, 1, HPDamage);
		Envelope.Records.Add(Record);
		Envelope.FinalSnapshot.Player.HP = Record.Damage.HPAfter;
		Envelope.FinalSnapshot.Player.bDead = Record.Damage.HPAfter <= 0;
		return Envelope;
	}

	FPresentationResolutionEnvelope MakeBlockEnvelope(
		const FPresentationStateSnapshot& Before,
		int64 ResolutionId,
		int32 BlockDelta)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Before.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Before.StateRevision;
		Envelope.FinalSnapshot = Before;

		FPresentationRecord Record;
		Record.BattleId = Before.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::BlockChanged;
		Record.BlockChanged.SourcePresentationId = Before.Player.PresentationId;
		Record.BlockChanged.TargetPresentationId = Before.Player.PresentationId;
		Record.BlockChanged.Reason = EBlockPresentationReason::Gain;
		Record.BlockChanged.BlockBefore = Before.Player.Block;
		Record.BlockChanged.BlockAfter = Before.Player.Block + BlockDelta;
		Record.BlockChanged.BlockDelta = BlockDelta;
		Envelope.Records.Add(Record);
		Envelope.FinalSnapshot.Player.Block = Record.BlockChanged.BlockAfter;
		return Envelope;
	}
}

using namespace SelectionPresentationG8CTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CDetachedDamageCommitsWithoutBlockingPlayback,
	"SlayTheSpireDemo.SelectionPresentation.G8C.DetachedDamage.CommitAndDebt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CDetachedDamageCommitsWithoutBlockingPlayback::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-C fixture initializes"), Fixture.IsReady())) return false;

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken SessionBefore;
	if (!TestTrue(TEXT("Presentation session exists"), Fixture.Controller->TryGetPresentationSessionToken(SessionBefore))) return false;

	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 3));

	TestEqual(TEXT("Detached prepare occurs once"), Fixture.Widget->DetachedPrepareCount, 1);
	TestEqual(TEXT("Detached activation occurs once"), Fixture.Widget->DetachedActivateCount, 1);
	TestEqual(TEXT("Committed hit/attack cue occurs once"), Fixture.Widget->DetachedCueCount, 1);
	TestFalse(TEXT("Detached Damage does not own Blocking completion"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestFalse(TEXT("Detached Damage does not activate Native Blocking playback"), Fixture.Widget->IsLocalPresentationActive());
	TestEqual(TEXT("Formal Damage reducer publishes HP immediately"), Fixture.ViewModel->Player.HP, Baseline.Player.HP - 3);
	TestTrue(TEXT("One Damage accrues legacy compatibility debt"),
		FMath::IsNearlyEqual(
			Fixture.Controller->GetCompatibilityDebtSecondsForTesting(),
			PresentationDamageTiming::GetLegacyDamageBlockingDuration(),
			KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Debt service uses Controller-owned timer"), Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting());
	TestTrue(TEXT("Debt-only wait is FastInput skippable"), Fixture.Controller->HasSkippablePresentationDelay());

	FPresentationSessionToken SessionAfter;
	TestTrue(TEXT("Session remains valid"), Fixture.Controller->TryGetPresentationSessionToken(SessionAfter));
	TestTrue(TEXT("Detached commit preserves authority session"), SessionAfter == SessionBefore);
	Fixture.Controller->SkipPresentation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CPrepareDeclineFallsBackToBlockingDamage,
	"SlayTheSpireDemo.SelectionPresentation.G8C.DetachedDamage.PrepareDeclineFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CPrepareDeclineFallsBackToBlockingDamage::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-C fixture initializes"), Fixture.IsReady())) return false;
	Fixture.Widget->SetAcceptDetachedDamage(false);
	Fixture.Widget->SetAcceptSyntheticPlayback(true);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 2));

	TestEqual(TEXT("Detached prepare declines before commit"), Fixture.Widget->DetachedPrepareCount, 0);
	TestTrue(TEXT("Old Damage path owns Blocking completion after decline"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Old Native playback starts after detached decline"), Fixture.Widget->IsLocalPresentationActive());
	TestTrue(TEXT("No staging debt exists before old Blocking Damage commits"),
		Fixture.Controller->GetCompatibilityDebtSecondsForTesting() <= KINDA_SMALL_NUMBER);
	Fixture.Controller->SkipPresentation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CSkipClearsDebtButPreservesSession,
	"SlayTheSpireDemo.SelectionPresentation.G8C.Debt.SkipPreservesSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CSkipClearsDebtButPreservesSession::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-C fixture initializes"), Fixture.IsReady())) return false;

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken SessionBefore;
	if (!TestTrue(TEXT("Session exists"), Fixture.Controller->TryGetPresentationSessionToken(SessionBefore))) return false;

	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 1));
	TestTrue(TEXT("Debt exists before Skip"), Fixture.Controller->GetCompatibilityDebtSecondsForTesting() > 0.0f);
	const int32 CancelCountBeforeSkip = Fixture.Widget->DetachedCancelCount;

	Fixture.Controller->SkipPresentation();

	FPresentationSessionToken SessionAfter;
	TestTrue(TEXT("Session still exists after ordinary Skip"), Fixture.Controller->TryGetPresentationSessionToken(SessionAfter));
	TestTrue(TEXT("Ordinary Skip keeps exact SessionToken"), SessionAfter == SessionBefore);
	TestTrue(TEXT("Ordinary Skip clears compatibility debt"), Fixture.Controller->GetCompatibilityDebtSecondsForTesting() <= KINDA_SMALL_NUMBER);
	TestFalse(TEXT("No skippable delay remains after debt-only Skip"), Fixture.Controller->HasSkippablePresentationDelay());
	TestTrue(TEXT("Skip cancels current-session detached cosmetic"), Fixture.Widget->DetachedCancelCount > CancelCountBeforeSkip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CBlockingChronologyFreezesAndResumesDebt,
	"SlayTheSpireDemo.SelectionPresentation.G8C.Debt.BlockingChronologyFreeze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CBlockingChronologyFreezesAndResumesDebt::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-C fixture initializes"), Fixture.IsReady())) return false;
	Fixture.Widget->SetAcceptSyntheticPlayback(true);
	Fixture.Widget->SetAcceptSyntheticBlockPlayback(true);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	const int64 FirstResolution = Fixture.NextResolutionId();
	const FPresentationResolutionEnvelope FirstDamage = MakeDamageEnvelope(Baseline, FirstResolution, 1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(FirstDamage);
	if (!TestTrue(TEXT("First detached Damage starts debt service"), Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting())) return false;

	const float DebtBeforeBlocking = Fixture.Controller->GetCompatibilityDebtSecondsForTesting();
	const FPresentationResolutionEnvelope BlockingEnvelope = MakeBlockEnvelope(
		FirstDamage.FinalSnapshot,
		FirstResolution + 1,
		2);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(BlockingEnvelope);

	TestFalse(TEXT("Accepted Blocking chronology pauses debt timer"), Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting());
	TestTrue(TEXT("Blocking Record owns normal completion"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Frozen remaining debt is preserved"), Fixture.Controller->GetCompatibilityDebtSecondsForTesting() > 0.0f);
	TestTrue(TEXT("Blocking start does not increase debt"), Fixture.Controller->GetCompatibilityDebtSecondsForTesting() <= DebtBeforeBlocking + KINDA_SMALL_NUMBER);

	Fixture.Widget->InvokeFinishForTesting(Fixture.Widget->ActiveLocalToken());
	TestFalse(TEXT("Blocking Record completes"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Debt resumes only after chronology is otherwise ready"), Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting());

	const float ResumedDebt = Fixture.Controller->GetCompatibilityDebtSecondsForTesting();
	const FPresentationResolutionEnvelope SecondDamage = MakeDamageEnvelope(
		BlockingEnvelope.FinalSnapshot,
		FirstResolution + 2,
		1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(SecondDamage);
	TestEqual(TEXT("Two detached Damage records prepare independently"), Fixture.Widget->DetachedPrepareCount, 2);
	TestTrue(TEXT("Second committed Damage adds another full legacy debt"),
		Fixture.Controller->GetCompatibilityDebtSecondsForTesting()
			>= ResumedDebt + PresentationDamageTiming::GetLegacyDamageBlockingDuration() - KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Accumulated debt resumes after second Damage envelope"), Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting());
	Fixture.Controller->SkipPresentation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CRuntimeDisableKeepsSessionAndRestoresBlockingPath,
	"SlayTheSpireDemo.SelectionPresentation.G8C.RuntimeDisable.KeepsSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CRuntimeDisableKeepsSessionAndRestoresBlockingPath::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-C fixture initializes"), Fixture.IsReady())) return false;
	Fixture.Widget->SetAcceptSyntheticPlayback(true);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken SessionBefore;
	if (!TestTrue(TEXT("Session exists"), Fixture.Controller->TryGetPresentationSessionToken(SessionBefore))) return false;

	const int64 FirstResolution = Fixture.NextResolutionId();
	const FPresentationResolutionEnvelope FirstDamage = MakeDamageEnvelope(Baseline, FirstResolution, 1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(FirstDamage);
	const int32 CancelBeforeDisable = Fixture.Widget->DetachedCancelCount;

	Fixture.Controller->SetDetachedDamageG8CEnabled(false);

	FPresentationSessionToken SessionAfterDisable;
	TestTrue(TEXT("Session still exists after feature disable"), Fixture.Controller->TryGetPresentationSessionToken(SessionAfterDisable));
	TestTrue(TEXT("Feature disable is not authority replacement"), SessionAfterDisable == SessionBefore);
	TestTrue(TEXT("Feature disable clears staging debt"), Fixture.Controller->GetCompatibilityDebtSecondsForTesting() <= KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Feature disable retires current detached cosmetics"), Fixture.Widget->DetachedCancelCount > CancelBeforeDisable);

	const FPresentationResolutionEnvelope SecondDamage = MakeDamageEnvelope(
		FirstDamage.FinalSnapshot,
		FirstResolution + 1,
		1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(SecondDamage);
	TestEqual(TEXT("Disabled G8-C accepts no new detached prepare"), Fixture.Widget->DetachedPrepareCount, 1);
	TestTrue(TEXT("Subsequent Damage returns to old Blocking path"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Old Blocking Damage owns Native playback after disable"), Fixture.Widget->IsLocalPresentationActive());
	Fixture.Controller->SkipPresentation();
	return true;
}

#endif
