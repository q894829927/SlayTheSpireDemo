#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectHandCardToDrawPileTopEffect.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDViewModel.h"
#include "UI/BattleHUDWidget.h"
#include "Engine/World.h"
#include "CardSelectionPresentationTestTypes.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "UI/BattleCardWidget.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

namespace CardSelectionInputRoutingTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("PlayerHero");
			Enemy->PresentationId = TEXT("EnemyPrimary");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreateCard(const TCHAR* CardId, ECardTargetType TargetType = ECardTargetType::None)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("Selection routing test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = TargetType == ECardTargetType::Enemy ? ECardType::Attack : ECardType::Skill;
			Card->TargetType = TargetType;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectionCard(const TCHAR* CardId)
		{
			UCardData* Card = CreateCard(CardId);
			USelectHandCardToDrawPileTopEffect* Effect = NewObject<USelectHandCardToDrawPileTopEffect>(Card);
			Effect->BaseSelectionCount = 1;
			Effect->UpgradedSelectionCount = 1;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
		{
			if (!IsValid(Battle) || Definitions.IsEmpty())
			{
				return false;
			}
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions)
			{
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetDeckRuntimeForTesting());
		}

		UCardInstance* FindHandCard(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck))
			{
				return nullptr;
			}
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId)
				{
					return Card.Get();
				}
			}
			return nullptr;
		}
	};
}

using namespace CardSelectionInputRoutingTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPendingBoundaryBlocksOrdinaryCardPlayTest,
	"SlayTheSpireDemo.CardSelection.Presentation.Input.PendingBoundaryBlocksOrdinaryCardPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPendingBoundaryBlocksOrdinaryCardPlayTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectionCard = Fixture.CreateSelectionCard(TEXT("RoutingSelection"));
	UCardData* CandidateCard = Fixture.CreateCard(TEXT("RoutingCandidate"), ECardTargetType::Enemy);
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectionCard, CandidateCard })))
	{
		return false;
	}

	UCardInstance* PlayedCard = Fixture.FindHandCard(TEXT("RoutingSelection"));
	UCardInstance* Candidate = Fixture.FindHandCard(TEXT("RoutingCandidate"));
	if (!TestNotNull(TEXT("Selection card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Candidate card exists"), Candidate))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UBattleHUDWidget* HUD = NewObject<UBattleHUDWidget>(Fixture.World);
	if (!TestNotNull(TEXT("ViewModel created"), ViewModel)
		|| !TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, false))
		|| !TestNotNull(TEXT("HUD created"), HUD))
	{
		return false;
	}
	HUD->SetViewModel(ViewModel);

	const int64 DisplayedBattleIdBefore = ViewModel->BattleId;
	const int64 DisplayedRevisionBefore = ViewModel->StateRevision;
	const FText FeedbackBefore = ViewModel->LastFeedback;

	TestTrue(TEXT("Selection card play is accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	TestTrue(TEXT("Gameplay owns an authoritative pending card selection"),
		ViewModel->HasAuthoritativePendingCardSelection());
	TestTrue(TEXT("Gameplay resolver remains pending"),
		Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	// Model the exact catch-up window deterministically even if a platform happens
	// to dispatch a scheduled read edge sooner than expected. Candidate exposure
	// remains gated because the displayed revision is still the preceding one.
	ViewModel->BattleId = DisplayedBattleIdBefore;
	ViewModel->StateRevision = DisplayedRevisionBefore;
	TestFalse(TEXT("Pending selection is not display-readable before boundary catch-up"),
		ViewModel->HasPendingCardSelection());
	TestTrue(TEXT("Authoritative pending selection remains visible to fail-closed routing"),
		ViewModel->HasAuthoritativePendingCardSelection());

	TestFalse(TEXT("Catch-up-window candidate click is not accepted as ordinary card play"),
		HUD->SelectCard(Candidate->GetRuntimeId()));
	TestEqual(TEXT("Ordinary card selection state is not entered"),
		ViewModel->SelectedCardRuntimeId,
		INDEX_NONE);
	TestTrue(TEXT("Fail-closed click does not replace feedback with ordinary play validation"),
		ViewModel->LastFeedback.EqualTo(FeedbackBefore));
	TestTrue(TEXT("Gameplay selection stays pending after swallowed click"),
		Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionRightMouseButtonCancelTest,
	"SlayTheSpireDemo.CardSelection.Presentation.Input.RightMouseButtonCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionRightMouseButtonCancelTest::RunTest(const FString& Parameters)
{
	// Ordinary card/target selection is presentation input and must be cancelled
	// through the same ViewModel path as the visible Cancel button.
	{
		FFixture Fixture;
		UCardData* Attack = Fixture.CreateCard(TEXT("RightClickAttack"), ECardTargetType::Enemy);
		if (!TestTrue(TEXT("Ordinary-selection fixture starts"), Fixture.Start({ Attack }))) return false;

		UCardInstance* AttackInstance = Fixture.FindHandCard(TEXT("RightClickAttack"));
		UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
		UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
		if (!TestNotNull(TEXT("Ordinary-selection card exists"), AttackInstance)
			|| !TestTrue(TEXT("Ordinary-selection ViewModel initializes"), VM->Initialize(Fixture.Battle, false))
			|| !TestNotNull(TEXT("Ordinary-selection HUD exists"), HUD))
		{
			return false;
		}
		HUD->SetTestWorld(Fixture.World);
		HUD->ConfigureSelectionCanvasForTesting(VM);

		TestTrue(TEXT("Attack enters target selection"), HUD->SelectCard(AttackInstance->GetRuntimeId()));
		TestEqual(TEXT("Attack waits for a target"), VM->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
		TestTrue(TEXT("Right click is consumed by the HUD"), HUD->InvokeRightMouseButtonCancelForTesting());
		TestEqual(TEXT("Right click clears the selected card"), VM->SelectedCardRuntimeId, INDEX_NONE);
		TestEqual(TEXT("Right click returns to idle"), VM->InteractionState, EBattleHUDInteractionState::Idle);
		TestFalse(TEXT("Right click leaves input locked"), VM->bInputLocked);
	}

	// A Gameplay-owned mandatory selection has its own cancel policy. Right click
	// must not bypass that policy or call ordinary card-play cancellation.
	{
		FFixture Fixture;
		if (!TestTrue(TEXT("Mandatory-selection fixture starts"), Fixture.Start({
			Fixture.CreateSelectionCard(TEXT("RightClickMandatory")),
			Fixture.CreateCard(TEXT("RightClickCandidate"))
		})))
		{
			return false;
		}
		UCardInstance* Choice = Fixture.FindHandCard(TEXT("RightClickMandatory"));
		TestTrue(TEXT("Mandatory choice play is accepted"),
			Fixture.Battle->RequestPlayCard(Choice, nullptr).IsAcceptedForResolution());
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();

		UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
		UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
		if (!TestTrue(TEXT("Mandatory-selection ViewModel initializes"), VM->Initialize(Fixture.Battle, false))
			|| !TestNotNull(TEXT("Mandatory-selection HUD exists"), HUD))
		{
			return false;
		}
		HUD->SetTestWorld(Fixture.World);
		HUD->ConfigureSelectionCanvasForTesting(VM);

		TestTrue(TEXT("Mandatory selection is authoritative and pending"), VM->HasAuthoritativePendingCardSelection());
		TestFalse(TEXT("Forbidden mandatory selection does not consume right click"), HUD->InvokeRightMouseButtonCancelForTesting());
		TestTrue(TEXT("Forbidden mandatory selection remains pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
		TestEqual(TEXT("Forbidden mandatory selection does not enter ordinary card state"),
			VM->SelectedCardRuntimeId,
			INDEX_NONE);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionProductionConfirmRoutingTest,
	"SlayTheSpireDemo.CardSelection.Presentation.Input.ProductionConfirmRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionProductionConfirmRoutingTest::RunTest(const FString& Parameters)
{
	UClass* ProductionHUD = LoadClass<UBattleHUDWidget>(nullptr,
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"));
	if (!TestNotNull(TEXT("Production Native HUD loads"), ProductionHUD)) return false;
	TestTrue(TEXT("Production HUD inherits shared selection routing and transfer"),
		ProductionHUD->IsChildOf(UBattleHUDSelectionWidget::StaticClass()));
	FFixture Fixture;
	UCardData* SelectionCard = Fixture.CreateSelectionCard(TEXT("ConfirmSelection"));
	SelectionCard->DefaultDestination = ECardDestination::Exhaust;
	UCardData* CandidateCard = Fixture.CreateCard(TEXT("ConfirmCandidate"), ECardTargetType::Enemy);
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectionCard, CandidateCard }))) return false;
	UCardInstance* Played = Fixture.FindHandCard(TEXT("ConfirmSelection"));
	UCardInstance* Candidate = Fixture.FindHandCard(TEXT("ConfirmCandidate"));
	if (!Played || !Candidate) return false;
	TestTrue(TEXT("Selection play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
	if (!TestTrue(TEXT("ViewModel initialized at decision"), VM->Initialize(Fixture.Battle, false))) return false;
	UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	HUD->SetTestWorld(Fixture.World);
	HUD->ConfigureSelectionCanvasForTesting(VM);
	UButton* Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	TestFalse(TEXT("Confirm disabled before choosing"), Confirm->GetIsEnabled());
	TestTrue(TEXT("Enemy-target card can be chosen as a card, without enemy targeting"), HUD->SelectCard(Candidate->GetRuntimeId()));
	TestTrue(TEXT("Candidate click leaves Gameplay pending"), VM->HasPendingCardSelection());
	TestTrue(TEXT("Confirm enabled at exact count"), Confirm->GetIsEnabled());
	const FText FeedbackBefore = VM->LastFeedback;
	Confirm->OnClicked.Broadcast();
	TestFalse(TEXT("Actual bound Confirm submits pending selection"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	TestEqual(TEXT("Chosen card enters DrawPile"), Deck->GetDrawCount(), 1);
	TestEqual(TEXT("Chosen exact identity is on top"), Deck->GetDrawCards().Last().Get(), Candidate);
	TestEqual(TEXT("Played card uses authored Exhaust cleanup"), Deck->GetExhaustCount(), 1);
	TestTrue(TEXT("Confirm never produces ordinary legal-target feedback"), VM->LastFeedback.EqualTo(FeedbackBefore));
	TestFalse(TEXT("Submitted Confirm is disabled immediately"), Confirm->GetIsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectionPresentationG5LifecycleTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G5.PersistentSequentialOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG5LifecycleTest::RunTest(const FString& Parameters)
{
	for (const bool bExhaust : { false, true })
	{
	FFixture Fixture;
	UCardData* Choice = Fixture.CreateSelectionCard(TEXT("G5ChooseThree"));
	CastChecked<USelectHandCardToDrawPileTopEffect>(Choice->Effects[0])->BaseSelectionCount = 3;
	if (bExhaust)
	{
		Choice->Effects.Reset();
		USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Choice);
		Effect->BaseSelectionCount = 3;
		Choice->Effects.Add(Effect);
	}
	if (!TestTrue(TEXT("Battle starts"), Fixture.Start({ Choice, Fixture.CreateCard(TEXT("A")), Fixture.CreateCard(TEXT("B")), Fixture.CreateCard(TEXT("C")) }))) return false;
	TestTrue(TEXT("Choice begins"), Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("G5ChooseThree")), nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
	VM->Initialize(Fixture.Battle, true);
	UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	TStrongObjectPtr<UCardSelectionPresentationHUDProbe> KeepHUD(HUD);
	HUD->SetTestWorld(Fixture.World);
	// Public observer is registered BEFORE the Native HUD; it must see both
	// surfaces committed, including under GC during an ownership publication.
	bool bObservedCoherent = true;
	bool bCollectedDuringPublication = false;
	ON_SCOPE_EXIT { VM->OnCardPresentationOwnershipChanged.Clear(); };
	VM->OnCardPresentationOwnershipChanged.AddLambda([&](const TArray<int32>& Ids)
	{
		if (!bCollectedDuringPublication) { bCollectedDuringPublication = true; CollectGarbage(RF_NoFlags); }
		for (int32 Id : Ids)
		{
			if (VM->GetCardPresentationOwner(Id) != ECardPresentationOwner::SelectionArea) continue;
			const auto* State = HUD->SelectionVisuals.Find(Id);
			bObservedCoherent &= State && IsValid(State->Card) && State->Card->GetParent() == HUD->GetSelectionAreaHostForTesting();
			for (UWidget* Child : HUD->GetHandForTesting()->GetAllChildren())
				if (UBattleCardWidget* Formal = Cast<UBattleCardWidget>(Child); Formal && Formal->GetRuntimeId() == Id)
					bObservedCoherent &= Formal->GetVisibility() == ESlateVisibility::Hidden && !Formal->GetIsEnabled();
		}
	});
	HUD->ConfigureSelectionCanvasForTesting(VM);
	UButton* Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	FPresentationStateSnapshot Displayed;
	Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Displayed);
	TArray<int32> Ids;
	for (const auto& Card : VM->HandCards) Ids.Add(Card.RuntimeId);
	TestTrue(TEXT("First selected"), HUD->SelectCard(Ids[0]));
	UBattleCardWidget* First = HUD->SelectionVisuals.FindChecked(Ids[0]).Card;
	First->OnBattleCardRequested.Broadcast(Ids[0]);
	TestEqual(TEXT("Pending visual click deselects"), VM->GetPendingCardSelectionSelectedCount(), 0);
	TestNull(TEXT("Deselected visual released"), First->GetParent());
	for (int32 Id : Ids) TestTrue(TEXT("Select in persistent Host"), HUD->SelectCard(Id));
	TestTrue(TEXT("Observer before HUD sees coherent surfaces"), bObservedCoherent);
	TArray<TObjectPtr<UBattleCardWidget>> Visuals;
	TArray<FVector2D> Positions;
	for (int32 Id : Ids)
	{
		Visuals.Add(HUD->SelectionVisuals.FindChecked(Id).Card);
		Positions.Add(Visuals.Last()->GetRenderTransform().Translation);
	}
	CollectGarbage(RF_NoFlags);
	for (int32 Index = 0; Index < Ids.Num(); ++Index) TestTrue(TEXT("Prepared/owned object survives GC"), IsValid(Visuals[Index]));
	TArray<FPresentationResolutionEnvelope> Outcomes;
	const FDelegateHandle Capture = Fixture.Battle->OnPresentationResolutionReady.AddLambda([&](const FPresentationResolutionEnvelope& Envelope) { Outcomes.Add(Envelope); });
	Confirm->OnClicked.Broadcast();
	for (int32 Id : Ids)
	{
		FCardPresentationOwnershipEntry Entry;
		TestTrue(TEXT("Request clearing retains exact owner"), VM->TryGetCardPresentationOwnershipEntry(Id, Entry));
		TestEqual(TEXT("Confirmed phase"), Entry.Phase, ESelectionPresentationVisualPhase::Confirmed);
		TestFalse(TEXT("Confirmed input disabled"), HUD->SelectionVisuals.FindChecked(Id).Card->GetIsEnabled());
	}
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	Fixture.Battle->OnPresentationResolutionReady.Remove(Capture);
	int32 Consumed = 0;
	for (const auto& Envelope : Outcomes)
	{
		for (const auto& Record : Envelope.Records)
		{
			if (Record.Type != EBattlePresentationRecordType::CardZoneChanged || Record.CardZoneChanged.FromZone != ECardZone::Hand) continue;
			FPresentationPlaybackToken Token;
			Token.BattleId = Record.BattleId; Token.ResolutionId = Record.ResolutionId;
			Token.PresentationSequence = Record.PresentationSequence; Token.LocalPlaybackGeneration = ++Consumed;
			const int32 Id = Record.CardZoneChanged.Card.RuntimeId;
			const int32 VisualIndex = Ids.Find(Id);
			FPresentationRecord Declined = Record;
			Declined.CardZoneChanged.ToIndex += 100;
			TestFalse(TEXT("Declined preflight does not take SelectionArea visual"), HUD->PlayPresentationRecord(Declined, Token));
			TestTrue(TEXT("Decline keeps exact object in Host"), Visuals[VisualIndex]->GetParent() == HUD->GetSelectionAreaHostForTesting());
			if (!TestTrue(TEXT("G4 accepts real SelectionArea source"), HUD->PlayPresentationRecord(Record, Token))) return false;
			TestTrue(TEXT("Transition consumes same visible object"), HUD->GetActiveNativeCardTransitionVisualForTesting() == Visuals[VisualIndex]);
			if (!bExhaust && Consumed == 2) HUD->CancelTrackedPresentationPlayback(Token);
			else HUD->FinishNativeCardTransitionForTesting(Token);
			FTSTicker::GetCoreTicker().Tick(0.0f);
			Displayed.HandCards.RemoveAt(Record.CardZoneChanged.FromIndex);
			if (bExhaust) ++Displayed.ExhaustCount;
			else ++Displayed.DrawCount;
			VM->ApplyPresentationSnapshot(Displayed, false);
			for (int32 Index = 0; Index < Ids.Num(); ++Index)
				if (const auto* Waiting = HUD->SelectionVisuals.Find(Ids[Index]))
				{
					TestTrue(TEXT("Later visible object survives Hand reconciliation"), Waiting->Card == Visuals[Index]);
					TestEqual(TEXT("Later confirmed position never re-centers"), Waiting->Card->GetRenderTransform().Translation, Positions[Index]);
				}
		}
	}
	TestEqual(TEXT("Three sequential children consumed"), Consumed, 3);
	TestEqual(TEXT("No remaining Host ghosts"), HUD->GetSelectionAreaHostForTesting()->GetChildrenCount(), 0);
	TestTrue(TEXT("All ownership notifications coherent"), bObservedCoherent);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectionPresentationG5ConfirmTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G5.ConfirmRejectionAndDirectOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG5ConfirmTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	Fixture.Battle->bEnableCommittedPresentationRecording = false;
	if (!TestTrue(TEXT("Direct battle starts"), Fixture.Start({ Fixture.CreateSelectionCard(TEXT("Choice")), Fixture.CreateCard(TEXT("Candidate")) }))) return false;
	Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("Choice")), nullptr);
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	USelectionPresentationSubmitProbe* VM = NewObject<USelectionPresentationSubmitProbe>(Fixture.World);
	VM->Initialize(Fixture.Battle, false);
	UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	HUD->SetTestWorld(Fixture.World);
	HUD->ConfigureSelectionCanvasForTesting(VM);
	UButton* Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	const int32 Id = Fixture.FindHandCard(TEXT("Candidate"))->GetRuntimeId();
	TestTrue(TEXT("Candidate selected"), HUD->SelectCard(Id));
	UBattleCardWidget* Visual = HUD->SelectionVisuals.FindChecked(Id).Card;
	VM->bReject = true;
	Confirm->OnClicked.Broadcast();
	FCardPresentationOwnershipEntry Entry;
	TestTrue(TEXT("Rejected submit retains ownership"), VM->TryGetCardPresentationOwnershipEntry(Id, Entry));
	TestEqual(TEXT("Rejected phase remains Pending"), Entry.Phase, ESelectionPresentationVisualPhase::Pending);
	TestTrue(TEXT("Rejected choice keeps same enabled visual"), Visual == HUD->SelectionVisuals.FindChecked(Id).Card && Visual->GetIsEnabled());
	TestTrue(TEXT("Rejected confirm remains retryable"), Confirm->GetIsEnabled());
	VM->bReject = false;
	Confirm->OnClicked.Broadcast();
	TestFalse(TEXT("Accepted choice clears Gameplay pending request"), VM->HasAuthoritativePendingCardSelection());
	TestTrue(TEXT("Accepted visual survives until actual read edge"), Visual->GetParent() == HUD->GetSelectionAreaHostForTesting());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestEqual(TEXT("Direct baseline clears consumed ownership without animation"), VM->GetCardPresentationOwner(Id), ECardPresentationOwner::Hand);
	TestNull(TEXT("Direct path leaves no ghost"), Visual->GetParent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectionPresentationG5NoDestinationTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G5.NoDestinationAndMissingCorrelation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG5NoDestinationTest::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FFixture Fixture;
		Fixture.Battle->bEnableCommittedPresentationRecording = Mode != 1;
		UCardData* Choice = Fixture.CreateCard(TEXT("NoDestination"));
		Choice->Effects.Add(NewObject<USelectionNoDestinationEffectProbe>(Choice));
		if (!Fixture.Start({ Choice, Fixture.CreateCard(TEXT("StaysInHand")) })) return false;
		Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("NoDestination")), nullptr);
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
		VM->Initialize(Fixture.Battle, Mode != 1);
		UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
		HUD->SetTestWorld(Fixture.World);
		HUD->ConfigureSelectionCanvasForTesting(VM);
		UButton* Confirm = NewObject<UButton>(HUD);
		HUD->BindConfirmButtonForTesting(Confirm);
		const int32 Id = Fixture.FindHandCard(TEXT("StaysInHand"))->GetRuntimeId();
		if (!TestTrue(TEXT("No-destination candidate selected"), HUD->SelectCard(Id))) return false;
		UBattleCardWidget* Visual = CastChecked<UBattleCardWidget>(HUD->GetSelectionAreaHostForTesting()->GetChildAt(0));
		if (Mode == 2) Fixture.Battle->OnPresentationResolutionReady.RemoveAll(VM);
		Confirm->OnClicked.Broadcast();
		TestEqual(TEXT("Request clearing cannot restore waiting card"), VM->GetCardPresentationOwner(Id), ECardPresentationOwner::SelectionArea);
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		if (Mode == 0)
		{
			FCardPresentationOwnershipEntry Entry;
			TestTrue(TEXT("Zero eligible destinations still arm recorded receipt"), VM->TryGetCardPresentationOwnershipEntry(Id, Entry));
			TestEqual(TEXT("Recorded mode is exact"), Entry.CompletionWatermark.Mode, ESelectionPresentationCompletionMode::RecordedResolution);
			VM->MarkPresentationResolutionCompleted(VM->BattleId, Entry.CompletionWatermark.ResolutionId + 100);
			TestEqual(TEXT("Unrelated completion does not release"), VM->GetCardPresentationOwner(Id), ECardPresentationOwner::SelectionArea);
			VM->MarkPresentationResolutionCompleted(VM->BattleId, Entry.CompletionWatermark.ResolutionId);
		}
		TestEqual(TEXT("No destination completes/recoveries to Hand without a ghost"), VM->GetCardPresentationOwner(Id), ECardPresentationOwner::Hand);
		TestNull(TEXT("Old SelectionArea visual retired"), Visual->GetParent());
		TestEqual(TEXT("Authoritative candidate stays in Gameplay Hand"), Fixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 1);
		if (Mode == 2)
		{
			TestEqual(TEXT("Missing correlation has explicit UI-only unavailable recovery"), VM->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
			TestFalse(TEXT("Missing metadata does not fault Gameplay"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectionPresentationG5ReplacementTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G5.PreflightReplacementAndSynchronousSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG5ReplacementTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!Fixture.Start({ Fixture.CreateSelectionCard(TEXT("ReplaceChoice")), Fixture.CreateCard(TEXT("ReplaceCandidate")) })) return false;
	Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("ReplaceChoice")), nullptr);
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	USelectionPresentationSubmitProbe* VM = NewObject<USelectionPresentationSubmitProbe>(Fixture.World);
	VM->Initialize(Fixture.Battle, true);
	UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	HUD->SetTestWorld(Fixture.World);
	HUD->ConfigureSelectionCanvasForTesting(VM);
	UButton* Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	const int32 Id = Fixture.FindHandCard(TEXT("ReplaceCandidate"))->GetRuntimeId();
	HUD->DisableSyntheticSelectionGeometryForTesting();
	TestFalse(TEXT("Unlaid-out Host rejects before Hand hide"), HUD->SelectCard(Id));
	TestEqual(TEXT("Preflight rejection changes no owner"), VM->GetCardPresentationOwner(Id), ECardPresentationOwner::Hand);
	// Replace the entire HUD and test that its Pending generation can be retried.
	HUD->DestructSelectionForTesting();
	HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	HUD->SetTestWorld(Fixture.World);
	HUD->ConfigureSelectionCanvasForTesting(VM);
	Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	TestTrue(TEXT("Replacement can select"), HUD->SelectCard(Id));
	UBattleCardWidget* OldVisual = CastChecked<UBattleCardWidget>(HUD->GetSelectionAreaHostForTesting()->GetChildAt(0));
	HUD->DestructSelectionForTesting();
	TestNull(TEXT("HUD destruction releases visible object"), OldVisual->GetParent());
	TestEqual(TEXT("HUD destruction resets Pending UI choice"), VM->GetPendingCardSelectionSelectedCount(), 0);
	HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
	HUD->SetTestWorld(Fixture.World);
	HUD->ConfigureSelectionCanvasForTesting(VM);
	Confirm = NewObject<UButton>(HUD);
	HUD->BindConfirmButtonForTesting(Confirm);
	TestTrue(TEXT("Next HUD can select same exact candidate"), HUD->SelectCard(Id));
	FPresentationStateSnapshot Boundary;
	Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Boundary);
	const int64 BoundaryRevision = VM->StateRevision;
	bool bDeferredSnapshot = false;
	bool bSynchronousOutcomeWasPending = false;
	VM->DuringSubmit = [&]()
	{
		FPresentationStateSnapshot Later = Boundary;
		Later.StateRevision += 1;
		VM->ApplyPresentationSnapshot(Later, true);
		bDeferredSnapshot = VM->StateRevision == BoundaryRevision;
	};
	VM->AfterSubmit = [&]()
	{
		// Force the real sealed receipts/read edge to arrive before the submit
		// wrapper returns, rather than fabricating a guessed ResolutionId.
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		FCardPresentationOwnershipEntry During;
		bSynchronousOutcomeWasPending = VM->TryGetCardPresentationOwnershipEntry(Id, During)
			&& During.Phase == ESelectionPresentationVisualPhase::Pending
			&& !During.CompletionWatermark.IsResolved();
	};
	Confirm->OnClicked.Broadcast();
	VM->DuringSubmit = nullptr;
	VM->AfterSubmit = nullptr;
	TestTrue(TEXT("Synchronous snapshot held until submit commits"), bDeferredSnapshot);
	TestTrue(TEXT("Synchronous outcome is buffered until acceptance commits Confirmed"), bSynchronousOutcomeWasPending);
	FCardPresentationOwnershipEntry Entry;
	TestTrue(TEXT("Confirmed owner survives deferred snapshot revision change"), VM->TryGetCardPresentationOwnershipEntry(Id, Entry));
	TestEqual(TEXT("Commit after acceptance preserves Confirmed phase"), Entry.Phase, ESelectionPresentationVisualPhase::Confirmed);
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestTrue(TEXT("Exact receipt eventually resolved"), VM->TryGetCardPresentationOwnershipEntry(Id, Entry) && Entry.CompletionWatermark.IsResolved());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectionPresentationG5SubmitReplacementTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G5.RejectedSubmitReplacementAndFaultSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG5SubmitReplacementTest::RunTest(const FString& Parameters)
{
	for (const bool bRestart : { false, true })
	{
		FFixture Fixture;
		if (!Fixture.Start({ Fixture.CreateSelectionCard(TEXT("OldChoice")), Fixture.CreateCard(TEXT("OldCandidate")) })) return false;
		Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("OldChoice")), nullptr);
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		USelectionPresentationSubmitProbe* VM = NewObject<USelectionPresentationSubmitProbe>(Fixture.World);
		VM->Initialize(Fixture.Battle, true);
		UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(Fixture.World);
		HUD->SetTestWorld(Fixture.World);
		HUD->ConfigureSelectionCanvasForTesting(VM);
		UButton* Confirm = NewObject<UButton>(HUD);
		HUD->BindConfirmButtonForTesting(Confirm);
		const int32 Id = Fixture.FindHandCard(TEXT("OldCandidate"))->GetRuntimeId();
		if (!HUD->SelectCard(Id)) return false;
		UBattleCardWidget* Visual = CastChecked<UBattleCardWidget>(HUD->GetSelectionAreaHostForTesting()->GetChildAt(0));
		FPresentationStateSnapshot Boundary;
		Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Boundary);
		VM->bReject = true;
		VM->DuringSubmit = [&]()
		{
			FPresentationStateSnapshot Replacement = Boundary;
			if (bRestart)
			{
				Fixture.Battle->StartBattle();
				Fixture.Battle->RequestPlayCard(Fixture.FindHandCard(TEXT("OldChoice")), nullptr);
				Fixture.Battle->FlushScheduledReadStateReadyForTesting();
				Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Replacement);
			}
			else
			{
				// Inject the immutable terminal display edge, not a new Gameplay rule.
				Replacement.StateRevision += 1;
				Replacement.BattleState = EBattleState::ResolutionFaulted;
				Replacement.Outcome = EBattleHUDOutcome::ResolutionFaulted;
			}
			VM->ApplyPresentationSnapshot(Replacement, true);
		};
		Confirm->OnClicked.Broadcast();
		VM->DuringSubmit = nullptr;
		TestNull(TEXT("Rejected old scope releases its visual on replacement/fault"), Visual->GetParent());
		TestEqual(TEXT("Old choices are not restored across the new boundary"), VM->GetPendingCardSelectionSelectedCount(), 0);
		TestEqual(TEXT("No old visual survives in Host"), HUD->GetSelectionAreaHostForTesting()->GetChildrenCount(), 0);
		if (bRestart)
		{
			TestTrue(TEXT("New Battle has distinct identity"), VM->BattleId != Boundary.BattleId);
			TestTrue(TEXT("Replacement request remains selectable"), HUD->SelectCard(Fixture.FindHandCard(TEXT("OldCandidate"))->GetRuntimeId()));
		}
		else TestEqual(TEXT("Fault display stays terminal"), VM->InteractionState, EBattleHUDInteractionState::Terminal);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
