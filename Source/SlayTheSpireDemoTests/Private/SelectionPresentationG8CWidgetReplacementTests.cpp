#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8CWidgetReplacementTest
{
	using namespace Phase6UIA1Test;

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

using namespace SelectionPresentationG8CWidgetReplacementTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FG8CDebtOnlyWidgetReplacementCatchesUp,
	"SlayTheSpireDemo.SelectionPresentation.G8C.Debt.WidgetReplacementCatchUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FG8CDebtOnlyWidgetReplacementCatchesUp::RunTest(const FString& Parameters)
{
	FHUDTestFixture BattleFixture(ECardTargetType::None, 0, 0);
	BattleFixture.DrainInitialReady();
	if (!TestTrue(TEXT("Battle fixture initializes"),
		IsValid(BattleFixture.World) && IsValid(BattleFixture.Battle)))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(BattleFixture.World);
	UPhase6UIA2NR5HUDProbe* OldWidget = NewObject<UPhase6UIA2NR5HUDProbe>(BattleFixture.World);
	UPhase6UIA2NR5HUDProbe* NewWidget = NewObject<UPhase6UIA2NR5HUDProbe>(BattleFixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(BattleFixture.World);
	if (!TestTrue(TEXT("Replacement fixture objects initialize"),
		IsValid(ViewModel) && IsValid(OldWidget) && IsValid(NewWidget) && IsValid(Controller)))
	{
		return false;
	}

	OldWidget->SetTestWorld(BattleFixture.World);
	OldWidget->SetViewModel(ViewModel);
	OldWidget->SetAcceptDetachedDamage(true);
	NewWidget->SetTestWorld(BattleFixture.World);
	NewWidget->SetViewModel(ViewModel);
	NewWidget->SetAcceptDetachedDamage(true);

	if (!TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(BattleFixture.Battle, true))
		|| !TestTrue(TEXT("Controller initializes"),
			Controller->Initialize(BattleFixture.Battle, ViewModel, OldWidget)))
	{
		if (IsValid(Controller)) Controller->Shutdown();
		if (IsValid(ViewModel)) ViewModel->Shutdown();
		return false;
	}
	OldWidget->SetPresentationController(Controller);

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"),
		BattleFixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline)))
	{
		OldWidget->SetPresentationController(nullptr);
		Controller->Shutdown();
		ViewModel->Shutdown();
		return false;
	}

	FPresentationSessionToken OldSession;
	if (!TestTrue(TEXT("Old presentation session exists"),
		Controller->TryGetPresentationSessionToken(OldSession)))
	{
		OldWidget->SetPresentationController(nullptr);
		Controller->Shutdown();
		ViewModel->Shutdown();
		return false;
	}

	const int64 ResolutionId =
		static_cast<int64>(BattleFixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 100;
	BattleFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeDamageEnvelope(Baseline, ResolutionId, 1));

	if (!TestTrue(TEXT("Scenario reaches debt-only wait"),
		Controller->GetBacklogCountForTesting() == 0
			&& !Controller->IsWaitingForCompletionForTesting()
			&& Controller->GetCompatibilityDebtSecondsForTesting() > 0.0f
			&& Controller->IsCompatibilityDebtServiceActiveForTesting()))
	{
		OldWidget->SetPresentationController(nullptr);
		Controller->Shutdown();
		ViewModel->Shutdown();
		return false;
	}
	TestTrue(TEXT("Debt-only wait keeps interaction locked before replacement"), ViewModel->bInputLocked);

	const int32 OldDetachedCancelCount = OldWidget->DetachedCancelCount;
	Controller->SetWidget(NewWidget);
	OldWidget->SetPresentationController(nullptr);
	NewWidget->SetPresentationController(Controller);

	FPresentationSessionToken NewSession;
	TestTrue(TEXT("Replacement establishes a new presentation session"),
		Controller->TryGetPresentationSessionToken(NewSession));
	TestTrue(TEXT("Widget replacement invalidates the old presentation session"),
		NewSession.IsValid() && NewSession != OldSession);
	TestTrue(TEXT("Old Widget cleans the detached cosmetic from the retired session"),
		OldWidget->DetachedCancelCount > OldDetachedCancelCount);
	TestTrue(TEXT("Debt-only replacement clears compatibility debt"),
		Controller->GetCompatibilityDebtSecondsForTesting() <= KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Debt timer is retired after replacement catch-up"),
		Controller->IsCompatibilityDebtServiceActiveForTesting());
	TestFalse(TEXT("No skippable presentation delay remains after replacement catch-up"),
		Controller->HasSkippablePresentationDelay());
	TestFalse(TEXT("Replacement catch-up does not leave a blocking completion owner"),
		Controller->IsWaitingForCompletionForTesting());
	TestFalse(TEXT("Replacement catch-up releases input"), ViewModel->bInputLocked);
	TestEqual(TEXT("Replacement catch-up restores the normal interaction surface"),
		ViewModel->InteractionState,
		EBattleHUDInteractionState::Idle);

	NewWidget->SetPresentationController(nullptr);
	Controller->Shutdown();
	ViewModel->Shutdown();
	return true;
}

#endif
