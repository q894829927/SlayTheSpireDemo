#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2ATestTypes.h"
#include "CardExpansionWave1CTestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDInteractionReadiness.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8BTest
{
	struct FSelectionBattleFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FSelectionBattleFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("G8BPlayer");
			Enemy->PresentationId = TEXT("G8BEnemy");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
		}

		~FSelectionBattleFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* MakePlain(const TCHAR* Id)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(Id);
			Card->DisplayName = FText::FromString(Id);
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* MakePlayerExhaust(const TCHAR* Id)
		{
			UCardData* Card = MakePlain(Id);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->BaseSelectionCount = 1;
			Effect->UpgradedSelectionCount = 1;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Cards)
		{
			if (!IsValid(Battle) || Cards.IsEmpty()) return false;
			Battle->OpeningHandDrawCount = Cards.Num();
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Card : Cards) Battle->DebugStartingDeck.Add(Card);
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn;
		}

		UCardInstance* FindHand(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck)) return nullptr;
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId) return Card.Get();
			}
			return nullptr;
		}
	};

	FPresentationResolutionEnvelope MakeAsyncProbeEnvelope(
		const FPresentationStateSnapshot& Baseline,
		int64 ResolutionId)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;

		FPresentationRecord Record;
		Record.BattleId = Baseline.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::BlockChanged;
		Record.BlockChanged.SourcePresentationId = NAME_None;
		Record.BlockChanged.TargetPresentationId = Baseline.Player.PresentationId;
		Record.BlockChanged.Reason = EBlockPresentationReason::Gain;
		Record.BlockChanged.BlockBefore = Baseline.Player.Block;
		Record.BlockChanged.BlockAfter = Baseline.Player.Block;
		Record.BlockChanged.BlockDelta = 0;
		Envelope.Records.Add(Record);
		return Envelope;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BTargetChoiceEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.TargetChoiceEndTurnBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BTargetChoiceEndTurnTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 0);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	if (!TestTrue(TEXT("Enemy-target card enters target choice"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId))) return false;
	TestEqual(TEXT("Target choice mode is active"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestFalse(TEXT("Target choice explicitly disables EndTurn"), Fixture.ViewModel->bCanEndTurn);

	const FBattleHUDInteractionReadinessShadow Shadow =
		Fixture.ViewModel->EvaluateInteractionReadinessShadow(nullptr);
	TestTrue(TEXT("DirectBaseline authority is recognized"), Shadow.AuthoritySource == EBattleHUDReadinessAuthoritySource::DirectBaseline);
	TestTrue(TEXT("TargetChoice is a distinct readiness mode"), Shadow.Mode == EBattleHUDReadinessMode::CardTargetChoice);
	TestTrue(TEXT("TargetChoice remains interaction-ready"), Shadow.bReady);
	TestTrue(TEXT("TargetChoice shadow matches post-fix baseline"), Shadow.bMatchesBaseline);

	const int32 SelectedBeforeEndTurn = Fixture.ViewModel->SelectedCardRuntimeId;
	TestFalse(TEXT("EndTurn request is rejected while choosing target"), Fixture.ViewModel->RequestEndTurn());
	TestEqual(TEXT("Rejected EndTurn preserves target-choice state"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestEqual(TEXT("Rejected EndTurn preserves selected card"), Fixture.ViewModel->SelectedCardRuntimeId, SelectedBeforeEndTurn);
	TestEqual(TEXT("Gameplay remains in player turn"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BDirectReadinessModesTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.Readiness.DirectBaselineModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BDirectReadinessModesTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	FBattleHUDInteractionReadinessShadow Shadow =
		Fixture.ViewModel->EvaluateInteractionReadinessShadow(nullptr);
	TestTrue(TEXT("Idle direct surface is exact"), Shadow.bExactReadSurface);
	TestTrue(TEXT("Idle direct surface is ready"), Shadow.bReady);
	TestTrue(TEXT("Idle direct shadow matches baseline"), Shadow.bMatchesBaseline);
	TestTrue(TEXT("Idle mode classified as NormalPlayerTurn"), Shadow.Mode == EBattleHUDReadinessMode::NormalPlayerTurn);

	if (!TestTrue(TEXT("Untargeted card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(Fixture.FirstRuntimeId()))) return false;
	Shadow = Fixture.ViewModel->EvaluateInteractionReadinessShadow(nullptr);
	TestTrue(TEXT("ReadyToConfirm classified separately"), Shadow.Mode == EBattleHUDReadinessMode::CardReadyToConfirm);
	TestTrue(TEXT("ReadyToConfirm remains ready"), Shadow.bReady);
	TestTrue(TEXT("ReadyToConfirm shadow matches baseline"), Shadow.bMatchesBaseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BResolverIdentityABATest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.ResolverABA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BResolverIdentityABATest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World created"), World)) return false;

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(World);
	USelectionResolver* Resolver = NewObject<USelectionResolver>(World);
	Resolver->Initialize(FSelectionResolverQueueAccess::CreateLambda(
		[Queue](const USelectionResolver*) -> UBattleActionQueue* { return Queue; }));

	UWave1CTestCandidateObject* CandidateObject = NewObject<UWave1CTestCandidateObject>(World);
	FSelectionRequest Request;
	Request.SelectionSource = TEXT("G8B.IdenticalRequest");
	Request.MinCount = Request.MaxCount = 1;
	FSelectionCandidate Candidate;
	Candidate.RuntimeObject = CandidateObject;
	Candidate.RuntimeSequence = 42;
	Candidate.SelectionKey = TEXT("SameCandidate");
	Request.Candidates.Add(Candidate);

	int32 MarkerCount = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(World);
	Continuation->Configure(&MarkerCount, TEXT("G8B"), 1);

	FPendingSelectionRequestIdentity FirstIdentity;
	FirstIdentity.BattleId = 7;
	FirstIdentity.SelectionBoundaryRevision = 100;
	USelectionRequestAction* FirstAction = NewObject<USelectionRequestAction>(Queue);
	FirstAction->Initialize(Resolver, Request, Continuation, FirstIdentity);
	Queue->AddToBack(FirstAction);
	Queue->StartProcessing();
	TestTrue(TEXT("First exact identity is active"), Resolver->IsPendingRequestIdentity(FirstIdentity));

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(CandidateObject);
	FirstAction->ResolvePendingSelection(Result);
	TestFalse(TEXT("First request clears after resolution"), Resolver->HasPendingSelection());

	FPendingSelectionRequestIdentity SecondIdentity;
	SecondIdentity.BattleId = 7;
	SecondIdentity.SelectionBoundaryRevision = 101;
	USelectionRequestAction* SecondAction = NewObject<USelectionRequestAction>(Queue);
	SecondAction->Initialize(Resolver, Request, Continuation, SecondIdentity);
	Queue->AddToBack(SecondAction);
	Queue->StartProcessing();

	TestFalse(TEXT("Old boundary cannot ABA-match identical next request"), Resolver->IsPendingRequestIdentity(FirstIdentity));
	TestTrue(TEXT("Second identical request owns its new boundary"), Resolver->IsPendingRequestIdentity(SecondIdentity));
	SecondAction->ResolvePendingSelection(Result);
	TestEqual(TEXT("Both identical requests resolved independently"), MarkerCount, 2);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BPendingReadAndStaleSubmitTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.ReadFenceAndStaleSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BPendingReadAndStaleSubmitTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG8BTest;
	FSelectionBattleFixture Fixture;
	UCardData* SelectCard = Fixture.MakePlayerExhaust(TEXT("G8BSelect"));
	UCardData* CandidateCard = Fixture.MakePlain(TEXT("G8BCandidate"));
	if (!TestTrue(TEXT("Selection fixture starts"), Fixture.Start({ SelectCard, CandidateCard }))) return false;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	if (!TestTrue(TEXT("Direct ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, false))) return false;
	UCardInstance* Played = Fixture.FindHand(TEXT("G8BSelect"));
	if (!TestNotNull(TEXT("Selection card exists"), Played)) return false;
	if (!TestTrue(TEXT("Player selection card starts resolution"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution())) return false;

	TestTrue(TEXT("Authoritative pending request exists before read edge"), ViewModel->HasAuthoritativePendingCardSelection());
	FPendingCardSelectionReadView HiddenView;
	TestFalse(TEXT("Pending candidates stay hidden before exact read edge"), ViewModel->TryGetPendingCardSelectionReadView(HiddenView));

	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Pending read surface appears at exact boundary"), ViewModel->TryGetPendingCardSelectionReadView(View))) return false;
	TestTrue(TEXT("Pending identity is valid"), View.RequestIdentity.IsValid());
	TestEqual(TEXT("Pending BattleId matches displayed VM"), View.RequestIdentity.BattleId, ViewModel->BattleId);
	TestEqual(TEXT("Pending boundary matches displayed revision"), View.RequestIdentity.SelectionBoundaryRevision, ViewModel->StateRevision);

	const FBattleHUDInteractionReadinessShadow PendingShadow =
		ViewModel->EvaluateInteractionReadinessShadow(nullptr);
	TestTrue(TEXT("Pending mode has authority priority over ordinary enum"), PendingShadow.Mode == EBattleHUDReadinessMode::PendingCardSelection);
	TestTrue(TEXT("Visible exact pending request is ready"), PendingShadow.bReady);
	TestTrue(TEXT("Pending shadow matches baseline"), PendingShadow.bMatchesBaseline);

	FPendingSelectionRequestIdentity StaleIdentity = View.RequestIdentity;
	++StaleIdentity.SelectionBoundaryRevision;
	TestFalse(TEXT("Stale expected boundary cannot submit current request"),
		BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, StaleIdentity, View.CandidateRuntimeIds));
	TestTrue(TEXT("Stale submit leaves current exact request pending"),
		Fixture.Battle->GetSelectionResolver()->IsPendingRequestIdentity(View.RequestIdentity));
	TestTrue(TEXT("Exact expected boundary submits normally"),
		BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, View.RequestIdentity, View.CandidateRuntimeIds));
	TestFalse(TEXT("Exact submit clears pending request"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BStaleCancelTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.StaleCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BStaleCancelTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG8BTest;
	FSelectionBattleFixture Fixture;
	UCardData* CandidateDefinition = Fixture.MakePlain(TEXT("G8BCancelCandidate"));
	if (!TestTrue(TEXT("Cancel fixture starts"), Fixture.Start({ CandidateDefinition }))) return false;

	UCardInstance* Card = Fixture.FindHand(TEXT("G8BCancelCandidate"));
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Candidate card exists"), Card)
		|| !TestNotNull(TEXT("Battle queue exists"), Queue)
		|| !TestNotNull(TEXT("Battle resolver exists"), Resolver)) return false;

	FSelectionRequest Request;
	Request.SelectionSource = TEXT("G8B.CancelProbe");
	Request.MinCount = Request.MaxCount = 1;
	Request.CancelPolicy = ESelectionCancelPolicy::Allowed;
	FSelectionCandidate Candidate;
	Candidate.RuntimeObject = Card;
	Candidate.RuntimeSequence = Card->GetRuntimeId();
	Candidate.SelectionKey = Card->GetCardId();
	Request.Candidates.Add(Candidate);

	int32 MarkerCount = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&MarkerCount, TEXT("CancelProbe"), 1);
	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;

	FPendingSelectionRequestIdentity Identity;
	Identity.BattleId = Baseline.BattleId;
	Identity.SelectionBoundaryRevision = Baseline.StateRevision + 100;
	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Queue);
	Action->Initialize(Resolver, Request, Continuation, Identity);
	Queue->AddToBack(Action);
	Queue->StartProcessing();
	if (!TestTrue(TEXT("Cancellable exact request is pending"), Resolver->IsPendingRequestIdentity(Identity))) return false;

	FPendingSelectionRequestIdentity Stale = Identity;
	++Stale.SelectionBoundaryRevision;
	TestFalse(TEXT("Stale cancel cannot cancel current request"), BattleSelectionRequest::SubmitPendingSelectionCancel(Fixture.Battle, Stale));
	TestTrue(TEXT("Stale cancel preserves request"), Resolver->IsPendingRequestIdentity(Identity));
	TestTrue(TEXT("Exact cancel succeeds"), BattleSelectionRequest::SubmitPendingSelectionCancel(Fixture.Battle, Identity));
	TestFalse(TEXT("Exact cancel clears request"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Cancel never runs continuation marker"), MarkerCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BPresentationAuthorityDelayTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.Readiness.PresentationAuthorityAndSkippableDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BPresentationAuthorityDelayTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	using namespace SelectionPresentationG8BTest;
	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	if (!TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))
		|| !TestTrue(TEXT("Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget))) return false;

	FBattleHUDInteractionReadinessShadow Shadow = ViewModel->EvaluateInteractionReadinessShadow(Controller);
	TestTrue(TEXT("Presentation-owned authority is recognized"), Shadow.AuthoritySource == EBattleHUDReadinessAuthoritySource::PresentationOwned);
	TestTrue(TEXT("Presentation session is attached to readiness"), Shadow.SessionToken.IsValid());
	TestTrue(TEXT("Caught-up presentation-owned normal surface is ready"), Shadow.bReady);
	TestTrue(TEXT("Presentation-owned shadow matches baseline"), Shadow.bMatchesBaseline);
	TestFalse(TEXT("No chronology means no skippable delay"), Controller->HasSkippablePresentationDelay());

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 200;
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeAsyncProbeEnvelope(Baseline, ResolutionId));
	TestTrue(TEXT("Active Controller chronology is skippable"), Controller->HasSkippablePresentationDelay());

	FPresentationSessionToken CapturedSession;
	int64 CapturedBattleId = 0;
	int64 ExpectedRevision = 0;
	TestTrue(TEXT("Fast input captures an exact catch-up target"), Controller->TryCaptureFastInputCatchUpTarget(CapturedSession, CapturedBattleId, ExpectedRevision));
	TestEqual(TEXT("Captured BattleId is exact"), CapturedBattleId, Baseline.BattleId);
	TestEqual(TEXT("Expected catch-up revision is latest frozen revision"), ExpectedRevision, Baseline.StateRevision);

	FPresentationSessionToken SessionBeforeSkip;
	TestTrue(TEXT("Session available before ordinary Skip"), Controller->TryGetPresentationSessionToken(SessionBeforeSkip));
	Controller->SkipPresentation();
	FPresentationSessionToken SessionAfterSkip;
	TestTrue(TEXT("Session available after ordinary Skip"), Controller->TryGetPresentationSessionToken(SessionAfterSkip));
	TestTrue(TEXT("Ordinary Skip preserves exact session"), SessionAfterSkip == SessionBeforeSkip && SessionAfterSkip == CapturedSession);
	TestFalse(TEXT("Caught-up chronology is no longer skippable"), Controller->HasSkippablePresentationDelay());
	return true;
}

#endif
