#pragma once
#include "Misc/AutomationTest.h"
#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR8TestTypes.h"
#include "NativeHandLayoutTestUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "Presentation/BattlePresentationController.h"
SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget*, const FGeometry&);

namespace NativePlayedCardTest
{
	struct FFixture
	{
		Phase6UIA1Test::FHUDTestFixture Game{ECardTargetType::None,1};
		UPhase6UIA2NR8HUDProbe* HUD = nullptr;
		UBattleHandFanPanel* Fan = nullptr;
		UBattlePresentationController* Controller = nullptr;
		// The fixture still owns the sender when HUD bindings are deliberately
		// removed and GC runs before late-callback checks (as a real Presenter does).
		TStrongObjectPtr<UBattlePresentationController> ControllerOwner;
		explicit FFixture(bool bEnableD1 = false, int32 HandCount = 1, ECardDestination Destination = ECardDestination::Discard,
			int32 GainBlockAmount = 0, int32 Cost = 1, ECardType CardType = ECardType::Attack)
		{
			Game.World->AddToRoot(); Game.World->SetGameInstance(NewObject<UGameInstance>(Game.World));
			Game.Battle->DebugStartingDeck.Reset();
			for (int32 I=0; I<HandCount; ++I)
			{
				auto* Definition = Phase6UIA1Test::CreateCard(Game.World, *FString::Printf(TEXT("JobCard%d"), I),
					GainBlockAmount > 0 ? ECardTargetType::Self : ECardTargetType::None, Cost, GainBlockAmount);
				Definition->DefaultDestination = Destination;
				Definition->CardType = CardType;
				Game.Battle->DebugStartingDeck.Add(Definition);
			}
			Game.Battle->OpeningHandDrawCount = HandCount;
			Game.Battle->PlayerTurnDrawCount = HandCount > 1 ? HandCount : 0;
			Game.Battle->bEnableCommittedPresentationRecording = true;
			Game.Battle->StartBattle(); Game.DrainInitialReady(); Game.InitializeViewModel();
			HUD = NewObject<UPhase6UIA2NR8HUDProbe>(Game.World); HUD->AddToRoot(); HUD->SetTestWorld(Game.World);
			// Shared C/D1 fixtures retain Blocking arrivals; D2 cases opt in explicitly.
			HUD->SetDetachedCardArrivalD2Enabled(false);
			UOverlay* Play = NewObject<UOverlay>(HUD);
			HUD->ConfigureCardSurfaces(NewObject<UHorizontalBox>(HUD),Play,NewObject<UTextBlock>(HUD),NewObject<UTextBlock>(HUD),NewObject<UTextBlock>(HUD));
			if (GainBlockAmount > 0) HUD->ConfigureBlockForTesting(NewObject<UTextBlock>(HUD));
			Fan = NewObject<UBattleHandFanPanel>(HUD); HUD->ConfigureFanForTesting(Fan); Fan->TakeWidget();
			HUD->SetViewModel(Game.ViewModel);
			const auto Root = FGeometry::MakeRoot(FVector2D(1000,700),FSlateLayoutTransform());
			CacheG9TestWidgetGeometry(HUD->WidgetTree->RootWidget,Root);
			CacheG9TestWidgetGeometry(Fan,Root.MakeChild(FVector2D(800,220),FSlateLayoutTransform(FVector2D(100,480))));
			CacheG9TestWidgetGeometry(Play,Root.MakeChild(FVector2D(400,300),FSlateLayoutTransform(FVector2D(300,100))));
			Controller = NewObject<UBattlePresentationController>(Game.World);
			ControllerOwner.Reset(Controller);
			HUD->SetPresentationController(Controller); Controller->Initialize(Game.Battle,Game.ViewModel,HUD);
			if (bEnableD1) Controller->SetDetachedCardDestinationD1Enabled(true);
		}
		~FFixture()
		{
			Controller->Shutdown(); HUD->InvokeNativeDestructForTesting(); HUD->SetViewModel(nullptr); HUD->RemoveFromRoot();
			Game.World->RemoveFromRoot();
		}
		bool Play()
		{
			auto* Card = Game.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
			if (!Card) return false;
			const bool Accepted = Game.Battle->RequestPlayCard(Card,Card->GetTargetType()==ECardTargetType::Self ? Game.Player : nullptr).IsAcceptedForResolution();
			Game.FlushReady(); return Accepted;
		}
		void Finish() { HUD->InvokeFinishForTesting(Controller->GetActivePlaybackTokenForTesting()); FTSTicker::GetCoreTicker().Tick(0); }
	};
}
