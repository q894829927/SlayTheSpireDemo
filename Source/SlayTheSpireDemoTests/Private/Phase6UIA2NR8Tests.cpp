#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR8TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR8Test
{
	constexpr int64 TestBattleId = 801;
	constexpr int64 TestResolutionId = 802;
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

	FPresentationCardSnapshot MakeSnapshot(
		int32 RuntimeId,
		const TCHAR* CardId,
		int32 Cost = 1,
		ECardType CardType = ECardType::Attack,
		ECardTargetType TargetType = ECardTargetType::Enemy)
	{
		FPresentationCardSnapshot Snapshot;
		Snapshot.RuntimeId = RuntimeId;
		Snapshot.CardId = FName(CardId);
		Snapshot.DisplayName = FText::FromString(CardId);
		Snapshot.Cost = Cost;
		Snapshot.CardType = CardType;
		Snapshot.TargetType = TargetType;
		Snapshot.Description = FText::FromString(FString::Printf(TEXT("Frozen %s"), CardId));
		return Snapshot;
	}

	FBattleHUDCardView MakeFormalView(const FPresentationCardSnapshot& Snapshot)
	{
		FBattleHUDCardView View;
		View.RuntimeId = Snapshot.RuntimeId;
		View.CardId = Snapshot.CardId;
		View.DisplayName = Snapshot.DisplayName;
		View.Cost = Snapshot.Cost;
		View.CardType = Snapshot.CardType;
		View.TargetType = Snapshot.TargetType;
		View.Description = Snapshot.Description;
		View.CardArt = Snapshot.CardArt;
		View.bGameplayPlayable = true;
		return View;
	}

	FPresentationRecord MakeCardPlayedRecord(
		int64 Sequence,
		const FPresentationCardSnapshot& Snapshot,
		int32 HandIndex,
		int32 EnergyBefore = 3)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::CardPlayed;
		Record.CardPlayed.Card = Snapshot;
		Record.CardPlayed.SourcePresentationId = PlayerPresentationId;
		Record.CardPlayed.TargetPresentationId =
			Snapshot.TargetType == ECardTargetType::Enemy
				? EnemyPresentationId
				: NAME_None;
		Record.CardPlayed.HandIndexBefore = HandIndex;
		Record.CardPlayed.PlayAreaIndexAfter = 0;
		Record.CardPlayed.EnergyBefore = EnergyBefore;
		Record.CardPlayed.EnergyAfter = EnergyBefore - Snapshot.Cost;
		Record.CardPlayed.CostPaid = Snapshot.Cost;
		return Record;
	}

	FPresentationRecord MakeZoneRecord(
		int64 Sequence,
		const FPresentationCardSnapshot& Snapshot,
		ECardZone FromZone,
		ECardZone ToZone,
		int32 FromIndex,
		int32 ToIndex)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.CardZoneChanged.Card = Snapshot;
		Record.CardZoneChanged.FromZone = FromZone;
		Record.CardZoneChanged.ToZone = ToZone;
		Record.CardZoneChanged.FromIndex = FromIndex;
		Record.CardZoneChanged.ToIndex = ToIndex;
		return Record;
	}

	struct FProbeFixture
	{
		UWorld* World = nullptr;
		UPhase6UIA2NR8HUDProbe* Probe = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UHorizontalBox* Hand = nullptr;
		UOverlay* PlayArea = nullptr;
		UTextBlock* DrawCount = nullptr;
		UTextBlock* DiscardCount = nullptr;
		UTextBlock* Energy = nullptr;

		FProbeFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			Probe = NewObject<UPhase6UIA2NR8HUDProbe>(World);
			ViewModel = NewObject<UBattleHUDViewModel>(Probe);
			Hand = NewObject<UHorizontalBox>(Probe);
			PlayArea = NewObject<UOverlay>(Probe);
			DrawCount = NewObject<UTextBlock>(Probe);
			DiscardCount = NewObject<UTextBlock>(Probe);
			Energy = NewObject<UTextBlock>(Probe);
			if (!IsValid(Probe) || !IsValid(ViewModel) || !IsValid(Hand)
				|| !IsValid(PlayArea) || !IsValid(DrawCount)
				|| !IsValid(DiscardCount) || !IsValid(Energy))
			{
				return;
			}

			Probe->SetTestWorld(World);
			Probe->SetViewModelForTesting(ViewModel);
			Probe->ConfigureCardSurfaces(Hand, PlayArea, DrawCount, DiscardCount, Energy);
			ViewModel->Player.PresentationId = PlayerPresentationId;
			ViewModel->Enemy.PresentationId = EnemyPresentationId;
			ViewModel->Energy = 3;
			ViewModel->MaxEnergy = 3;
			ViewModel->DrawCount = 0;
			ViewModel->DiscardCount = 0;
			ViewModel->ExhaustCount = 0;
			SyncCountText();
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
			return IsValid(World) && IsValid(Probe) && IsValid(ViewModel)
				&& IsValid(Hand) && IsValid(PlayArea) && IsValid(DrawCount)
				&& IsValid(DiscardCount) && IsValid(Energy);
		}

		void SyncCountText()
		{
			if (IsValid(DrawCount) && IsValid(DiscardCount) && IsValid(Energy)
				&& IsValid(ViewModel))
			{
				DrawCount->SetText(FText::AsNumber(ViewModel->DrawCount));
				DiscardCount->SetText(FText::AsNumber(ViewModel->DiscardCount));
				Energy->SetText(FText::Format(
					FText::FromString(TEXT("{0}/{1}")),
					FText::AsNumber(ViewModel->Energy),
					FText::AsNumber(ViewModel->MaxEnergy)));
			}
		}

		UPhase6UIA2NR8CardProbe* AddFormalCard(
			const FPresentationCardSnapshot& Snapshot,
			ESlateVisibility Visibility = ESlateVisibility::Visible)
		{
			if (!IsValidFixture())
			{
				return nullptr;
			}
			UPhase6UIA2NR8CardProbe* Card = NewObject<UPhase6UIA2NR8CardProbe>(Probe);
			const FBattleHUDCardView View = MakeFormalView(Snapshot);
			Card->SetCardView(View);
			Card->SetVisibility(Visibility);
			ViewModel->HandCards.Add(View);
			Hand->AddChild(Card);
			return Card;
		}

		void ApplyCardPlayedSnapshot(const FPresentationCardSnapshot& Snapshot)
		{
			ViewModel->HandCards.RemoveAll(
				[RuntimeId = Snapshot.RuntimeId](const FBattleHUDCardView& View)
				{
					return View.RuntimeId == RuntimeId;
				});
			ViewModel->Energy -= Snapshot.Cost;
			Hand->ClearChildren();
			SyncCountText();
		}

		void RebuildFormalHandWidgetsForTesting()
		{
			Hand->ClearChildren();
			for (const FBattleHUDCardView& View : ViewModel->HandCards)
			{
				UPhase6UIA2NR8CardProbe* Card = NewObject<UPhase6UIA2NR8CardProbe>(Probe);
				Card->SetCardView(View);
				Hand->AddChild(Card);
			}
		}
	};

	bool BeginAndFinishCardPlayed(
		FAutomationTestBase& Test,
		FProbeFixture& Fixture,
		const FPresentationCardSnapshot& Snapshot,
		int64 Sequence = 1)
	{
		Fixture.AddFormalCard(Snapshot);
		const FPresentationRecord Played = MakeCardPlayedRecord(Sequence, Snapshot, 0);
		const FPresentationPlaybackToken Token = MakeToken(Sequence);
		if (!Test.TestTrue(TEXT("CardPlayed setup Begin"), Fixture.Probe->PlayPresentationRecord(Played, Token)))
		{
			return false;
		}
		Fixture.Probe->InvokeFinishForTesting(Token);
		FTSTicker::GetCoreTicker().Tick(0.0f);
		Fixture.ApplyCardPlayedSnapshot(Snapshot);
		return IsValid(Fixture.Probe->PlayedCardForTesting());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8CardPlayedLifecycleTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.CardPlayed.ExactIdentityFinishAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8CardPlayedLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR8Test;
	FProbeFixture Fixture;
	if (!Fixture.IsValidFixture())
	{
		AddError(TEXT("Failed to create the R8 CardPlayed fixture."));
		return false;
	}

	const FPresentationCardSnapshot Snapshot = MakeSnapshot(8101, TEXT("R8Strike"));
	UPhase6UIA2NR8CardProbe* FormalCard = Fixture.AddFormalCard(
		Snapshot,
		ESlateVisibility::SelfHitTestInvisible);
	const FPresentationRecord Record = MakeCardPlayedRecord(1, Snapshot, 0);
	const FPresentationPlaybackToken Token = MakeToken(1);
	const FPresentationPlaybackToken StaleToken = MakeToken(99);
	TestTrue(TEXT("CardPlayed accepts exact frozen identity and HandIndex"),
		Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("CardPlayed hides the exact historical Hand Widget"),
		FormalCard->GetVisibility() == ESlateVisibility::Hidden);
	UBattleCardWidget* PlayedCard = Fixture.Probe->PlayedCardForTesting();
	TestNotNull(TEXT("CardPlayed creates one PlayArea transient"), PlayedCard);
	if (IsValid(PlayedCard))
	{
		TestTrue(TEXT("Played transient is owned by OV_PlayArea"),
			PlayedCard->GetParent() == Fixture.PlayArea);
		TestEqual(TEXT("Played transient preserves RuntimeId"), PlayedCard->GetRuntimeId(), Snapshot.RuntimeId);
		TestEqual(TEXT("Played transient preserves CardId"), PlayedCard->GetCardId(), Snapshot.CardId);
		TestFalse(TEXT("Played transient is never Gameplay-playable"), PlayedCard->GetCardView().bGameplayPlayable);
		TestFalse(TEXT("Played transient binds no HUD request delegate"), PlayedCard->OnBattleCardRequested.IsBound());
		TestTrue(TEXT("Played transient cannot receive hit tests"),
			PlayedCard->GetVisibility() == ESlateVisibility::HitTestInvisible);
	}
	TestEqual(TEXT("CardPlayed does not synthesize EnergyChanged state"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("CardPlayed does not mutate the Energy surface"), Fixture.Energy->GetText().ToString(), FString(TEXT("3/3")));

	Fixture.Probe->InvokeFinishForTesting(StaleToken);
	TestTrue(TEXT("Stale CardPlayed Finish is a no-op"), Fixture.Probe->IsLocalPresentationActive());
	Fixture.Probe->InvokeFinishForTesting(Token);
	TestFalse(TEXT("Exact CardPlayed Finish clears ownership"), Fixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Exact CardPlayed Finish retains the PlayArea transient"),
		Fixture.Probe->PlayedCardForTesting() == PlayedCard);
	Fixture.Probe->InvokeFinishForTesting(Token);
	TestTrue(TEXT("Duplicate CardPlayed Finish cannot retire the Played card"),
		Fixture.Probe->PlayedCardForTesting() == PlayedCard);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	Fixture.ApplyCardPlayedSnapshot(Snapshot);
	const FPresentationRecord Discard = MakeZoneRecord(
		2, Snapshot, ECardZone::PlayArea, ECardZone::DiscardPile, 0, 0);
	const FPresentationPlaybackToken DiscardToken = MakeToken(2);
	TestTrue(TEXT("Next PlayArea->Discard Record accepts the same frozen Played card"),
		Fixture.Probe->PlayPresentationRecord(Discard, DiscardToken));
	Fixture.Probe->InvokeFinishForTesting(DiscardToken);
	TestNull(TEXT("PlayArea->Discard Finish retires the transient"),
		Fixture.Probe->PlayedCardForTesting());
	TestEqual(TEXT("PlayArea->Discard does not prematurely modify DiscardCount"),
		Fixture.ViewModel->DiscardCount, 0);

	FProbeFixture CancelFixture;
	const FPresentationCardSnapshot CancelSnapshot = MakeSnapshot(8102, TEXT("R8Defend"), 1, ECardType::Skill, ECardTargetType::Self);
	UPhase6UIA2NR8CardProbe* CancelFormal = CancelFixture.AddFormalCard(
		CancelSnapshot,
		ESlateVisibility::SelfHitTestInvisible);
	const FPresentationRecord CancelRecord = MakeCardPlayedRecord(3, CancelSnapshot, 0);
	const FPresentationPlaybackToken CancelToken = MakeToken(3);
	TestTrue(TEXT("Second CardPlayed Begin succeeds"),
		CancelFixture.Probe->PlayPresentationRecord(CancelRecord, CancelToken));
	CancelFixture.Probe->InvokeCancelForTesting(MakeToken(88));
	TestTrue(TEXT("Wrong-token CardPlayed Cancel is a no-op"), CancelFixture.Probe->IsLocalPresentationActive());
	CancelFixture.Probe->InvokeCancelForTesting(CancelToken);
	TestFalse(TEXT("Exact CardPlayed Cancel clears ownership"), CancelFixture.Probe->IsLocalPresentationActive());
	TestTrue(TEXT("Exact CardPlayed Cancel restores exact historical visibility"),
		CancelFormal->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Exact CardPlayed Cancel removes its PlayArea transient"),
		CancelFixture.PlayArea->GetChildrenCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8CardIdentityRejectionTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.CardPlayed.InvalidIdentityZeroSideEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8CardIdentityRejectionTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR8Test;
	FProbeFixture Fixture;
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(8201, TEXT("ExactCard"));
	UPhase6UIA2NR8CardProbe* FormalCard = Fixture.AddFormalCard(Snapshot);
	FPresentationRecord WrongCardId = MakeCardPlayedRecord(1, Snapshot, 0);
	WrongCardId.CardPlayed.Card.CardId = TEXT("WrongCard");
	TestFalse(TEXT("Wrong CardId returns false"),
		Fixture.Probe->PlayPresentationRecord(WrongCardId, MakeToken(1)));
	TestTrue(TEXT("Wrong CardId leaves exact formal card visible"),
		FormalCard->GetVisibility() == ESlateVisibility::Visible);
	TestEqual(TEXT("Wrong CardId creates no transient"), Fixture.PlayArea->GetChildrenCount(), 0);
	TestFalse(TEXT("Wrong CardId owns no local presentation"), Fixture.Probe->IsLocalPresentationActive());

	Fixture.AddFormalCard(Snapshot);
	FPresentationRecord DuplicateRuntime = MakeCardPlayedRecord(2, Snapshot, 0);
	TestFalse(TEXT("Duplicate RuntimeId returns false"),
		Fixture.Probe->PlayPresentationRecord(DuplicateRuntime, MakeToken(2)));
	TestEqual(TEXT("Duplicate RuntimeId leaves both formal Widgets untouched"),
		Fixture.Hand->GetChildrenCount(), 2);
	TestEqual(TEXT("Duplicate RuntimeId creates no PlayArea visual"),
		Fixture.PlayArea->GetChildrenCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8HandDiscardTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.Zone.HandToDiscardFinishCancelAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8HandDiscardTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR8Test;
	FProbeFixture Fixture;
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(8301, TEXT("DiscardMe"), 0);
	UPhase6UIA2NR8CardProbe* FormalCard = Fixture.AddFormalCard(
		Snapshot,
		ESlateVisibility::SelfHitTestInvisible);
	const FPresentationRecord Record = MakeZoneRecord(
		1, Snapshot, ECardZone::Hand, ECardZone::DiscardPile, 0, 0);
	const FPresentationPlaybackToken Token = MakeToken(1);
	TestTrue(TEXT("Hand->Discard Begin accepts exact card/index"),
		Fixture.Probe->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("Hand->Discard Begin collapses exact historical Widget"),
		FormalCard->GetVisibility() == ESlateVisibility::Collapsed);
	Fixture.Probe->InvokeCancelForTesting(Token);
	TestTrue(TEXT("Hand->Discard Cancel restores exact historical visibility"),
		FormalCard->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	TestTrue(TEXT("Hand->Discard may start again after exact Cancel"),
		Fixture.Probe->PlayPresentationRecord(Record, Token));
	Fixture.Probe->InvokeFinishForTesting(Token);
	TestTrue(TEXT("Hand->Discard Finish preserves committed hidden After"),
		FormalCard->GetVisibility() == ESlateVisibility::Collapsed);
	TestEqual(TEXT("Hand->Discard Finish does not proactively RefreshHand"),
		Fixture.Hand->GetChildrenCount(), 1);
	TestEqual(TEXT("Hand->Discard Begin/Finish does not mutate pile count early"),
		Fixture.ViewModel->DiscardCount, 0);

	FProbeFixture InvalidFixture;
	UPhase6UIA2NR8CardProbe* InvalidFormal = InvalidFixture.AddFormalCard(Snapshot);
	const FPresentationRecord WrongIndex = MakeZoneRecord(
		2, Snapshot, ECardZone::Hand, ECardZone::DiscardPile, 1, 0);
	TestFalse(TEXT("Invalid Hand index returns false"),
		InvalidFixture.Probe->PlayPresentationRecord(WrongIndex, MakeToken(2)));
	TestTrue(TEXT("Invalid Hand index has zero visibility side effects"),
		InvalidFormal->GetVisibility() == ESlateVisibility::Visible);
	const FPresentationRecord Unsupported = MakeZoneRecord(
		3, Snapshot, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0);
	TestFalse(TEXT("Unsupported zone pair returns false"),
		InvalidFixture.Probe->PlayPresentationRecord(Unsupported, MakeToken(3)));
	TestTrue(TEXT("Unsupported pair has zero local side effects"),
		InvalidFormal->GetVisibility() == ESlateVisibility::Visible
			&& InvalidFixture.PlayArea->GetChildrenCount() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8DrawSequentialTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.Zone.DrawToHandSequentialPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8DrawSequentialTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR8Test;
	FProbeFixture Fixture;
	Fixture.ViewModel->DrawCount = 2;
	Fixture.SyncCountText();
	const FPresentationCardSnapshot First = MakeSnapshot(8401, TEXT("FirstDraw"));
	const FPresentationCardSnapshot Second = MakeSnapshot(8402, TEXT("SecondDraw"), 0, ECardType::Skill, ECardTargetType::Self);
	const FPresentationRecord FirstRecord = MakeZoneRecord(
		1, First, ECardZone::DrawPile, ECardZone::Hand, 1, 0);
	const FPresentationRecord SecondRecord = MakeZoneRecord(
		2, Second, ECardZone::DrawPile, ECardZone::Hand, 0, 1);
	const FPresentationPlaybackToken FirstToken = MakeToken(1);
	const FPresentationPlaybackToken SecondToken = MakeToken(2);
	TestTrue(TEXT("First Draw Record starts"),
		Fixture.Probe->PlayPresentationRecord(FirstRecord, FirstToken));
	TestEqual(TEXT("First Draw owns exactly one Hand transient"),
		Fixture.Hand->GetChildrenCount(), 1);
	UBattleCardWidget* FirstTransient = Fixture.Probe->DrawnCardForTesting();
	TestNotNull(TEXT("First Draw owns its presentation-only card"), FirstTransient);
	if (IsValid(FirstTransient))
	{
		TestEqual(TEXT("First transient is exact frozen identity"), FirstTransient->GetRuntimeId(), First.RuntimeId);
		TestFalse(TEXT("Draw transient is not Gameplay-playable"), FirstTransient->GetCardView().bGameplayPlayable);
		TestFalse(TEXT("Draw transient binds no request delegate"), FirstTransient->OnBattleCardRequested.IsBound());
		TestTrue(TEXT("Draw transient is HitTestInvisible"),
			FirstTransient->GetVisibility() == ESlateVisibility::HitTestInvisible);
	}
	TestEqual(TEXT("First Draw updates only its frozen DrawPile After"),
		Fixture.DrawCount->GetText().ToString(), FString(TEXT("1")));
	TestFalse(TEXT("Second Draw cannot start while first exact Token owns playback"),
		Fixture.Probe->InvokeBeginDirectForTesting(SecondRecord, SecondToken));
	TestEqual(TEXT("Rejected second Begin cannot add a future card"),
		Fixture.Hand->GetChildrenCount(), 1);
	TestTrue(TEXT("First exact Token remains owner"), Fixture.Probe->ActiveLocalToken() == FirstToken);

	Fixture.Probe->InvokeNativeTickForTesting(0.25f);
	TestTrue(TEXT("Native Draw movement initializes from DrawPile visual anchor"),
		Fixture.Probe->DrawAnimationInitializedForTesting());
	if (IsValid(FirstTransient))
	{
		TestTrue(TEXT("Draw movement fades the current card in"),
			FirstTransient->GetRenderOpacity() > 0.0f
				&& FirstTransient->GetRenderOpacity() < 1.0f);
	}
	Fixture.Probe->InvokeFinishForTesting(MakeToken(99));
	TestTrue(TEXT("Stale Draw Finish cannot release first ownership"),
		Fixture.Probe->IsLocalPresentationActive());
	Fixture.Probe->InvokeCancelForTesting(MakeToken(88));
	TestTrue(TEXT("Wrong-token Draw Cancel cannot remove first transient"),
		Fixture.Hand->GetChildrenCount() == 1 && Fixture.Probe->IsLocalPresentationActive());
	Fixture.Probe->InvokeFinishForTesting(FirstToken);
	TestFalse(TEXT("Exact first Draw Finish releases local ownership"),
		Fixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("Exact first Finish retains only its one visual until snapshot refresh"),
		Fixture.Hand->GetChildrenCount(), 1);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	Fixture.ViewModel->DrawCount = 1;
	Fixture.ViewModel->HandCards.Add(MakeFormalView(First));
	Fixture.RebuildFormalHandWidgetsForTesting();
	Fixture.SyncCountText();
	TestEqual(TEXT("Per-Record refresh formalizes only the first drawn card"),
		Fixture.Hand->GetChildrenCount(), 1);
	TestEqual(TEXT("Future second card is still absent before its Begin"),
		CastChecked<UBattleCardWidget>(Fixture.Hand->GetChildAt(0))->GetRuntimeId(), First.RuntimeId);

	TestTrue(TEXT("Second Draw starts only after first Finish and snapshot application"),
		Fixture.Probe->PlayPresentationRecord(SecondRecord, SecondToken));
	TestEqual(TEXT("Second Draw adds exactly one current transient"),
		Fixture.Hand->GetChildrenCount(), 2);
	TestEqual(TEXT("Second transient targets exact append ToIndex"),
		CastChecked<UBattleCardWidget>(Fixture.Hand->GetChildAt(1))->GetRuntimeId(), Second.RuntimeId);
	Fixture.Probe->InvokeCancelForTesting(SecondToken);
	TestEqual(TEXT("Second Draw Cancel removes only its transient"),
		Fixture.Hand->GetChildrenCount(), 1);
	TestEqual(TEXT("Second Draw Cancel restores DrawPile Before"),
		Fixture.DrawCount->GetText().ToString(), FString(TEXT("1")));

	FProbeFixture DuplicateFixture;
	DuplicateFixture.ViewModel->DrawCount = 1;
	DuplicateFixture.AddFormalCard(Second);
	DuplicateFixture.SyncCountText();
	const FPresentationRecord DuplicateDraw = MakeZoneRecord(
		3, Second, ECardZone::DrawPile, ECardZone::Hand, 0, 1);
	TestFalse(TEXT("Draw rejects a duplicate RuntimeId already in formal Hand"),
		DuplicateFixture.Probe->PlayPresentationRecord(DuplicateDraw, MakeToken(3)));
	TestEqual(TEXT("Rejected duplicate Draw creates no transient or count mutation"),
		DuplicateFixture.Hand->GetChildrenCount(), 1);
	TestEqual(TEXT("Rejected duplicate Draw preserves DrawPile Before"),
		DuplicateFixture.DrawCount->GetText().ToString(), FString(TEXT("1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8PlayAreaDestinationsAndCleanupTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.Zone.PlayAreaDestinationsAndDestruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8PlayAreaDestinationsAndCleanupTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2NR8Test;
	const ECardZone Destinations[] =
	{
		ECardZone::DiscardPile,
		ECardZone::ExhaustPile,
		ECardZone::RemovedPile
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Destinations); ++Index)
	{
		FProbeFixture Fixture;
		const FPresentationCardSnapshot Snapshot = MakeSnapshot(
			8500 + Index,
			Index == 0 ? TEXT("ToDiscard") : (Index == 1 ? TEXT("ToExhaust") : TEXT("ToRemoved")));
		if (!BeginAndFinishCardPlayed(*this, Fixture, Snapshot))
		{
			AddError(TEXT("Failed to establish the PlayedCard transient."));
			continue;
		}
		const int32 ToIndex = Destinations[Index] == ECardZone::RemovedPile ? 4 : 0;
		const FPresentationRecord Zone = MakeZoneRecord(
			2, Snapshot, ECardZone::PlayArea, Destinations[Index], 0, ToIndex);
		const FPresentationPlaybackToken Token = MakeToken(2);
		TestTrue(*FString::Printf(TEXT("PlayArea destination %d accepts"), Index),
			Fixture.Probe->PlayPresentationRecord(Zone, Token));
		TestNotNull(TEXT("PlayArea transient remains until exact Finish"),
			Fixture.Probe->PlayedCardForTesting());
		Fixture.Probe->InvokeFinishForTesting(Token);
		TestNull(TEXT("Exact destination Finish retires PlayedCard transient"),
			Fixture.Probe->PlayedCardForTesting());
		TestEqual(TEXT("Destination Begin/Finish does not eagerly mutate DiscardCount"),
			Fixture.ViewModel->DiscardCount, 0);
		TestEqual(TEXT("Destination Begin/Finish does not eagerly mutate ExhaustCount"),
			Fixture.ViewModel->ExhaustCount, 0);
	}

	FProbeFixture DestructFixture;
	const FPresentationCardSnapshot DestructSnapshot = MakeSnapshot(8590, TEXT("DestructPlayed"));
	UPhase6UIA2NR8CardProbe* HistoricalCard = DestructFixture.AddFormalCard(DestructSnapshot);
	const FPresentationRecord Played = MakeCardPlayedRecord(9, DestructSnapshot, 0);
	TestTrue(TEXT("Destruct fixture starts CardPlayed"),
		DestructFixture.Probe->PlayPresentationRecord(Played, MakeToken(9)));
	DestructFixture.Probe->InvokeNativeDestructForTesting();
	TestFalse(TEXT("NativeDestruct clears local ownership"),
		DestructFixture.Probe->IsLocalPresentationActive());
	TestEqual(TEXT("NativeDestruct removes PlayedCard transient"),
		DestructFixture.PlayArea->GetChildrenCount(), 0);
	TestTrue(TEXT("NativeDestruct does not historical-restore the formal card"),
		HistoricalCard->GetVisibility() == ESlateVisibility::Hidden);

	FProbeFixture DrawDestructFixture;
	DrawDestructFixture.ViewModel->DrawCount = 1;
	DrawDestructFixture.SyncCountText();
	const FPresentationCardSnapshot DrawSnapshot = MakeSnapshot(8591, TEXT("DestructDraw"));
	const FPresentationRecord DrawRecord = MakeZoneRecord(
		10, DrawSnapshot, ECardZone::DrawPile, ECardZone::Hand, 0, 0);
	TestTrue(TEXT("Draw Destruct fixture starts one transient"),
		DrawDestructFixture.Probe->PlayPresentationRecord(DrawRecord, MakeToken(10)));
	TestEqual(TEXT("Draw Destruct precondition has one transient"),
		DrawDestructFixture.Hand->GetChildrenCount(), 1);
	DrawDestructFixture.Probe->InvokeNativeDestructForTesting();
	TestEqual(TEXT("NativeDestruct removes active Draw transient"),
		DrawDestructFixture.Hand->GetChildrenCount(), 0);
	TestEqual(TEXT("NativeDestruct performs no historical Draw-count restore"),
		DrawDestructFixture.DrawCount->GetText().ToString(), FString(TEXT("0")));
	return true;
}

#endif
