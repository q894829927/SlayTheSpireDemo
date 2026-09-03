#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Phase6UIA2NR4TestTypes.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"
#include "Modifiers/Damage/DamageFlatAddModifier.h"
#include "Modifiers/Damage/DamageRatioModifier.h"
#include "Presentation/PresentationTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"

namespace Phase6UIA3CardPlayedRichHandoff
{
	const FPresentationRecord* FindCardPlayed(const FPresentationResolutionEnvelope& Envelope)
	{
		return Envelope.Records.FindByPredicate(
			[](const FPresentationRecord& Record)
			{
				return Record.Type == EBattlePresentationRecordType::CardPlayed;
			});
	}

	UPhase6UIA2NR4CardProbe* MakeCardProbe(
		UObject* Outer,
		const FBattleHUDCardView& View,
		URichTextBlock*& OutDescription)
	{
		UPhase6UIA2NR4CardProbe* Card = NewObject<UPhase6UIA2NR4CardProbe>(Outer);
		UButton* Button = NewObject<UButton>(Card);
		UTextBlock* Name = NewObject<UTextBlock>(Card);
		UTextBlock* Cost = NewObject<UTextBlock>(Card);
		OutDescription = NewObject<URichTextBlock>(Card);
		UTextBlock* Type = NewObject<UTextBlock>(Card);
		UImage* Art = NewObject<UImage>(Card);
		Card->ConfigureSurfaces(Button, Name, Cost, OutDescription, Type, Art);
		Card->SetCardView(View);
		return Card;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA3CardPlayedRichHandoffTest,
	"SlayTheSpireDemo.UIA3.CardPlayedRichHandoff.StrengthAndVulnerableStayFrozen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA3CardPlayedRichHandoffTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA3CardPlayedRichHandoff;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World"), World)) return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, Params);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(
		ACombatant::StaticClass(),
		FTransform(FVector(100.0, 0.0, 0.0)),
		Params);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("Player"), Player)
		|| !TestNotNull(TEXT("Enemy"), Enemy)
		|| !TestNotNull(TEXT("Battle"), Battle))
	{
		World->DestroyWorld(false);
		return false;
	}

	Player->MaxHP = 100;
	Enemy->MaxHP = 100;
	Player->PresentationId = TEXT("PlayerRichHandoff");
	Enemy->PresentationId = TEXT("EnemyRichHandoff");
	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 1;
	Battle->PlayerTurnDrawCount = 0;
	Battle->EnemyTestAttackDamage = 0;
	Battle->bEnableCommittedPresentationRecording = true;

	UCardData* Strike = NewObject<UCardData>(World);
	Strike->CardId = TEXT("RichHandoffStrike");
	Strike->DisplayName = FText::FromString(TEXT("Strike"));
	Strike->Description = FText::FromString(TEXT("Deal {Damage} damage."));
	Strike->BaseCost = 1;
	Strike->CardType = ECardType::Attack;
	Strike->TargetType = ECardTargetType::Enemy;
	UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Strike);
	Damage->BaseAmount = 6;
	Damage->HitCount = 1;
	Damage->DescriptionArgumentName = TEXT("Damage");
	Strike->Effects.Add(Damage);
	Battle->DebugStartingDeck.Add(Strike);

	TArray<FPresentationResolutionEnvelope> Deliveries;
	Battle->OnPresentationResolutionReady.AddLambda(
		[&Deliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			Deliveries.Add(Envelope);
		});

	Battle->StartBattle();
	Battle->FlushScheduledReadStateReadyForTesting();
	UDeckRuntime* Deck = Battle->GetDeckRuntimeForTesting();
	UCardInstance* Card = IsValid(Deck) ? Deck->GetFirstHandCard() : nullptr;
	if (!TestNotNull(TEXT("Opening Strike"), Card))
	{
		World->DestroyWorld(false);
		return false;
	}

	UStatusData* Strength = NewObject<UStatusData>(World);
	Strength->StatusId = TEXT("StrengthRichHandoff");
	UDamageFlatAddModifier* StrengthModifier = NewObject<UDamageFlatAddModifier>(Strength);
	StrengthModifier->Scope = EModifierScope::Source;
	StrengthModifier->Value = 2;
	StrengthModifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
	Strength->DamageModifiers.Add(StrengthModifier);

	UStatusData* Vulnerable = NewObject<UStatusData>(World);
	Vulnerable->StatusId = TEXT("VulnerableRichHandoff");
	UDamageRatioModifier* VulnerableModifier = NewObject<UDamageRatioModifier>(Vulnerable);
	VulnerableModifier->Scope = EModifierScope::Target;
	VulnerableModifier->Phase = EDamageModifierPhase::TargetMultiplier;
	VulnerableModifier->Numerator = 3;
	VulnerableModifier->Denominator = 2;
	VulnerableModifier->AmountMode = EModifierAmountMode::PresenceOnly;
	Vulnerable->DamageModifiers.Add(VulnerableModifier);

	bool bStrengthCreated = false;
	bool bVulnerableCreated = false;
	TestNotNull(
		TEXT("Strength runtime"),
		Player->GetStatusContainer()->ApplyStatus(
			Strength,
			1,
			Battle->AllocateRuntimeSequence(),
			bStrengthCreated));
	TestNotNull(
		TEXT("Vulnerable runtime"),
		Enemy->GetStatusContainer()->ApplyStatus(
			Vulnerable,
			1,
			Battle->AllocateRuntimeSequence(),
			bVulnerableCreated));
	TestTrue(TEXT("Strength created"), bStrengthCreated);
	TestTrue(TEXT("Vulnerable created"), bVulnerableCreated);

	Deliveries.Reset();
	const FGameplayRequestResult Result = Battle->RequestPlayCard(Card, Enemy);
	TestTrue(TEXT("Enemy-target Strike request accepted"), Result.IsAcceptedForResolution());
	Battle->FlushScheduledReadStateReadyForTesting();
	if (!TestEqual(TEXT("One play resolution delivered"), Deliveries.Num(), 1))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FPresentationRecord* CardPlayed = FindCardPlayed(Deliveries[0]);
	if (!TestNotNull(TEXT("CardPlayed record"), CardPlayed))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(
		TEXT("Stable plain CardPlayed description keeps source-side Strength value"),
		CardPlayed->CardPlayed.Card.Description.ToString(),
		FString(TEXT("Deal 8 damage.")));
	TestEqual(
		TEXT("CardPlayed freezes target-specific Strength + Vulnerable RichText"),
		CardPlayed->CardPlayed.Card.RichDescription.ToString(),
		FString(TEXT("Deal <PreviewIncrease>12</> damage.")));

	UPhase6UIA2APlaybackWidget* PresentationWidget = NewObject<UPhase6UIA2APlaybackWidget>(World);
	if (!TestNotNull(TEXT("Presentation widget probe"), PresentationWidget))
	{
		World->DestroyWorld(false);
		return false;
	}
	const FBattleHUDCardView PresentationView =
		PresentationWidget->MakePresentationCardView(CardPlayed->CardPlayed.Card);
	TestEqual(
		TEXT("Presentation DTO preserves frozen target-specific RichText"),
		PresentationView.RichDescription.ToString(),
		FString(TEXT("Deal <PreviewIncrease>12</> damage.")));

	URichTextBlock* VisibleDescription = nullptr;
	UPhase6UIA2NR4CardProbe* CardProbe =
		MakeCardProbe(PresentationWidget, PresentationView, VisibleDescription);
	TestNotNull(TEXT("Presentation card probe"), CardProbe);
	if (TestNotNull(TEXT("Presentation RichText surface"), VisibleDescription))
	{
		TestEqual(
			TEXT("A2 presentation card visibly keeps resolved value and color tag"),
			VisibleDescription->GetText().ToString(),
			FString(TEXT("Deal <PreviewIncrease>12</> damage.")));
	}

	World->DestroyWorld(false);
	return true;
}

#endif
