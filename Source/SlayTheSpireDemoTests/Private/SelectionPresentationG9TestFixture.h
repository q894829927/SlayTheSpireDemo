#pragma once
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA0TestTypes.h"
#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleReadSnapshot.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDBufferedPlayerInput.h"

namespace SelectionPresentationG9ATest
{
	struct FShadowFixture
	{
		Phase6UIA1Test::FHUDTestFixture Gameplay{ECardTargetType::None, 0, 0};
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;

		explicit FShadowFixture(bool bRecorded = true, bool bMandatoryA = false, ECardTargetType BTarget = ECardTargetType::None, bool bMandatoryB = false)
		{
			ABattleManager* Battle = Gameplay.Battle;
			if (!IsValid(Battle)) return;
			Battle->bEnableCommittedPresentationRecording = bRecorded;
			Battle->DebugStartingDeck.Reset();
			for (const TCHAR* Name : {TEXT("A"), TEXT("B"), TEXT("C")})
			{
				UCardData* Definition = Phase6UIA1Test::CreateCard(Gameplay.World, Name, FString(Name) == TEXT("B") ? BTarget : ECardTargetType::None, 0);
				if ((bMandatoryA && Definition->CardId == TEXT("A")) || (bMandatoryB && Definition->CardId == TEXT("B")))
				{
					Definition->Effects.Add(NewObject<USelectExhaustHandCardEffect>(Definition));
				}
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->OpeningHandDrawCount = 3;
			Battle->StartBattle();
			Gameplay.DrainInitialReady();
			Gameplay.InitializeViewModel();
			Widget = NewObject<UPhase6UIA2APlaybackWidget>(Gameplay.World);
			Controller = NewObject<UBattlePresentationController>(Gameplay.World);
			Widget->SetViewModel(Gameplay.ViewModel);
			Widget->SetPresentationController(Controller);
			Controller->Initialize(Battle, Gameplay.ViewModel, Widget);
			Input().SetEnabled(true);
		}

		~FShadowFixture()
		{
			if (IsValid(Controller)) Controller->Shutdown();
		}

		FBattleHUDBufferedPlayerInput& Input() { return Widget->GetBufferedPlayerInputShadowForTesting(); }
		UCardInstance* Card(const TCHAR* Name) const
		{
			for (const TObjectPtr<UCardInstance>& Card : Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCards())
			{
				if (Card->GetCardId() == FName(Name)) return Card.Get();
			}
			return nullptr;
		}
		bool PlayA()
		{
			UCardInstance* Played = Card(TEXT("A"));
			if (!Played || !Gameplay.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution()) return false;
			Gameplay.FlushReady();
			return Controller->IsWaitingForCompletionForTesting();
		}
		void FinishPlayback()
		{
			for (int32 Index = 0; Index < 32 && Controller->IsWaitingForCompletionForTesting(); ++Index)
			{
				Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
			}
		}
		UPhase6UIA0ManualFinishAction* HoldQueue()
		{
			UBattleActionQueue* Queue = Gameplay.Battle->GetActionQueueForTesting();
			UPhase6UIA0ManualFinishAction* Action = NewObject<UPhase6UIA0ManualFinishAction>(Queue);
			Queue->AddToBack(Action);
			Queue->StartProcessing();
			return Action;
		}
	};
}


#endif
