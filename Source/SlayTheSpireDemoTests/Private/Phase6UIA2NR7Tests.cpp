#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR7TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR7Test
{
	constexpr int64 TestBattleId = 701;
	constexpr int64 TestResolutionId = 702;
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

	FPresentationRecord MakeDamageRecord(
		int64 Sequence,
		FName SourcePresentationId,
		FName TargetPresentationId,
		int32 IncomingDamage,
		int32 HPBefore,
		int32 HPAfter,
		int32 BlockBefore,
		int32 BlockAfter,
		EDamageKind DamageKind = EDamageKind::Attack)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::Damage;
		Record.Damage.SourcePresentationId = SourcePresentationId;
		Record.Damage.TargetPresentationId = TargetPresentationId;
		Record.Damage.DamageKind = DamageKind;
		Record.Damage.IncomingDamage = IncomingDamage;
		Record.Damage.HPBefore = HPBefore;
		Record.Damage.HPAfter = HPAfter;
		Record.Damage.BlockBefore = BlockBefore;
		Record.Damage.BlockAfter = BlockAfter;
		Record.Damage.BlockedDamage = BlockBefore - BlockAfter;
		Record.Damage.HPDamage = HPBefore - HPAfter;
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
		UPhase6UIA2NR7HUDProbe* Probe = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2NR7CombatantProbe* PlayerPresentation = nullptr;
		UProgressBar* PlayerHPProgress = nullptr;
		UTextBlock* PlayerHPText = nullptr;
		FBlockSurface PlayerBlock;
		UPhase6UIA2NR7CombatantProbe* EnemyPresentation = nullptr;
		UProgressBar* EnemyHPProgress = nullptr;
		UTextBlock* EnemyHPText = nullptr;
		FBlockSurface EnemyBlock;
		UTextBlock* DamageText = nullptr;

		FProbeFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			Probe = NewObject<UPhase6UIA2NR7HUDProbe>(World);
			if (!IsValid(Probe))
			{
				return;
			}

			ViewModel = NewObject<UBattleHUDViewModel>(Probe);
			PlayerPresentation = NewObject<UPhase6UIA2NR7CombatantProbe>(Probe);
			PlayerHPProgress = NewObject<UProgressBar>(Probe);
			PlayerHPText = NewObject<UTextBlock>(Probe);
			PlayerBlock = MakeBlockSurface(Probe);
			EnemyPresentation = NewObject<UPhase6UIA2NR7CombatantProbe>(Probe);
			EnemyHPProgress = NewObject<UProgressBar>(Probe);
			EnemyHPText = NewObject<UTextBlock>(Probe);
			EnemyBlock = MakeBlockSurface(Probe);
			DamageText = NewObject<UTextBlock>(Probe);

			Probe->SetTestWorld(World);
			Probe->ViewModel = ViewModel;
			Probe->ConfigureDamageSurfaces(
				PlayerPresentation,
				PlayerHPProgress,
				PlayerHPText,
				PlayerBlock.Text,
				EnemyPresentation,
				EnemyHPProgress,
				EnemyHPText,
				EnemyBlock.Text,
				DamageText);

			ViewModel->Player.PresentationId = PlayerPresentationId;
			ViewModel->Player.HP = 80;
			ViewModel->Player.MaxHP = 80;
			ViewModel->Player.Block = 5;
			ViewModel->Enemy.PresentationId = EnemyPresentationId;
			ViewModel->Enemy.HP = 100;
			ViewModel->Enemy.MaxHP = 100;
			ViewModel->Enemy.Block = 0;
			SetSurfacesToHistoricalState();
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

		void SetSurfacesToHistoricalState()
		{
			if (!IsValid(ViewModel))
			{
				return;
			}
			PlayerHPText->SetText(FText::Format(
				FText::FromString(TEXT("{0}/{1}")),
				FText::AsNumber(ViewModel->Player.HP),
				FText::AsNumber(ViewModel->Player.MaxHP)));
			PlayerHPProgress->SetPercent(
				static_cast<float>(ViewModel->Player.HP) / ViewModel->Player.MaxHP);
			PlayerBlock.Text->SetText(FText::AsNumber(ViewModel->Player.Block));
			PlayerBlock.Badge->SetVisibility(
				ViewModel->Player.Block > 0
					? ESlateVisibility::SelfHitTestInvisible
					: ESlateVisibility::Collapsed);
			PlayerPresentation->SetRenderOpacity(1.0f);

			EnemyHPText->SetText(FText::Format(
				FText::FromString(TEXT("{0}/{1}")),
				FText::AsNumber(ViewModel->Enemy.HP),
				FText::AsNumber(ViewModel->Enemy.MaxHP)));
			EnemyHPProgress->SetPercent(
				static_cast<float>(ViewModel->Enemy.HP) / ViewModel->Enemy.MaxHP);
			EnemyBlock.Text->SetText(FText::AsNumber(ViewModel->Enemy.Block));
			EnemyBlock.Badge->SetVisibility(
				ViewModel->Enemy.Block > 0
					? ESlateVisibility::SelfHitTestInvisible
					: ESlateVisibility::Collapsed);
			EnemyPresentation->SetRenderOpacity(1.0f);

			DamageText->SetText(FText::GetEmpty());
			DamageText->SetVisibility(ESlateVisibility::Collapsed);
		}

		bool IsValidFixture() const
		{
			return IsValid(World)
				&& IsValid(Probe)
				&& IsValid(ViewModel)
				&& IsValid(PlayerPresentation)
				&& IsValid(PlayerHPProgress)
				&& IsValid(PlayerHPText)
				&& IsValid(PlayerBlock.Badge)
				&& IsValid(PlayerBlock.Text)
				&& IsValid(EnemyPresentation)
				&& IsValid(EnemyHPProgress)
				&& IsValid(EnemyHPText)
				&& IsValid(EnemyBlock.Badge)
				&& IsValid(EnemyBlock.Text)
				&& IsValid(DamageText);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR7EnemyDamageTest,
	"SlayTheSpireDemo.Phase6UIA2N.R7.EnemyTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR7EnemyDamageTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR7Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R7 Enemy Damage fixture."));
		return false;
	}

	const FPresentationRecord RecordA = MakeDamageRecord(
		1, PlayerPresentationId, EnemyPresentationId, 6, 100, 94, 0, 0);
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	const FPresentationPlaybackToken StaleToken = MakeToken(99);
	TestTrue(TEXT("Enemy Damage Begin accepts a valid frozen Record"), Fixture.Probe->PlayPresentationRecord(RecordA, TokenA));
	TestTrue(TEXT("Enemy Damage Begin owns the exact Token"), Fixture.Probe->ActiveLocalToken() == TokenA);
	TestTrue(TEXT("Enemy Damage Begin owns a finish timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Damage text displays frozen IncomingDamage"), Fixture.DamageText->GetText().ToString(), FString(TEXT("6")));
	TestTrue(TEXT("Damage text is visible without hit testing"), Fixture.DamageText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Only the Enemy receives the Legacy opacity feedback"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 0.45f));
	TestTrue(TEXT("Player opacity is unaffected"), FMath::IsNearlyEqual(Fixture.PlayerPresentation->GetRenderOpacity(), 1.0f));
	TestEqual(TEXT("Enemy HP displays frozen HPAfter"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("94/100")));
	TestTrue(TEXT("Enemy HP bar displays frozen HPAfter ratio"), FMath::IsNearlyEqual(Fixture.EnemyHPProgress->GetPercent(), 0.94f));

	Fixture.Probe->InvokeFinishForTesting(StaleToken);
	TestTrue(TEXT("Stale Finish cannot clear Enemy Damage ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Stale Finish leaves Damage text visible"), Fixture.DamageText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Stale Finish leaves target feedback active"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 0.45f));

	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Exact Enemy Damage Finish clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Enemy Damage Finish clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Finish retains frozen HPAfter"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("94/100")));
	TestTrue(TEXT("Exact Finish hides Damage text"), Fixture.DamageText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Exact Finish restores Enemy opacity"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 1.0f));
	Fixture.Probe->InvokeFinishForTesting(TokenA);
	TestFalse(TEXT("Duplicate Finish remains a no-op"), Fixture.Probe->IsLocalPresentationActive());
	FTSTicker::GetCoreTicker().Tick(0.0f);

	const int32 DispatchesAfterFinish = Fixture.Probe->CancelDispatchCount;
	Fixture.ViewModel->Enemy.HP = 94;
	const FPresentationRecord RecordB = MakeDamageRecord(
		2, PlayerPresentationId, EnemyPresentationId, 4, 94, 90, 0, 0);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	TestTrue(TEXT("The next Damage Record starts after exact Finish"), Fixture.Probe->PlayPresentationRecord(RecordB, TokenB));
	TestEqual(TEXT("The next Record is not cancelled by stale ownership"), Fixture.Probe->CancelDispatchCount, DispatchesAfterFinish);
	TestEqual(TEXT("The next Record displays its own frozen HPAfter"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("90/100")));
	Fixture.Probe->InvokeCancelForTesting(TokenB);
	TestEqual(TEXT("Next-record Cancel restores its own frozen HPBefore"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("94/100")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR7PlayerBlockedDamageTest,
	"SlayTheSpireDemo.Phase6UIA2N.R7.PlayerBlockedAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR7PlayerBlockedDamageTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR7Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R7 Player blocked-Damage fixture."));
		return false;
	}

	const FPresentationRecord RecordA = MakeDamageRecord(
		1, EnemyPresentationId, PlayerPresentationId, 5, 80, 80, 5, 0, EDamageKind::Effect);
	const FPresentationPlaybackToken TokenA = MakeToken(1);
	const FPresentationPlaybackToken WrongToken = MakeToken(88);
	TestTrue(TEXT("Player fully-blocked Damage Begin is accepted"), Fixture.Probe->PlayPresentationRecord(RecordA, TokenA));
	TestEqual(TEXT("IncomingDamage remains visible when HP is unchanged"), Fixture.DamageText->GetText().ToString(), FString(TEXT("5")));
	TestEqual(TEXT("Frozen HPAfter remains unchanged"), Fixture.PlayerHPText->GetText().ToString(), FString(TEXT("80/80")));
	TestTrue(TEXT("Frozen HPAfter keeps the HP bar full"), FMath::IsNearlyEqual(Fixture.PlayerHPProgress->GetPercent(), 1.0f));
	TestEqual(TEXT("Frozen BlockAfter is displayed"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("0")));
	TestTrue(TEXT("Zero BlockAfter collapses the complete badge"), Fixture.PlayerBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Player receives the target opacity feedback"), FMath::IsNearlyEqual(Fixture.PlayerPresentation->GetRenderOpacity(), 0.45f));

	Fixture.Probe->InvokeCancelForTesting(WrongToken);
	TestTrue(TEXT("Wrong-token Cancel cannot clear Damage ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Wrong-token Cancel cannot hide Damage text"), Fixture.DamageText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Wrong-token Cancel cannot restore BlockBefore"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("0")));

	Fixture.Probe->InvokeCancelForTesting(TokenA);
	TestFalse(TEXT("Exact Cancel clears Damage ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Cancel clears the timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Exact Cancel restores frozen HPBefore"), Fixture.PlayerHPText->GetText().ToString(), FString(TEXT("80/80")));
	TestEqual(TEXT("Exact Cancel restores frozen BlockBefore"), Fixture.PlayerBlock.Text->GetText().ToString(), FString(TEXT("5")));
	TestTrue(TEXT("Exact Cancel restores the positive Block badge"), Fixture.PlayerBlock.Badge->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("Exact Cancel hides Damage text"), Fixture.DamageText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Exact Cancel restores Player opacity"), FMath::IsNearlyEqual(Fixture.PlayerPresentation->GetRenderOpacity(), 1.0f));

	// Direct visual Cancel leaves the base tracked Token incomplete. A replacement
	// must dispatch TokenA again, proving Cancel never emitted normal completion.
	const int32 DispatchesAfterDirectCancel = Fixture.Probe->CancelDispatchCount;
	const FPresentationRecord RecordB = MakeDamageRecord(
		2, EnemyPresentationId, PlayerPresentationId, 5, 80, 80, 5, 0, EDamageKind::Effect);
	const FPresentationPlaybackToken TokenB = MakeToken(2);
	TestTrue(TEXT("Damage can restart after exact Cancel"), Fixture.Probe->PlayPresentationRecord(RecordB, TokenB));
	TestEqual(TEXT("Replacement re-dispatches uncompleted TokenA"), Fixture.Probe->CancelDispatchCount, DispatchesAfterDirectCancel + 1);
	TestTrue(TEXT("Replacement Cancel carried exact TokenA"), Fixture.Probe->LastCancelDispatchToken == TokenA);
	Fixture.Probe->InvokeFinishForTesting(TokenB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR7LethalDamageTest,
	"SlayTheSpireDemo.Phase6UIA2N.R7.Lethal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR7LethalDamageTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR7Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R7 lethal-Damage fixture."));
		return false;
	}

	Fixture.ViewModel->Enemy.HP = 3;
	Fixture.SetSurfacesToHistoricalState();
	const FPresentationRecord Record = MakeDamageRecord(
		1, PlayerPresentationId, EnemyPresentationId, 10, 3, 0, 0, 0);
	const FPresentationPlaybackToken Token = MakeToken(1);
	TestTrue(TEXT("Lethal overkill Damage Begin is accepted"), Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("Lethal Begin displays frozen HPAfter zero"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("0/100")));
	TestTrue(TEXT("Lethal Begin sets HP progress to zero"), FMath::IsNearlyZero(Fixture.EnemyHPProgress->GetPercent()));
	TestEqual(TEXT("Lethal Begin displays full IncomingDamage, not HPDamage"), Fixture.DamageText->GetText().ToString(), FString(TEXT("10")));
	Fixture.Probe->InvokeFinishForTesting(Token);
	TestEqual(TEXT("Lethal Finish retains frozen HPAfter"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("0/100")));
	TestTrue(TEXT("Lethal Finish hides Damage text"), Fixture.DamageText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Lethal Finish restores target opacity"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR7InvalidBeginTest,
	"SlayTheSpireDemo.Phase6UIA2N.R7.InvalidBegin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR7InvalidBeginTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR7Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R7 invalid-Begin fixture."));
		return false;
	}

	Fixture.DamageText->SetText(FText::FromString(TEXT("damage-sentinel")));
	Fixture.DamageText->SetVisibility(ESlateVisibility::Hidden);
	Fixture.EnemyHPText->SetText(FText::FromString(TEXT("hp-sentinel")));
	Fixture.EnemyHPProgress->SetPercent(0.42f);
	Fixture.EnemyBlock.Text->SetText(FText::FromString(TEXT("block-sentinel")));
	Fixture.EnemyBlock.Badge->SetVisibility(ESlateVisibility::Hidden);
	Fixture.EnemyPresentation->SetRenderOpacity(0.73f);

	const FPresentationRecord InvalidTarget = MakeDamageRecord(
		1, PlayerPresentationId, TEXT("UnknownPresentation"), 6, 100, 94, 0, 0);
	TestFalse(TEXT("Invalid TargetPresentationId returns false"), Fixture.Probe->PlayPresentationRecord(InvalidTarget, MakeToken(1)));

	FPresentationRecord InvalidPayload = MakeDamageRecord(
		2, PlayerPresentationId, EnemyPresentationId, 6, 100, 94, 0, 0);
	InvalidPayload.Damage.HPDamage = 5;
	TestFalse(TEXT("Inconsistent frozen Damage payload returns false"), Fixture.Probe->PlayPresentationRecord(InvalidPayload, MakeToken(2)));

	const FPresentationRecord BeforeMismatch = MakeDamageRecord(
		3, PlayerPresentationId, EnemyPresentationId, 6, 99, 93, 0, 0);
	TestFalse(TEXT("Frozen Before mismatch returns false"), Fixture.Probe->PlayPresentationRecord(BeforeMismatch, MakeToken(3)));

	const FPresentationRecord ConsistentRecord = MakeDamageRecord(
		4, PlayerPresentationId, EnemyPresentationId, 6, 100, 94, 0, 0);
	TestFalse(TEXT("Record/Token mismatch returns false"), Fixture.Probe->PlayPresentationRecord(ConsistentRecord, MakeToken(44)));

	TestFalse(TEXT("Invalid Begins leave zero local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Invalid Begins leave zero local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestTrue(TEXT("Invalid Begins reset local Record type"), Fixture.Probe->ActiveLocalType() == EBattlePresentationRecordType::None);
	TestEqual(TEXT("Invalid Begin leaves Damage text unchanged"), Fixture.DamageText->GetText().ToString(), FString(TEXT("damage-sentinel")));
	TestTrue(TEXT("Invalid Begin leaves Damage visibility unchanged"), Fixture.DamageText->GetVisibility() == ESlateVisibility::Hidden);
	TestEqual(TEXT("Invalid Begin leaves HP text unchanged"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("hp-sentinel")));
	TestTrue(TEXT("Invalid Begin leaves HP progress unchanged"), FMath::IsNearlyEqual(Fixture.EnemyHPProgress->GetPercent(), 0.42f));
	TestEqual(TEXT("Invalid Begin leaves Block text unchanged"), Fixture.EnemyBlock.Text->GetText().ToString(), FString(TEXT("block-sentinel")));
	TestTrue(TEXT("Invalid Begin leaves Block badge unchanged"), Fixture.EnemyBlock.Badge->GetVisibility() == ESlateVisibility::Hidden);
	TestTrue(TEXT("Invalid Begin leaves target opacity unchanged"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 0.73f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR7DestructCleanupTest,
	"SlayTheSpireDemo.Phase6UIA2N.R7.DestructCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR7DestructCleanupTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR7Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R7 destruction fixture."));
		return false;
	}

	const FPresentationRecord Record = MakeDamageRecord(
		1, PlayerPresentationId, EnemyPresentationId, 6, 100, 94, 0, 0);
	const FPresentationPlaybackToken Token = MakeToken(1);
	TestTrue(TEXT("Destruction fixture begins Damage playback"), Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("Damage owns local state before destruction"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Damage owns a timer before destruction"), Fixture.Probe->IsLocalFinishTimerSet());

	Fixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("NativeDestruct clears Damage ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("NativeDestruct clears Damage timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("NativeDestruct does not dispatch visual Cancel"), Fixture.Probe->CancelDispatchCount, 0);
	TestTrue(TEXT("NativeDestruct hides Damage text"), Fixture.DamageText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("NativeDestruct restores target opacity"), FMath::IsNearlyEqual(Fixture.EnemyPresentation->GetRenderOpacity(), 1.0f));
	TestEqual(TEXT("NativeDestruct does not historical-restore HPBefore"), Fixture.EnemyHPText->GetText().ToString(), FString(TEXT("94/100")));
	return true;
}

#endif
