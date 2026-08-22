#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/ApplyStatusCardEffect.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5CardStatusIntegrationTest
{
	using namespace Phase6UIA2D5Test;

	UStatusData* CreateStatusDefinition(
		UObject* Outer,
		FName StatusId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const FVector2D& UVOffset
	)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		Definition->StatusId = StatusId;
		Definition->DisplayName = FText::FromString(DisplayName);
		Definition->Description = FText::FromString(Description);
		Definition->IconRegion.bUseAtlasIcon = true;
		Definition->IconRegion.UVOffset = UVOffset;
		Definition->IconRegion.UVScale = FVector2D(0.125, 0.25);
		Definition->IconRegion.TrimOffset = FVector2D(0.1, 0.2);
		Definition->IconRegion.TrimScale = FVector2D(0.8, 0.7);
		return Definition;
	}

	UCardData* CreateIntegrationCard(
		UObject* Outer,
		UStatusData* Weak,
		UStatusData* Vulnerable
	)
	{
		if (!IsValid(Outer) || !IsValid(Weak) || !IsValid(Vulnerable))
		{
			return nullptr;
		}

		UCardData* Card = NewObject<UCardData>(Outer);
		if (!IsValid(Card))
		{
			return nullptr;
		}

		Card->CardId = TEXT("A2D5CardStatus");
		Card->DisplayName = FText::FromString(TEXT("A2D5 Card Status"));
		Card->Description = FText::FromString(TEXT("Deal {Damage} damage. Apply {WeakAmount} Weak and {VulnerableAmount} Vulnerable."));
		Card->CardType = ECardType::Attack;
		Card->TargetType = ECardTargetType::Enemy;
		Card->BaseCost = 1;
		Card->DefaultDestination = ECardDestination::Discard;

		UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Card);
		UApplyStatusCardEffect* ApplyWeak = NewObject<UApplyStatusCardEffect>(Card);
		UApplyStatusCardEffect* ApplyVulnerable = NewObject<UApplyStatusCardEffect>(Card);
		if (!IsValid(Damage) || !IsValid(ApplyWeak) || !IsValid(ApplyVulnerable))
		{
			return nullptr;
		}

		Damage->DescriptionArgumentName = TEXT("Damage");
		Damage->BaseAmount = 7;
		Damage->HitCount = 1;
		Damage->DamageKind = EDamageKind::Attack;

		ApplyWeak->DescriptionArgumentName = TEXT("WeakAmount");
		ApplyWeak->StatusDefinition = Weak;
		ApplyWeak->Amount = 2;

		ApplyVulnerable->DescriptionArgumentName = TEXT("VulnerableAmount");
		ApplyVulnerable->StatusDefinition = Vulnerable;
		ApplyVulnerable->Amount = 1;

		Card->Effects.Add(Damage);
		Card->Effects.Add(ApplyWeak);
		Card->Effects.Add(ApplyVulnerable);
		return Card;
	}

	UStatusInstance* FindMutableStatus(UStatusContainer* Container, FName StatusId)
	{
		if (!IsValid(Container))
		{
			return nullptr;
		}
		for (const TObjectPtr<UStatusInstance>& Status : Container->GetStatuses())
		{
			if (IsValid(Status.Get()) && Status->GetStatusId() == StatusId)
			{
				return Status.Get();
			}
		}
		return nullptr;
	}

	const FBattleHUDStatusView* FindDisplayedStatus(
		const UBattleHUDViewModel* ViewModel,
		FName StatusId
	)
	{
		if (!IsValid(ViewModel))
		{
			return nullptr;
		}
		return ViewModel->Enemy.Statuses.FindByPredicate(
			[StatusId](const FBattleHUDStatusView& Status)
			{
				return Status.StatusId == StatusId;
			}
		);
	}

	int32 CountRecords(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		int32 Count = 0;
		for (const FPresentationRecord& Record : Envelope.Records)
		{
			if (Record.Type == Type)
			{
				++Count;
			}
		}
		return Count;
	}

	bool AssertRecordType(
		FAutomationTestBase& Test,
		const FPresentationResolutionEnvelope& Envelope,
		int32 Index,
		EBattlePresentationRecordType ExpectedType,
		const FString& Context
	)
	{
		if (!Envelope.Records.IsValidIndex(Index))
		{
			Test.AddError(FString::Printf(TEXT("%s missing Record[%d]."), *Context, Index));
			return false;
		}
		return Test.TestTrue(
			*FString::Printf(TEXT("%s Record[%d] type"), *Context, Index),
			Envelope.Records[Index].Type == ExpectedType
		);
	}

	bool AssertStatusRecord(
		FAutomationTestBase& Test,
		const FPresentationRecord& Record,
		FName ExpectedStatusId,
		EStatusChangeReason ExpectedReason,
		int32 ExpectedBefore,
		int32 ExpectedAfter,
		int64 ExpectedRuntimeSequence,
		const FString& Context
	)
	{
		bool bOk = true;
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s type"), *Context), Record.Type == EBattlePresentationRecordType::StatusChanged);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s StatusId"), *Context), Record.StatusChanged.StatusId, ExpectedStatusId);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s Source"), *Context), Record.StatusChanged.SourcePresentationId, FName(TEXT("PlayerHero")));
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s Target"), *Context), Record.StatusChanged.TargetPresentationId, FName(TEXT("EnemyPrimary")));
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s Reason"), *Context), Record.StatusChanged.Reason == ExpectedReason);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s AmountBefore"), *Context), Record.StatusChanged.AmountBefore, ExpectedBefore);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s AmountAfter"), *Context), Record.StatusChanged.AmountAfter, ExpectedAfter);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s RuntimeSequence"), *Context), Record.StatusChanged.RuntimeSequence, ExpectedRuntimeSequence);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s bCreated"), *Context),
			Record.StatusChanged.bCreated,
			ExpectedReason == EStatusChangeReason::Applied
		);
		bOk &= Test.TestFalse(*FString::Printf(TEXT("%s bRemoved"), *Context), Record.StatusChanged.bRemoved);
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D5CardStatusIntegrationTest,
	"SlayTheSpireDemo.Phase6UIA2D5.CardStatusIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5CardStatusIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5CardStatusIntegrationTest;

	FAcceptanceFixture Fixture;
	UStatusData* WeakDefinition = CreateStatusDefinition(
		Fixture.World,
		TEXT("Weak"),
		TEXT("Weak Integration"),
		TEXT("Weak amount {Amount}."),
		FVector2D(0.25, 0.5)
	);
	UStatusData* VulnerableDefinition = CreateStatusDefinition(
		Fixture.World,
		TEXT("Vulnerable"),
		TEXT("Vulnerable Integration"),
		TEXT("Vulnerable amount {Amount}."),
		FVector2D(0.5, 0.5)
	);
	UCardData* CardDefinition = CreateIntegrationCard(Fixture.World, WeakDefinition, VulnerableDefinition);
	if (!TestNotNull(TEXT("Weak definition"), WeakDefinition)
		|| !TestNotNull(TEXT("Vulnerable definition"), VulnerableDefinition)
		|| !TestNotNull(TEXT("Integration card definition"), CardDefinition))
	{
		return false;
	}

	TArray<UCardData*> DeckDefinitions;
	DeckDefinitions.Add(CardDefinition);
	DeckDefinitions.Add(CardDefinition);
	if (!TestTrue(TEXT("Acceptance fixture starts with two integration cards"), Fixture.Start(DeckDefinitions, 2)))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UStatusContainer* EnemyStatuses = Fixture.Enemy->GetStatusContainer();
	if (!TestNotNull(TEXT("Deck runtime"), Deck)
		|| !TestNotNull(TEXT("Enemy status container"), EnemyStatuses)
		|| !TestEqual(TEXT("Opening hand has two cards"), Deck->GetHandCount(), 2)
		|| !TestEqual(TEXT("Displayed opening hand has two cards"), Fixture.ViewModel->HandCards.Num(), 2))
	{
		return false;
	}

	UCardInstance* FirstCard = Deck->GetHandCards()[0].Get();
	UCardInstance* SecondCard = Deck->GetHandCards()[1].Get();
	if (!TestNotNull(TEXT("First runtime card"), FirstCard)
		|| !TestNotNull(TEXT("Second runtime card"), SecondCard)
		|| !TestTrue(TEXT("Runtime card identities are distinct"), FirstCard != SecondCard))
	{
		return false;
	}
	const int32 FirstRuntimeId = FirstCard->GetRuntimeId();
	const int32 SecondRuntimeId = SecondCard->GetRuntimeId();
	TestTrue(TEXT("Runtime ids are distinct"), FirstRuntimeId != SecondRuntimeId);

	const int32 CapturesBeforeFirstPlay = Fixture.CapturedEnvelopes.Num();
	const FGameplayRequestResult FirstPlay = Fixture.Battle->RequestPlayCard(FirstCard, Fixture.Enemy);
	if (!TestTrue(TEXT("First card play accepted"), FirstPlay.IsAcceptedForResolution()))
	{
		return false;
	}
	Fixture.Flush();
	if (!TestEqual(TEXT("First card play publishes one Envelope"), Fixture.CapturedEnvelopes.Num(), CapturesBeforeFirstPlay + 1))
	{
		return false;
	}

	const FCapturedEnvelope& FirstCapture = Fixture.CapturedEnvelopes.Last();
	const FPresentationResolutionEnvelope& FirstEnvelope = FirstCapture.Envelope;
	if (!TestEqual(TEXT("First card play emits exactly five Records"), FirstEnvelope.Records.Num(), 5))
	{
		return false;
	}
	AssertRecordType(*this, FirstEnvelope, 0, EBattlePresentationRecordType::CardPlayed, TEXT("First play"));
	AssertRecordType(*this, FirstEnvelope, 1, EBattlePresentationRecordType::Damage, TEXT("First play"));
	AssertRecordType(*this, FirstEnvelope, 2, EBattlePresentationRecordType::StatusChanged, TEXT("First play"));
	AssertRecordType(*this, FirstEnvelope, 3, EBattlePresentationRecordType::StatusChanged, TEXT("First play"));
	AssertRecordType(*this, FirstEnvelope, 4, EBattlePresentationRecordType::CardZoneChanged, TEXT("First play"));
	TestEqual(TEXT("First card cost emits no duplicate EnergyChanged"), CountRecords(FirstEnvelope, EBattlePresentationRecordType::EnergyChanged), 0);

	const FPresentationRecord& FirstPlayedRecord = FirstEnvelope.Records[0];
	TestEqual(TEXT("First CardPlayed RuntimeId"), FirstPlayedRecord.CardPlayed.Card.RuntimeId, FirstRuntimeId);
	TestEqual(TEXT("First CardPlayed CardId"), FirstPlayedRecord.CardPlayed.Card.CardId, FName(TEXT("A2D5CardStatus")));
	TestEqual(TEXT("First CardPlayed source"), FirstPlayedRecord.CardPlayed.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("First CardPlayed target"), FirstPlayedRecord.CardPlayed.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("First CardPlayed energy before"), FirstPlayedRecord.CardPlayed.EnergyBefore, 3);
	TestEqual(TEXT("First CardPlayed energy after"), FirstPlayedRecord.CardPlayed.EnergyAfter, 2);
	TestEqual(TEXT("First CardPlayed CostPaid"), FirstPlayedRecord.CardPlayed.CostPaid, 1);

	const FPresentationRecord& FirstDamageRecord = FirstEnvelope.Records[1];
	TestEqual(TEXT("First Damage source"), FirstDamageRecord.Damage.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("First Damage target"), FirstDamageRecord.Damage.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("First Damage HP before"), FirstDamageRecord.Damage.HPBefore, 100);
	TestEqual(TEXT("First Damage HP after"), FirstDamageRecord.Damage.HPAfter, 93);
	TestEqual(TEXT("First Damage HP damage"), FirstDamageRecord.Damage.HPDamage, 7);

	UStatusInstance* WeakAfterFirst = FindMutableStatus(EnemyStatuses, TEXT("Weak"));
	UStatusInstance* VulnerableAfterFirst = FindMutableStatus(EnemyStatuses, TEXT("Vulnerable"));
	if (!TestNotNull(TEXT("Gameplay Weak exists after first play"), WeakAfterFirst)
		|| !TestNotNull(TEXT("Gameplay Vulnerable exists after first play"), VulnerableAfterFirst))
	{
		return false;
	}
	const int64 WeakRuntimeSequence = static_cast<int64>(WeakAfterFirst->GetRuntimeSequence());
	const int64 VulnerableRuntimeSequence = static_cast<int64>(VulnerableAfterFirst->GetRuntimeSequence());
	TestTrue(TEXT("Weak RuntimeSequence positive"), WeakRuntimeSequence > 0);
	TestTrue(TEXT("Vulnerable follows Weak RuntimeSequence"), VulnerableRuntimeSequence > WeakRuntimeSequence);
	AssertStatusRecord(
		*this,
		FirstEnvelope.Records[2],
		TEXT("Weak"),
		EStatusChangeReason::Applied,
		0,
		2,
		WeakRuntimeSequence,
		TEXT("First Weak")
	);
	AssertStatusRecord(
		*this,
		FirstEnvelope.Records[3],
		TEXT("Vulnerable"),
		EStatusChangeReason::Applied,
		0,
		1,
		VulnerableRuntimeSequence,
		TEXT("First Vulnerable")
	);

	const FPresentationRecord& FirstZoneRecord = FirstEnvelope.Records[4];
	TestEqual(TEXT("First finish-card RuntimeId"), FirstZoneRecord.CardZoneChanged.Card.RuntimeId, FirstRuntimeId);
	TestEqual(TEXT("First finish-card FromZone"), FirstZoneRecord.CardZoneChanged.FromZone, ECardZone::PlayArea);
	TestEqual(TEXT("First finish-card ToZone"), FirstZoneRecord.CardZoneChanged.ToZone, ECardZone::DiscardPile);
	TestEqual(TEXT("No Hand->PlayArea CardZoneChanged is emitted"), CountRecords(FirstEnvelope, EBattlePresentationRecordType::CardZoneChanged), 1);

	// Gameplay is already authoritative, but the historical display must remain at
	// the pre-Envelope baseline until the first CardPlayed token completes.
	if (!TestTrue(TEXT("Controller waits on first CardPlayed"), Fixture.Controller->IsWaitingForCompletionForTesting())
		|| !TestEqual(TEXT("Only first CardPlayed reached widget initially"), Fixture.Widget->PlayCallCount, 1))
	{
		return false;
	}
	TestEqual(TEXT("Gameplay energy already committed"), Fixture.Battle->Energy, 2);
	TestEqual(TEXT("Gameplay enemy HP already committed"), Fixture.Enemy->HP, 93);
	TestEqual(TEXT("Gameplay first card already discarded"), Deck->GetDiscardCount(), 1);
	TestEqual(TEXT("Gameplay Weak already amount two"), WeakAfterFirst->GetAmount(), 2);
	TestEqual(TEXT("Gameplay Vulnerable already amount one"), VulnerableAfterFirst->GetAmount(), 1);
	TestEqual(TEXT("Displayed energy remains baseline before CardPlayed completion"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("Displayed hand remains baseline before CardPlayed completion"), Fixture.ViewModel->HandCards.Num(), 2);
	TestEqual(TEXT("Displayed enemy HP remains baseline before CardPlayed completion"), Fixture.ViewModel->Enemy.HP, 100);
	TestEqual(TEXT("Displayed discard remains baseline before CardPlayed completion"), Fixture.ViewModel->DiscardCount, 0);
	TestEqual(TEXT("Displayed statuses remain absent before CardPlayed completion"), Fixture.ViewModel->Enemy.Statuses.Num(), 0);

	if (!TestTrue(TEXT("Complete first CardPlayed"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Damage becomes next visible record"), Fixture.Widget->PlayedRecords.Last().Type, EBattlePresentationRecordType::Damage);
	TestEqual(TEXT("Displayed energy advances on CardPlayed"), Fixture.ViewModel->Energy, 2);
	TestEqual(TEXT("Displayed hand removes played card on CardPlayed"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("Displayed enemy HP waits for Damage"), Fixture.ViewModel->Enemy.HP, 100);
	TestEqual(TEXT("Displayed discard waits for finish-card"), Fixture.ViewModel->DiscardCount, 0);

	if (!TestTrue(TEXT("Complete first Damage"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Weak becomes next visible record"), Fixture.Widget->PlayedRecords.Last().Type, EBattlePresentationRecordType::StatusChanged);
	TestEqual(TEXT("Displayed enemy HP advances on Damage"), Fixture.ViewModel->Enemy.HP, 93);
	TestTrue(TEXT("Displayed Weak still absent before its record completes"), FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak")) == nullptr);

	if (!TestTrue(TEXT("Complete first Weak"), Fixture.CompleteCurrentPlayback())) return false;
	const FBattleHUDStatusView* DisplayedWeakFirst = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak appears after Weak record"), DisplayedWeakFirst)) return false;
	TestEqual(TEXT("Displayed Weak amount after first record"), DisplayedWeakFirst->Amount, 2);
	TestEqual(TEXT("Displayed Weak concrete identity"), DisplayedWeakFirst->RuntimeSequence, WeakRuntimeSequence);
	TestTrue(TEXT("Displayed Vulnerable still absent before its record completes"), FindDisplayedStatus(Fixture.ViewModel, TEXT("Vulnerable")) == nullptr);

	if (!TestTrue(TEXT("Complete first Vulnerable"), Fixture.CompleteCurrentPlayback())) return false;
	const FBattleHUDStatusView* DisplayedVulnerableFirst = FindDisplayedStatus(Fixture.ViewModel, TEXT("Vulnerable"));
	if (!TestNotNull(TEXT("Displayed Vulnerable appears after its record"), DisplayedVulnerableFirst)) return false;
	TestEqual(TEXT("Displayed Vulnerable amount after first record"), DisplayedVulnerableFirst->Amount, 1);
	TestEqual(TEXT("Displayed Vulnerable concrete identity"), DisplayedVulnerableFirst->RuntimeSequence, VulnerableRuntimeSequence);
	TestEqual(TEXT("Finish-card zone becomes next visible record"), Fixture.Widget->PlayedRecords.Last().Type, EBattlePresentationRecordType::CardZoneChanged);
	TestEqual(TEXT("Displayed discard still waits for finish-card completion"), Fixture.ViewModel->DiscardCount, 0);

	if (!TestTrue(TEXT("Complete first finish-card"), Fixture.CompleteCurrentPlayback())) return false;
	if (!TestTrue(TEXT("First playback is fully caught up"), Fixture.DrainPlayback())) return false;
	TestEqual(TEXT("Displayed discard advances after finish-card"), Fixture.ViewModel->DiscardCount, 1);
	TestEqual(TEXT("Displayed hand after first play"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("First FinalSnapshot energy"), FirstEnvelope.FinalSnapshot.Energy, 2);
	TestEqual(TEXT("First FinalSnapshot hand count"), FirstEnvelope.FinalSnapshot.HandCards.Num(), 1);
	TestEqual(TEXT("First FinalSnapshot discard count"), FirstEnvelope.FinalSnapshot.DiscardCount, 1);
	TestEqual(TEXT("First FinalSnapshot enemy HP"), FirstEnvelope.FinalSnapshot.Enemy.HP, 93);
	TestEqual(TEXT("First FinalSnapshot has two statuses"), FirstEnvelope.FinalSnapshot.Enemy.Statuses.Num(), 2);

	if (!TestTrue(
		TEXT("First Envelope reducer-owned state matches FinalSnapshot"),
		AssertReducerOwnedStateMatchesFinalSnapshot(
			*this,
			FirstCapture.Baseline,
			FirstEnvelope,
			TEXT("CardStatusIntegration First")
		)
	))
	{
		return false;
	}

	if (!TestEqual(TEXT("One card remains authoritative after first play"), Deck->GetHandCount(), 1)) return false;
	UCardInstance* RemainingCard = Deck->GetFirstHandCard();
	if (!TestNotNull(TEXT("Second card remains in hand"), RemainingCard)) return false;
	TestEqual(TEXT("Second runtime card remains"), RemainingCard->GetRuntimeId(), SecondRuntimeId);

	const int32 CapturesBeforeSecondPlay = Fixture.CapturedEnvelopes.Num();
	const int32 PlayedBeforeSecondPlay = Fixture.Widget->PlayCallCount;
	const FGameplayRequestResult SecondPlay = Fixture.Battle->RequestPlayCard(RemainingCard, Fixture.Enemy);
	if (!TestTrue(TEXT("Second card play accepted"), SecondPlay.IsAcceptedForResolution()))
	{
		return false;
	}
	Fixture.Flush();
	if (!TestEqual(TEXT("Second card play publishes one Envelope"), Fixture.CapturedEnvelopes.Num(), CapturesBeforeSecondPlay + 1))
	{
		return false;
	}

	const FCapturedEnvelope& SecondCapture = Fixture.CapturedEnvelopes.Last();
	const FPresentationResolutionEnvelope& SecondEnvelope = SecondCapture.Envelope;
	if (!TestEqual(TEXT("Second card play emits exactly five Records"), SecondEnvelope.Records.Num(), 5))
	{
		return false;
	}
	AssertRecordType(*this, SecondEnvelope, 0, EBattlePresentationRecordType::CardPlayed, TEXT("Second play"));
	AssertRecordType(*this, SecondEnvelope, 1, EBattlePresentationRecordType::Damage, TEXT("Second play"));
	AssertRecordType(*this, SecondEnvelope, 2, EBattlePresentationRecordType::StatusChanged, TEXT("Second play"));
	AssertRecordType(*this, SecondEnvelope, 3, EBattlePresentationRecordType::StatusChanged, TEXT("Second play"));
	AssertRecordType(*this, SecondEnvelope, 4, EBattlePresentationRecordType::CardZoneChanged, TEXT("Second play"));
	TestEqual(TEXT("Second card cost emits no duplicate EnergyChanged"), CountRecords(SecondEnvelope, EBattlePresentationRecordType::EnergyChanged), 0);

	TestEqual(TEXT("Second CardPlayed RuntimeId"), SecondEnvelope.Records[0].CardPlayed.Card.RuntimeId, SecondRuntimeId);
	TestEqual(TEXT("Second CardPlayed energy before"), SecondEnvelope.Records[0].CardPlayed.EnergyBefore, 2);
	TestEqual(TEXT("Second CardPlayed energy after"), SecondEnvelope.Records[0].CardPlayed.EnergyAfter, 1);
	TestEqual(TEXT("Second CardPlayed CostPaid"), SecondEnvelope.Records[0].CardPlayed.CostPaid, 1);
	TestEqual(TEXT("Second Damage HP before"), SecondEnvelope.Records[1].Damage.HPBefore, 93);
	TestEqual(TEXT("Second Damage HP after"), SecondEnvelope.Records[1].Damage.HPAfter, 86);
	TestEqual(TEXT("Second Damage HP damage"), SecondEnvelope.Records[1].Damage.HPDamage, 7);

	UStatusInstance* WeakAfterSecond = FindMutableStatus(EnemyStatuses, TEXT("Weak"));
	UStatusInstance* VulnerableAfterSecond = FindMutableStatus(EnemyStatuses, TEXT("Vulnerable"));
	if (!TestNotNull(TEXT("Gameplay Weak survives second application"), WeakAfterSecond)
		|| !TestNotNull(TEXT("Gameplay Vulnerable survives second application"), VulnerableAfterSecond))
	{
		return false;
	}
	TestTrue(TEXT("Weak reapplication keeps exact runtime object"), WeakAfterSecond == WeakAfterFirst);
	TestTrue(TEXT("Vulnerable reapplication keeps exact runtime object"), VulnerableAfterSecond == VulnerableAfterFirst);
	TestEqual(TEXT("Weak reapplication keeps RuntimeSequence"), static_cast<int64>(WeakAfterSecond->GetRuntimeSequence()), WeakRuntimeSequence);
	TestEqual(TEXT("Vulnerable reapplication keeps RuntimeSequence"), static_cast<int64>(VulnerableAfterSecond->GetRuntimeSequence()), VulnerableRuntimeSequence);
	TestEqual(TEXT("Gameplay Weak increases to four"), WeakAfterSecond->GetAmount(), 4);
	TestEqual(TEXT("Gameplay Vulnerable increases to two"), VulnerableAfterSecond->GetAmount(), 2);

	AssertStatusRecord(
		*this,
		SecondEnvelope.Records[2],
		TEXT("Weak"),
		EStatusChangeReason::Increased,
		2,
		4,
		WeakRuntimeSequence,
		TEXT("Second Weak")
	);
	AssertStatusRecord(
		*this,
		SecondEnvelope.Records[3],
		TEXT("Vulnerable"),
		EStatusChangeReason::Increased,
		1,
		2,
		VulnerableRuntimeSequence,
		TEXT("Second Vulnerable")
	);
	TestEqual(TEXT("Second finish-card FromZone"), SecondEnvelope.Records[4].CardZoneChanged.FromZone, ECardZone::PlayArea);
	TestEqual(TEXT("Second finish-card ToZone"), SecondEnvelope.Records[4].CardZoneChanged.ToZone, ECardZone::DiscardPile);

	TestEqual(TEXT("Second play starts exactly one new visible playback"), Fixture.Widget->PlayCallCount, PlayedBeforeSecondPlay + 1);
	TestEqual(TEXT("Displayed energy remains at first-play history while second CardPlayed is active"), Fixture.ViewModel->Energy, 2);
	TestEqual(TEXT("Displayed enemy HP remains at first-play history while second CardPlayed is active"), Fixture.ViewModel->Enemy.HP, 93);
	const FBattleHUDStatusView* DisplayedWeakBeforeSecondDrain = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	const FBattleHUDStatusView* DisplayedVulnerableBeforeSecondDrain = FindDisplayedStatus(Fixture.ViewModel, TEXT("Vulnerable"));
	if (!TestNotNull(TEXT("Displayed Weak remains before second drain"), DisplayedWeakBeforeSecondDrain)
		|| !TestNotNull(TEXT("Displayed Vulnerable remains before second drain"), DisplayedVulnerableBeforeSecondDrain))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak has not advanced early"), DisplayedWeakBeforeSecondDrain->Amount, 2);
	TestEqual(TEXT("Displayed Vulnerable has not advanced early"), DisplayedVulnerableBeforeSecondDrain->Amount, 1);

	if (!TestTrue(TEXT("Second card playback drains"), Fixture.DrainPlayback()))
	{
		return false;
	}
	const FBattleHUDStatusView* DisplayedWeakFinal = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	const FBattleHUDStatusView* DisplayedVulnerableFinal = FindDisplayedStatus(Fixture.ViewModel, TEXT("Vulnerable"));
	if (!TestNotNull(TEXT("Displayed Weak final row"), DisplayedWeakFinal)
		|| !TestNotNull(TEXT("Displayed Vulnerable final row"), DisplayedVulnerableFinal))
	{
		return false;
	}
	TestEqual(TEXT("Displayed status row count remains two"), Fixture.ViewModel->Enemy.Statuses.Num(), 2);
	TestEqual(TEXT("Displayed Weak final amount"), DisplayedWeakFinal->Amount, 4);
	TestEqual(TEXT("Displayed Weak final RuntimeSequence"), DisplayedWeakFinal->RuntimeSequence, WeakRuntimeSequence);
	TestEqual(TEXT("Displayed Vulnerable final amount"), DisplayedVulnerableFinal->Amount, 2);
	TestEqual(TEXT("Displayed Vulnerable final RuntimeSequence"), DisplayedVulnerableFinal->RuntimeSequence, VulnerableRuntimeSequence);
	TestEqual(TEXT("Displayed final energy"), Fixture.ViewModel->Energy, 1);
	TestEqual(TEXT("Displayed final hand empty"), Fixture.ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Displayed final discard count"), Fixture.ViewModel->DiscardCount, 2);
	TestEqual(TEXT("Displayed final enemy HP"), Fixture.ViewModel->Enemy.HP, 86);

	TestEqual(TEXT("Second FinalSnapshot energy"), SecondEnvelope.FinalSnapshot.Energy, 1);
	TestEqual(TEXT("Second FinalSnapshot hand empty"), SecondEnvelope.FinalSnapshot.HandCards.Num(), 0);
	TestEqual(TEXT("Second FinalSnapshot discard count"), SecondEnvelope.FinalSnapshot.DiscardCount, 2);
	TestEqual(TEXT("Second FinalSnapshot enemy HP"), SecondEnvelope.FinalSnapshot.Enemy.HP, 86);
	TestEqual(TEXT("Second FinalSnapshot status row count"), SecondEnvelope.FinalSnapshot.Enemy.Statuses.Num(), 2);
	if (SecondEnvelope.FinalSnapshot.Enemy.Statuses.Num() == 2)
	{
		TestEqual(TEXT("FinalSnapshot Weak first by RuntimeSequence"), SecondEnvelope.FinalSnapshot.Enemy.Statuses[0].StatusId, FName(TEXT("Weak")));
		TestEqual(TEXT("FinalSnapshot Vulnerable second by RuntimeSequence"), SecondEnvelope.FinalSnapshot.Enemy.Statuses[1].StatusId, FName(TEXT("Vulnerable")));
		TestTrue(
			TEXT("FinalSnapshot status order remains RuntimeSequence ascending"),
			SecondEnvelope.FinalSnapshot.Enemy.Statuses[0].RuntimeSequence
				< SecondEnvelope.FinalSnapshot.Enemy.Statuses[1].RuntimeSequence
		);
	}

	TestTrue(
		TEXT("Captured Envelope order remains monotonic"),
		AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("CardStatusIntegration"))
	);
	for (int32 Index = 0; Index < Fixture.CapturedEnvelopes.Num(); ++Index)
	{
		const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[Index];
		TestTrue(
			*FString::Printf(TEXT("Envelope[%d] reducer-owned state matches FinalSnapshot"), Index),
			AssertReducerOwnedStateMatchesFinalSnapshot(
				*this,
				Capture.Baseline,
				Capture.Envelope,
				FString::Printf(TEXT("CardStatusIntegration Envelope[%d]"), Index)
			)
		);
	}

	TestTrue(
		TEXT("Controller playback records/tokens match both card Envelopes in producer order"),
		AssertControllerPlaybackMatchesCapturedHistory(
			*this,
			Fixture.CapturedEnvelopes,
			Fixture.Widget,
			TEXT("CardStatusIntegration Controller")
		)
	);
	return true;
}

#endif
