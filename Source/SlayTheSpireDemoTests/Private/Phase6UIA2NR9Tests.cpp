#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR9TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR9Test
{
	constexpr int64 TestBattleId = 901;
	constexpr int64 TestResolutionId = 902;
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

	FBattleHUDStatusView MakeStatusView(
		const TCHAR* StatusId,
		int64 RuntimeSequence,
		int32 Amount,
		const TCHAR* Description)
	{
		FBattleHUDStatusView View;
		View.StatusId = FName(StatusId);
		View.RuntimeSequence = RuntimeSequence;
		View.DisplayName = FText::FromString(StatusId);
		View.Description = FText::FromString(Description);
		View.Amount = Amount;
		View.bUseAtlasIcon = false;
		return View;
	}

	FPresentationRecord MakeStatusRecord(
		int64 Sequence,
		FName TargetPresentationId,
		const FBattleHUDStatusView& Before,
		int32 AmountAfter,
		const TCHAR* DescriptionAfter,
		EStatusChangeReason Reason,
		bool bCreated,
		bool bRemoved,
		int64 RuntimeSequenceOverride = 0)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::StatusChanged;
		Record.StatusChanged.SourcePresentationId = PlayerPresentationId;
		Record.StatusChanged.TargetPresentationId = TargetPresentationId;
		Record.StatusChanged.StatusId = Before.StatusId;
		Record.StatusChanged.RuntimeSequence = RuntimeSequenceOverride > 0
			? RuntimeSequenceOverride
			: Before.RuntimeSequence;
		Record.StatusChanged.AmountBefore = bCreated ? 0 : Before.Amount;
		Record.StatusChanged.AmountAfter = AmountAfter;
		Record.StatusChanged.bCreated = bCreated;
		Record.StatusChanged.bRemoved = bRemoved;
		Record.StatusChanged.Reason = Reason;
		Record.StatusChanged.DisplayName = Before.DisplayName;
		Record.StatusChanged.DescriptionBefore = bCreated
			? FText::GetEmpty()
			: Before.Description;
		Record.StatusChanged.DescriptionAfter = bRemoved
			? FText::GetEmpty()
			: FText::FromString(DescriptionAfter);
		Record.StatusChanged.bUseAtlasIcon = Before.bUseAtlasIcon;
		Record.StatusChanged.UVOffset = Before.UVOffset;
		Record.StatusChanged.UVScale = Before.UVScale;
		Record.StatusChanged.TrimOffset = Before.TrimOffset;
		Record.StatusChanged.TrimScale = Before.TrimScale;
		return Record;
	}

	struct FFixture
	{
		UWorld* World = nullptr;
		UPhase6UIA2NR9HUDProbe* Probe = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UWrapBox* PlayerStatuses = nullptr;
		UWrapBox* EnemyStatuses = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}
			Probe = NewObject<UPhase6UIA2NR9HUDProbe>(World);
			ViewModel = IsValid(Probe) ? NewObject<UBattleHUDViewModel>(Probe) : nullptr;
			PlayerStatuses = IsValid(Probe) ? NewObject<UWrapBox>(Probe) : nullptr;
			EnemyStatuses = IsValid(Probe) ? NewObject<UWrapBox>(Probe) : nullptr;
			if (!IsValid(Probe) || !IsValid(ViewModel)
				|| !IsValid(PlayerStatuses) || !IsValid(EnemyStatuses))
			{
				return;
			}
			Probe->SetTestWorld(World);
			Probe->SetViewModelForTesting(ViewModel);
			Probe->ConfigureStatusSurfaces(PlayerStatuses, EnemyStatuses);
			ViewModel->Player.PresentationId = PlayerPresentationId;
			ViewModel->Enemy.PresentationId = EnemyPresentationId;
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
				&& IsValid(PlayerStatuses) && IsValid(EnemyStatuses);
		}

		UBattleStatusWidget* StatusAt(UWrapBox* Container, int32 Index) const
		{
			return IsValid(Container)
				? Cast<UBattleStatusWidget>(Container->GetChildAt(Index))
				: nullptr;
		}

		void Rebuild()
		{
			Probe->RefreshStatusRowsForTesting();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR9StatusWidgetDTOTest,
	"SlayTheSpireDemo.Phase6UIA2N.R9.StatusWidget.DTOAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR9StatusWidgetDTOTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR9Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R9 fixture."));
		return false;
	}

	const FBattleHUDStatusView View = MakeStatusView(TEXT("Strength"), 11, 3, TEXT("Frozen +3"));
	Fixture.ViewModel->Player.Statuses = {View};
	Fixture.Rebuild();
	TestEqual(TEXT("One formal status row is created"), Fixture.PlayerStatuses->GetChildrenCount(), 1);
	UBattleStatusWidget* Widget = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	TestNotNull(TEXT("Formal row is Native Status Widget"), Widget);
	if (IsValid(Widget))
	{
		TestEqual(TEXT("StatusId getter is frozen identity"), Widget->GetStatusId(), View.StatusId);
		TestEqual(TEXT("RuntimeSequence getter is frozen identity"), Widget->GetRuntimeSequence(), View.RuntimeSequence);
		TestEqual(TEXT("Amount stays in frozen DTO"), Widget->GetStatusView().Amount, 3);
		TestTrue(TEXT("Description stays frozen"), Widget->GetStatusView().Description.EqualTo(View.Description));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR9CreateIncreaseReuseTest,
	"SlayTheSpireDemo.Phase6UIA2N.R9.Lifecycle.CreateIncreaseReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR9CreateIncreaseReuseTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR9Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R9 fixture."));
		return false;
	}

	const FBattleHUDStatusView Seed = MakeStatusView(TEXT("Strength"), 21, 0, TEXT(""));
	const FPresentationRecord CreateRecord = MakeStatusRecord(
		1, PlayerPresentationId, Seed, 2, TEXT("Frozen +2"),
		EStatusChangeReason::Applied, true, false);
	const FPresentationPlaybackToken CreateToken = MakeToken(1);
	TestTrue(TEXT("Create begins through public playback boundary"),
		Fixture.Probe->PlayPresentationRecord(CreateRecord, CreateToken));
	TestEqual(TEXT("Create adds exactly one row"), Fixture.PlayerStatuses->GetChildrenCount(), 1);
	UBattleStatusWidget* CreatedWidget = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	TestNotNull(TEXT("Create owns exact Native Status Widget"), CreatedWidget);
	if (IsValid(CreatedWidget))
	{
		TestEqual(TEXT("Create displays frozen After amount"), CreatedWidget->GetStatusView().Amount, 2);
	}
	TestTrue(TEXT("Create is locally marked transient"), Fixture.Probe->IsActiveStatusCreateForTesting());
	Fixture.Probe->InvokeFinishForTesting(CreateToken);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	FBattleHUDStatusView Historical = MakeStatusView(TEXT("Strength"), 21, 2, TEXT("Frozen +2"));
	Fixture.ViewModel->Player.Statuses = {Historical};
	Fixture.Rebuild();
	UBattleStatusWidget* FormalWidget = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	TestNotNull(TEXT("Reducer refresh formalizes the created status"), FormalWidget);

	const FPresentationRecord IncreaseRecord = MakeStatusRecord(
		2, PlayerPresentationId, Historical, 3, TEXT("Frozen +3"),
		EStatusChangeReason::Increased, false, false);
	const FPresentationPlaybackToken IncreaseToken = MakeToken(2);
	TestTrue(TEXT("Increase begins"), Fixture.Probe->PlayPresentationRecord(IncreaseRecord, IncreaseToken));
	TestEqual(TEXT("Increase does not add a duplicate row"), Fixture.PlayerStatuses->GetChildrenCount(), 1);
	TestTrue(TEXT("Increase reuses the exact formal Widget"),
		Fixture.StatusAt(Fixture.PlayerStatuses, 0) == FormalWidget);
	if (IsValid(FormalWidget))
	{
		TestEqual(TEXT("Reused Widget displays frozen After amount"), FormalWidget->GetStatusView().Amount, 3);
	}
	Fixture.Probe->InvokeFinishForTesting(IncreaseToken);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR9ReductionRemovalCancelTest,
	"SlayTheSpireDemo.Phase6UIA2N.R9.Lifecycle.ReductionRemovalAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR9ReductionRemovalCancelTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR9Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R9 fixture."));
		return false;
	}

	FBattleHUDStatusView Weak = MakeStatusView(TEXT("Weak"), 31, 2, TEXT("Weak 2"));
	FBattleHUDStatusView Vulnerable = MakeStatusView(TEXT("Vulnerable"), 32, 3, TEXT("Vulnerable 3"));
	Fixture.ViewModel->Player.Statuses = {Weak};
	Fixture.ViewModel->Enemy.Statuses = {Vulnerable};
	Fixture.Rebuild();
	UBattleStatusWidget* WeakBefore = Fixture.StatusAt(Fixture.PlayerStatuses, 0);

	const FPresentationRecord ReduceRecord = MakeStatusRecord(
		3, PlayerPresentationId, Weak, 1, TEXT("Weak 1"),
		EStatusChangeReason::TurnEndDecay, false, false);
	const FPresentationPlaybackToken ReduceToken = MakeToken(3);
	TestTrue(TEXT("2 -> 1 reduction begins"), Fixture.Probe->PlayPresentationRecord(ReduceRecord, ReduceToken));
	TestTrue(TEXT("Reduction reuses exact Widget"), Fixture.StatusAt(Fixture.PlayerStatuses, 0) == WeakBefore);
	TestEqual(TEXT("Reduction displays one"), WeakBefore->GetStatusView().Amount, 1);

	Fixture.Probe->InvokeCancelForTesting(ReduceToken);
	TestFalse(TEXT("Exact Cancel clears ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Exact Cancel clears timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Cancel rebuilds Player row from historical VM"), Fixture.PlayerStatuses->GetChildrenCount(), 1);
	TestEqual(TEXT("Cancel rebuilds Enemy row too"), Fixture.EnemyStatuses->GetChildrenCount(), 1);
	TestEqual(TEXT("Player amount restored from historical VM"),
		Fixture.StatusAt(Fixture.PlayerStatuses, 0)->GetStatusView().Amount, 2);
	TestEqual(TEXT("Enemy historical row remains intact"),
		Fixture.StatusAt(Fixture.EnemyStatuses, 0)->GetStatusView().Amount, 3);

	Weak.Amount = 1;
	Weak.Description = FText::FromString(TEXT("Weak 1"));
	Fixture.ViewModel->Player.Statuses = {Weak};
	Fixture.Rebuild();
	UBattleStatusWidget* ExactWeak = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	const FPresentationRecord RemoveRecord = MakeStatusRecord(
		4, PlayerPresentationId, Weak, 0, TEXT(""),
		EStatusChangeReason::TurnEndDecay, false, true);
	const FPresentationPlaybackToken RemoveToken = MakeToken(4);
	TestTrue(TEXT("1 -> 0 removal begins"), Fixture.Probe->PlayPresentationRecord(RemoveRecord, RemoveToken));
	TestTrue(TEXT("Removal targets exact Widget"), Fixture.StatusAt(Fixture.PlayerStatuses, 0) == ExactWeak);
	TestEqual(TEXT("Removal collapses exact Widget"), ExactWeak->GetVisibility(), ESlateVisibility::Collapsed);

	FPresentationPlaybackToken WrongToken = RemoveToken;
	WrongToken.LocalPlaybackGeneration = 99;
	Fixture.Probe->InvokeCancelForTesting(WrongToken);
	TestTrue(TEXT("Wrong-token Cancel is no-op"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Wrong-token Cancel leaves removal visual intact"),
		ExactWeak->GetVisibility(), ESlateVisibility::Collapsed);

	Fixture.Probe->InvokeCancelForTesting(RemoveToken);
	TestFalse(TEXT("Exact removal Cancel clears ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Exact removal Cancel rebuilds historical row"), Fixture.PlayerStatuses->GetChildrenCount(), 1);
	TestEqual(TEXT("Exact removal Cancel restores amount one"),
		Fixture.StatusAt(Fixture.PlayerStatuses, 0)->GetStatusView().Amount, 1);
	TestNotEqual(TEXT("Restored formal row is visible"),
		Fixture.StatusAt(Fixture.PlayerStatuses, 0)->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR9IdentityInvalidTest,
	"SlayTheSpireDemo.Phase6UIA2N.R9.Identity.NewSequenceAndInvalidFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR9IdentityInvalidTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR9Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R9 fixture."));
		return false;
	}

	FBattleHUDStatusView Strength1 = MakeStatusView(TEXT("Strength"), 41, 1, TEXT("Strength 1"));
	Fixture.ViewModel->Player.Statuses = {Strength1};
	Fixture.Rebuild();
	UBattleStatusWidget* Original = Fixture.StatusAt(Fixture.PlayerStatuses, 0);

	FPresentationRecord WrongSequence = MakeStatusRecord(
		5, PlayerPresentationId, Strength1, 2, TEXT("Strength 2"),
		EStatusChangeReason::Increased, false, false, 42);
	TestFalse(TEXT("Same StatusId with wrong RuntimeSequence cannot update old Widget"),
		Fixture.Probe->InvokeBeginDirectForTesting(WrongSequence, MakeToken(5)));
	TestFalse(TEXT("Invalid identity leaves no local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Invalid identity starts no timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestEqual(TEXT("Invalid identity leaves exact formal row unchanged"),
		Original->GetStatusView().Amount, 1);

	FPresentationRecord WrongTarget = MakeStatusRecord(
		6, FName(TEXT("UnknownTarget")), Strength1, 2, TEXT("Strength 2"),
		EStatusChangeReason::Increased, false, false);
	TestFalse(TEXT("Wrong target falls back with zero side effects"),
		Fixture.Probe->InvokeBeginDirectForTesting(WrongTarget, MakeToken(6)));
	TestEqual(TEXT("Wrong target does not duplicate rows"), Fixture.PlayerStatuses->GetChildrenCount(), 1);

	FPresentationRecord BadReason = MakeStatusRecord(
		7, PlayerPresentationId, Strength1, 2, TEXT("Strength 2"),
		EStatusChangeReason::Applied, false, false);
	TestFalse(TEXT("Invalid reason/flags are rejected"),
		Fixture.Probe->InvokeBeginDirectForTesting(BadReason, MakeToken(7)));

	// Simulate completed removal + reducer catch-up, then a later re-application
	// of the same StatusId with a new RuntimeSequence. Identity must not retain 41.
	Fixture.ViewModel->Player.Statuses.Reset();
	Fixture.Rebuild();
	FBattleHUDStatusView NewStrength = MakeStatusView(TEXT("Strength"), 42, 0, TEXT(""));
	const FPresentationRecord Recreate = MakeStatusRecord(
		8, PlayerPresentationId, NewStrength, 2, TEXT("Strength new"),
		EStatusChangeReason::Applied, true, false);
	const FPresentationPlaybackToken RecreateToken = MakeToken(8);
	TestTrue(TEXT("Same StatusId may create a later new RuntimeSequence"),
		Fixture.Probe->PlayPresentationRecord(Recreate, RecreateToken));
	UBattleStatusWidget* RecreatedWidget = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	TestNotNull(TEXT("New sequence creates one exact Widget"), RecreatedWidget);
	if (IsValid(RecreatedWidget))
	{
		TestEqual(TEXT("New sequence identity is preserved"), RecreatedWidget->GetRuntimeSequence(), int64(42));
	}
	Fixture.Probe->InvokeFinishForTesting(RecreateToken);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR9StaleAndDestructTest,
	"SlayTheSpireDemo.Phase6UIA2N.R9.Token.StaleAndDestructCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR9StaleAndDestructTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR9Test;
	FFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create R9 fixture."));
		return false;
	}

	FBattleHUDStatusView Status = MakeStatusView(TEXT("Strength"), 51, 2, TEXT("Strength 2"));
	Fixture.ViewModel->Player.Statuses = {Status};
	Fixture.Rebuild();
	UBattleStatusWidget* Widget = Fixture.StatusAt(Fixture.PlayerStatuses, 0);
	const FPresentationRecord Increase = MakeStatusRecord(
		9, PlayerPresentationId, Status, 3, TEXT("Strength 3"),
		EStatusChangeReason::Increased, false, false);
	const FPresentationPlaybackToken Token = MakeToken(9);
	TestTrue(TEXT("Increase begins"), Fixture.Probe->PlayPresentationRecord(Increase, Token));

	FPresentationPlaybackToken Stale = Token;
	Stale.PresentationSequence = 8;
	Fixture.Probe->InvokeFinishForTesting(Stale);
	TestTrue(TEXT("Stale Finish cannot clear active presentation"), Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Stale Finish leaves After display"), Widget->GetStatusView().Amount, 3);

	FPresentationPlaybackToken WrongCancel = Token;
	WrongCancel.LocalPlaybackGeneration = 2;
	Fixture.Probe->InvokeCancelForTesting(WrongCancel);
	TestTrue(TEXT("Wrong-token Cancel remains no-op"), Fixture.Probe->IsLocalPresentationActive());

	Fixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("Destruct clears local ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Destruct clears local timer"), Fixture.Probe->IsLocalFinishTimerSet());
	TestNull(TEXT("Destruct clears local Status reference"), Fixture.Probe->ActiveStatusForTesting());
	TestEqual(TEXT("Destruct does not historical-restore formal Status"), Widget->GetStatusView().Amount, 3);
	return true;
}

#endif
