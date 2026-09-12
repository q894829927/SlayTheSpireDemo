#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Battle/BattleManager.h"
#include "CardExpansionWave1CTestTypes.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Phase6UIA0TestTypes.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleBufferedPlayerIntent.h"

namespace SelectionPresentationG9ATest
{
	struct FBattleFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FBattleFixture()
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
			Player->PresentationId = TEXT("G9APlayer");
			Enemy->PresentationId = TEXT("G9AEnemy");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
		}

		~FBattleFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool Start()
		{
			if (!IsValid(Battle)) return false;
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn;
		}
	};

	FBufferedCardIntent MakeCardIntent()
	{
		FBufferedCardIntent Intent;
		Intent.SessionToken.BattleId = 7;
		Intent.SessionToken.ControllerEpoch = 1;
		Intent.SessionToken.PresentationSessionGeneration = 1;
		Intent.BattleId = 7;
		Intent.ExpectedReadyRevision = 11;
		Intent.CaptureWindow.SourceResolutionId = 3;
		Intent.CaptureWindow.SourcePresentationSequence = 5;
		Intent.CaptureWindow.LocalWindowGeneration = 1;
		Intent.RuntimeId = 42;
		return Intent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9APlayerTurnAuthorityABATest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.Authority.PlayerTurnABA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9APlayerTurnAuthorityABATest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FBattleFixture Fixture;
	if (!TestTrue(TEXT("Battle reaches first PlayerTurn"), Fixture.Start())) return false;

	FPlayerTurnAuthorityToken FirstTurn;
	if (!TestTrue(TEXT("First player-turn authority is available"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(FirstTurn))) return false;
	TestTrue(TEXT("First token is valid"), FirstTurn.IsValid());
	TestEqual(TEXT("First player turn uses serial 1"), FirstTurn.PlayerTurnSerial, uint64(1));

	Fixture.Battle->TestGainBlock();
	FPlayerTurnAuthorityToken SameTurn;
	if (!TestTrue(TEXT("Authority remains available after same-turn resolution"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(SameTurn))) return false;
	TestTrue(TEXT("Same player turn preserves exact authority token"), SameTurn == FirstTurn);

	if (!TestTrue(TEXT("EndTurn request is accepted"),
		Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution())) return false;
	FPlayerTurnAuthorityToken SecondTurn;
	if (!TestTrue(TEXT("Macro turn flow returns to next PlayerTurn"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(SecondTurn))) return false;
	TestEqual(TEXT("Battle identity remains stable across turns"), SecondTurn.BattleId, FirstTurn.BattleId);
	TestTrue(TEXT("Next player turn cannot ABA-match prior token"), SecondTurn != FirstTurn);
	TestEqual(TEXT("Second player turn increments serial exactly once"), SecondTurn.PlayerTurnSerial, uint64(2));

	Fixture.Battle->StartBattle();
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	FPlayerTurnAuthorityToken RestartedTurn;
	if (!TestTrue(TEXT("Restarted battle exposes a player-turn token"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(RestartedTurn))) return false;
	TestTrue(TEXT("Restarted battle gets a new BattleId"), RestartedTurn.BattleId != SecondTurn.BattleId);
	TestEqual(TEXT("Restarted battle first player turn restarts serial at 1"), RestartedTurn.PlayerTurnSerial, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9AResolutionIdleOpportunityTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.EndTurn.ResolutionIdleOpportunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9AResolutionIdleOpportunityTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FBattleFixture Fixture;
	if (!TestTrue(TEXT("Battle reaches PlayerTurn"), Fixture.Start())) return false;

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	if (!TestNotNull(TEXT("ActionQueue exists"), Queue)) return false;
	if (!TestTrue(TEXT("System presentation resolution opens"), Fixture.Battle->BeginSystemPresentationResolutionForTesting())) return false;

	UPhase6UIA0ManualFinishAction* Hold = NewObject<UPhase6UIA0ManualFinishAction>(Queue);
	if (!TestNotNull(TEXT("Manual-finish action created"), Hold)) return false;
	if (!TestTrue(TEXT("Manual-finish action enqueued"), Queue->AddToBack(Hold))) return false;
	if (!TestTrue(TEXT("Queue starts processing"), Queue->StartProcessing())) return false;
	TestTrue(TEXT("Manual action executed and keeps queue busy"), Hold->HasExecuted() && Queue->IsBusy());

	FBufferedEndTurnIntent Captured;
	if (!TestTrue(TEXT("EndTurn shadow capture accepts ResolutionBusy in same player turn"),
		TryCaptureBufferedEndTurnShadow(Fixture.Battle, Captured))) return false;

	FBufferedPlayerIntentShadowState Owner;
	if (!TestTrue(TEXT("Shadow owner stores captured EndTurn"), Owner.StoreEndTurn(Captured))) return false;
	FBufferedEndTurnIntent Stored;
	if (!TestTrue(TEXT("Stored EndTurn can be atomically observed by take/re-store"), Owner.TryTakeEndTurn(Stored))) return false;
	if (!TestTrue(TEXT("Stored EndTurn has owner generation"), Stored.IsValid())) return false;
	if (!TestTrue(TEXT("EndTurn can be restored for evaluation"), Owner.StoreEndTurn(Stored))) return false;
	if (!Owner.TryTakeEndTurn(Stored)) return false;

	TestEqual(
		TEXT("Busy exact turn evaluates Waiting"),
		EvaluateBufferedEndTurnShadow(Fixture.Battle, Stored),
		EBufferedIntentShadowEvaluation::Waiting);

	int32 OpportunityCount = 0;
	FPlayerTurnAuthorityToken OpportunityToken;
	Fixture.Battle->OnPlayerCommandOpportunity.AddLambda(
		[&OpportunityCount, &OpportunityToken](const FPlayerTurnAuthorityToken& Token)
		{
			++OpportunityCount;
			OpportunityToken = Token;
		});

	Hold->CompleteManually();
	TestFalse(TEXT("Manual completion settles queue"), Queue->IsBusy());
	TestEqual(TEXT("ResolutionIdle publishes one player-command opportunity"), OpportunityCount, 1);
	TestTrue(TEXT("Opportunity preserves exact captured player-turn authority"), OpportunityToken == Stored.Turn);
	TestEqual(
		TEXT("Same exact turn becomes Ready after ResolutionIdle"),
		EvaluateBufferedEndTurnShadow(Fixture.Battle, Stored),
		EBufferedIntentShadowEvaluation::Ready);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9AIntentArbitrationTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.Intent.Arbitration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9AIntentArbitrationTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FBufferedPlayerIntentShadowState Owner;
	const FBufferedCardIntent Card = MakeCardIntent();
	if (!TestTrue(TEXT("Probe card intent is valid"), Card.IsValid())) return false;
	if (!TestTrue(TEXT("Card intent stores into empty owner"), Owner.StoreCardSelection(Card))) return false;
	TestTrue(TEXT("Owner reports card intent"), Owner.HasCardSelection());

	FBufferedEndTurnIntent EndTurn;
	EndTurn.Turn.BattleId = 7;
	EndTurn.Turn.PlayerTurnSerial = 3;
	if (!TestTrue(TEXT("EndTurn overrides older card intent"), Owner.StoreEndTurn(EndTurn))) return false;
	TestTrue(TEXT("Owner reports EndTurn"), Owner.HasEndTurn());
	TestFalse(TEXT("EndTurn retirement clears old card intent"), Owner.HasCardSelection());
	TestFalse(TEXT("Card cannot replace accepted EndTurn"), Owner.StoreCardSelection(Card));

	FBufferedEndTurnIntent Taken;
	if (!TestTrue(TEXT("EndTurn consumes exactly once"), Owner.TryTakeEndTurn(Taken))) return false;
	TestTrue(TEXT("Taken EndTurn is fully valid"), Taken.IsValid());
	TestTrue(TEXT("Owner is empty after consume"), Owner.IsEmpty());
	TestFalse(TEXT("Second EndTurn consume is rejected"), Owner.TryTakeEndTurn(Taken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9AMandatorySelectionFenceTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.EndTurn.MandatorySelectionFence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9AMandatorySelectionFenceTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FBattleFixture Fixture;
	if (!TestTrue(TEXT("Battle reaches PlayerTurn"), Fixture.Start())) return false;

	FBufferedEndTurnIntent Captured;
	if (!TestTrue(TEXT("EndTurn shadow capture succeeds before mandatory selection"),
		TryCaptureBufferedEndTurnShadow(Fixture.Battle, Captured))) return false;
	FBufferedPlayerIntentShadowState Owner;
	if (!Owner.StoreEndTurn(Captured)) return false;
	FBufferedEndTurnIntent Stored;
	if (!Owner.TryTakeEndTurn(Stored)) return false;
	TestEqual(TEXT("Captured EndTurn starts Ready"),
		EvaluateBufferedEndTurnShadow(Fixture.Battle, Stored),
		EBufferedIntentShadowEvaluation::Ready);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("SelectionResolver exists"), Resolver)) return false;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	USelectionRequestAction* PendingAction = NewObject<USelectionRequestAction>(Fixture.Battle->GetActionQueueForTesting());
	UWave1CTestCandidateObject* CandidateObject = NewObject<UWave1CTestCandidateObject>(Fixture.World);
	if (!TestNotNull(TEXT("Continuation created"), Continuation)
		|| !TestNotNull(TEXT("Pending action created"), PendingAction)
		|| !TestNotNull(TEXT("Candidate created"), CandidateObject)) return false;

	FSelectionRequest Request;
	Request.SelectionSource = TEXT("G9A.Mandatory");
	Request.MinCount = 1;
	Request.MaxCount = 1;
	Request.CancelPolicy = ESelectionCancelPolicy::Forbidden;
	FSelectionCandidate Candidate;
	Candidate.RuntimeObject = CandidateObject;
	Candidate.RuntimeSequence = 1;
	Candidate.SelectionKey = TEXT("Candidate");
	Request.Candidates.Add(Candidate);
	if (!TestTrue(TEXT("Mandatory selection becomes authoritative"),
		Resolver->BeginSelection(Request, Continuation, PendingAction))) return false;
	TestTrue(TEXT("Resolver reports pending mandatory selection"), Resolver->HasPendingSelection());

	FBufferedEndTurnIntent RejectedCapture;
	TestFalse(TEXT("New EndTurn cannot be captured across mandatory selection"),
		TryCaptureBufferedEndTurnShadow(Fixture.Battle, RejectedCapture));
	TestEqual(TEXT("Older buffered EndTurn becomes stale when mandatory selection appears"),
		EvaluateBufferedEndTurnShadow(Fixture.Battle, Stored),
		EBufferedIntentShadowEvaluation::Stale);
	return true;
}

#endif
