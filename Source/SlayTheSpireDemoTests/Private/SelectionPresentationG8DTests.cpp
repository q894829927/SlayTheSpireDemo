#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8DTest
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

		FPresentationRecord Record;
		Record.BattleId = Before.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = 1;
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
		Envelope.Records.Add(Record);

		Envelope.FinalSnapshot.Player.HP = Record.Damage.HPAfter;
		Envelope.FinalSnapshot.Player.bDead = Record.Damage.HPAfter <= 0;
		return Envelope;
	}
}

using namespace SelectionPresentationG8DTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8DDetachedDamageIsTrulyNonBlocking,
	"SlayTheSpireDemo.SelectionPresentation.G8D.DetachedDamage.NonBlockingReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8DDetachedDamageIsTrulyNonBlocking::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-D fixture initializes"), Fixture.IsReady())) return false;

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken SessionBefore;
	if (!TestTrue(TEXT("Presentation session exists"),
		Fixture.Controller->TryGetPresentationSessionToken(SessionBefore))) return false;

	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 3));

	TestEqual(TEXT("Detached prepare occurs once"), Fixture.Widget->DetachedPrepareCount, 1);
	TestEqual(TEXT("Detached activation occurs once"), Fixture.Widget->DetachedActivateCount, 1);
	TestEqual(TEXT("Committed hit/attack cue occurs once"), Fixture.Widget->DetachedCueCount, 1);
	TestFalse(TEXT("Detached Damage owns no Blocking completion"),
		Fixture.Controller->IsWaitingForCompletionForTesting());
	TestFalse(TEXT("Detached Damage starts no Native Blocking playback"),
		Fixture.Widget->IsLocalPresentationActive());
	TestEqual(TEXT("Formal HP publishes immediately"),
		Fixture.ViewModel->Player.HP,
		Baseline.Player.HP - 3);
	TestTrue(TEXT("G8-D creates no compatibility debt"),
		Fixture.Controller->GetCompatibilityDebtSecondsForTesting() <= KINDA_SMALL_NUMBER);
	TestFalse(TEXT("G8-D starts no compatibility timer"),
		Fixture.Controller->IsCompatibilityDebtServiceActiveForTesting());
	TestFalse(TEXT("Pure DamageNumber tail is not a skippable delay"),
		Fixture.Controller->HasSkippablePresentationDelay());
	TestFalse(TEXT("Input is released while DamageNumber is still alive"),
		Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Normal player surface is restored"),
		Fixture.ViewModel->InteractionState,
		EBattleHUDInteractionState::Idle);

	FPresentationSessionToken CapturedSession;
	int64 CapturedBattleId = 0;
	int64 CapturedRevision = 0;
	TestFalse(TEXT("Pure cosmetic tail cannot capture FastInput catch-up"),
		Fixture.Controller->TryCaptureFastInputCatchUpTarget(
			CapturedSession,
			CapturedBattleId,
			CapturedRevision));

	FPresentationSessionToken SessionAfter;
	TestTrue(TEXT("Session remains valid"),
		Fixture.Controller->TryGetPresentationSessionToken(SessionAfter));
	TestTrue(TEXT("Detached commit preserves authority session"), SessionAfter == SessionBefore);

	TestTrue(TEXT("DamageNumber remains independently alive after readiness"),
		Fixture.Widget->CancelDetachedDamageVisual(Fixture.Widget->LastActivatedDetachedToken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8DPrepareDeclineFallsBackToBlockingDamage,
	"SlayTheSpireDemo.SelectionPresentation.G8D.DetachedDamage.PrepareDeclineFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8DPrepareDeclineFallsBackToBlockingDamage::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-D fixture initializes"), Fixture.IsReady())) return false;
	Fixture.Widget->SetAcceptDetachedDamage(false);
	Fixture.Widget->SetAcceptSyntheticPlayback(true);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 2));

	TestEqual(TEXT("Detached prepare declines before commit"), Fixture.Widget->DetachedPrepareCount, 0);
	TestTrue(TEXT("Old Damage path owns Blocking completion after decline"),
		Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Old Native playback starts after detached decline"),
		Fixture.Widget->IsLocalPresentationActive());
	TestTrue(TEXT("Blocking fallback remains FastInput-skippable"),
		Fixture.Controller->HasSkippablePresentationDelay());
	Fixture.Controller->SkipPresentation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8DRuntimeDisableKeepsSessionAndRestoresBlockingPath,
	"SlayTheSpireDemo.SelectionPresentation.G8D.RuntimeDisable.KeepsSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8DRuntimeDisableKeepsSessionAndRestoresBlockingPath::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-D fixture initializes"), Fixture.IsReady())) return false;
	Fixture.Widget->SetAcceptSyntheticPlayback(true);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken SessionBefore;
	if (!TestTrue(TEXT("Session exists"),
		Fixture.Controller->TryGetPresentationSessionToken(SessionBefore))) return false;

	const int64 FirstResolution = Fixture.NextResolutionId();
	const FPresentationResolutionEnvelope FirstDamage =
		MakeDamageEnvelope(Baseline, FirstResolution, 1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(FirstDamage);
	TestFalse(TEXT("First detached Damage is already input-ready"), Fixture.ViewModel->bInputLocked);
	const int32 CancelBeforeDisable = Fixture.Widget->DetachedCancelCount;

	Fixture.Controller->SetDetachedDamageG8CEnabled(false);

	FPresentationSessionToken SessionAfterDisable;
	TestTrue(TEXT("Session still exists after feature disable"),
		Fixture.Controller->TryGetPresentationSessionToken(SessionAfterDisable));
	TestTrue(TEXT("Feature disable is not authority replacement"), SessionAfterDisable == SessionBefore);
	TestTrue(TEXT("Feature disable retires current detached cosmetics"),
		Fixture.Widget->DetachedCancelCount > CancelBeforeDisable);
	TestFalse(TEXT("Feature disable leaves no skippable cosmetic delay"),
		Fixture.Controller->HasSkippablePresentationDelay());

	const FPresentationResolutionEnvelope SecondDamage =
		MakeDamageEnvelope(FirstDamage.FinalSnapshot, FirstResolution + 1, 1);
	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(SecondDamage);
	TestEqual(TEXT("Disabled detached feature accepts no new prepare"),
		Fixture.Widget->DetachedPrepareCount,
		1);
	TestTrue(TEXT("Subsequent Damage returns to old Blocking path"),
		Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Old Blocking Damage owns Native playback after disable"),
		Fixture.Widget->IsLocalPresentationActive());
	Fixture.Controller->SkipPresentation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8DWidgetReplacementRetiresCosmeticWithoutRelockingInput,
	"SlayTheSpireDemo.SelectionPresentation.G8D.Cosmetic.WidgetReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8DWidgetReplacementRetiresCosmeticWithoutRelockingInput::RunTest(const FString& Parameters)
{
	FPresentationFixture Fixture;
	if (!TestTrue(TEXT("G8-D fixture initializes"), Fixture.IsReady())) return false;

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.GetBaseline(Baseline))) return false;
	FPresentationSessionToken OldSession;
	if (!TestTrue(TEXT("Old session exists"),
		Fixture.Controller->TryGetPresentationSessionToken(OldSession))) return false;

	Fixture.BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, Fixture.NextResolutionId(), 1));
	if (!TestFalse(TEXT("Damage tail does not keep input locked"), Fixture.ViewModel->bInputLocked))
	{
		return false;
	}

	UPhase6UIA2NR5HUDProbe* NewWidget =
		NewObject<UPhase6UIA2NR5HUDProbe>(Fixture.BattleFixture.World);
	if (!TestTrue(TEXT("Replacement Widget exists"), IsValid(NewWidget))) return false;
	NewWidget->SetTestWorld(Fixture.BattleFixture.World);
	NewWidget->SetViewModel(Fixture.ViewModel);
	NewWidget->SetAcceptDetachedDamage(true);

	const int32 CancelBeforeReplacement = Fixture.Widget->DetachedCancelCount;
	Fixture.Controller->SetWidget(NewWidget);
	Fixture.Widget->SetPresentationController(nullptr);
	NewWidget->SetPresentationController(Fixture.Controller);

	FPresentationSessionToken NewSession;
	TestTrue(TEXT("Replacement establishes new session"),
		Fixture.Controller->TryGetPresentationSessionToken(NewSession));
	TestTrue(TEXT("Old session is stale after Widget replacement"), NewSession != OldSession);
	TestTrue(TEXT("Old Widget retires its cosmetic tail"),
		Fixture.Widget->DetachedCancelCount > CancelBeforeReplacement);
	TestFalse(TEXT("Replacement creates no skippable delay"),
		Fixture.Controller->HasSkippablePresentationDelay());
	TestFalse(TEXT("Replacement does not relock ready input"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Replacement preserves normal interaction surface"),
		Fixture.ViewModel->InteractionState,
		EBattleHUDInteractionState::Idle);

	NewWidget->SetPresentationController(nullptr);
	NewWidget->SetViewModel(nullptr);
	return true;
}

#endif
