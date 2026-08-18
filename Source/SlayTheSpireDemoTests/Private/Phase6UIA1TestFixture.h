#pragma once

#include "Misc/AutomationTest.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA1Test
{
	inline UCardData* CreateCard(UObject* Outer, const TCHAR* CardId, ECardTargetType TargetType, int32 Cost)
	{
		UCardData* Card = NewObject<UCardData>(Outer);
		Card->CardId = FName(CardId);
		Card->DisplayName = FText::FromString(CardId);
		Card->TargetType = TargetType;
		Card->BaseCost = Cost;
		Card->DefaultDestination = ECardDestination::Discard;
		return Card;
	}

	struct FHUDTestFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;

		FHUDTestFixture(ECardTargetType TargetType = ECardTargetType::Enemy, int32 Cost = 0, int32 EnemyDamage = 5)
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
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 1;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = EnemyDamage;
			Battle->DeckDebugSeed = 1337;
			Battle->DebugStartingDeck.Add(CreateCard(World, TEXT("HUDCard"), TargetType, Cost));
			Battle->StartBattle();
		}

		~FHUDTestFixture()
		{
			if (IsValid(ViewModel)) ViewModel->Shutdown();
			if (IsValid(World)) World->DestroyWorld(false);
		}

		void DrainInitialReady()
		{
			if (IsValid(Battle)) Battle->FlushScheduledReadStateReadyForTesting();
		}

		bool InitializeViewModel()
		{
			if (!IsValid(World) || !IsValid(Battle)) return false;
			ViewModel = NewObject<UBattleHUDViewModel>(World);
			return IsValid(ViewModel) && ViewModel->Initialize(Battle);
		}

		void FlushReady()
		{
			if (IsValid(Battle)) Battle->FlushScheduledReadStateReadyForTesting();
		}

		int32 FirstRuntimeId() const
		{
			return IsValid(ViewModel) && ViewModel->HandCards.Num() > 0
				? ViewModel->HandCards[0].RuntimeId
				: INDEX_NONE;
		}

		UCardInstance* FirstAuthoritativeHandCard() const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			return IsValid(Deck) ? Deck->GetFirstHandCard() : nullptr;
		}
	};

	inline bool RequireFixture(FAutomationTestBase& Test, const FHUDTestFixture& Fixture)
	{
		if (IsValid(Fixture.World) && IsValid(Fixture.Player) && IsValid(Fixture.Enemy) &&
			IsValid(Fixture.Battle) && IsValid(Fixture.ViewModel))
		{
			return true;
		}
		Test.AddError(TEXT("Failed to create Phase 6UI-A1 HUD fixture."));
		return false;
	}
}
