#include "SelectionPresentationG9TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Phase6UIA2NR8TestTypes.h"
#include "UI/BattleHandFanPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Engine/GameInstance.h"
#include "Containers/Ticker.h"

using namespace SelectionPresentationG9ATest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BCardReplayTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.CardReplayWithoutSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BCardReplayTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A playback starts"), F.PlayA())) return false;
	const int32 B = F.Card(TEXT("B"))->GetRuntimeId();
	const int32 C = F.Card(TEXT("C"))->GetRuntimeId();
	TestTrue(TEXT("B captures an already sealed target"), F.Widget->TryBufferCardSelection(B));
	TestTrue(TEXT("New physical click captures C"), F.Widget->TryBufferCardSelection(C));
	TestTrue(TEXT("Buffer does not Skip A"), F.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("No early selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	F.FinishPlayback();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Exact ready selects newest card"), F.Gameplay.ViewModel->SelectedCardRuntimeId, C);
	TestEqual(TEXT("Fresh confirmation still required"), F.Gameplay.ViewModel->InteractionState, EBattleHUDInteractionState::ReadyToConfirm);
	TestTrue(TEXT("Card remains in authoritative Hand"), F.Card(TEXT("C")) != nullptr);
	F.Widget->NotifyBufferedPlayerInputReadinessChanged();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Repeated event cannot toggle selection off"), F.Gameplay.ViewModel->SelectedCardRuntimeId, C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BEndTurnPresentationTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurnRetiresCardWithoutSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BEndTurnPresentationTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A playback starts"), F.PlayA())) return false;
	TestTrue(TEXT("Older B buffered"), F.Widget->TryBufferCardSelection(F.Card(TEXT("B"))->GetRuntimeId()));
	const FPresentationPlaybackToken Token = F.Controller->GetActivePlaybackTokenForTesting();
	FPlayerTurnAuthorityToken Before;
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Before);
	TestTrue(TEXT("EndTurn control accepts through Blocking playback"), F.Widget->CanAcceptEndTurnIntent());
	TestTrue(TEXT("Physical EndTurn accepted"), F.Widget->EndTurn());
	TestTrue(TEXT("EndTurn preserves active visual token"), F.Controller->GetActivePlaybackTokenForTesting() == Token);
	F.Gameplay.FlushReady();
	F.FinishPlayback();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	FPlayerTurnAuthorityToken After;
	TestTrue(TEXT("Next player turn reached"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(After));
	TestEqual(TEXT("Exactly one turn transition"), After.PlayerTurnSerial, Before.PlayerTurnSerial + 1);
	TestEqual(TEXT("Older buffered card never selects"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BEndTurnBusyTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurnBusyAndDirectBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BEndTurnBusyTest::RunTest(const FString&)
{
	for (bool bRecorded : {false, true})
	{
		FShadowFixture F(bRecorded);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		UPhase6UIA0ManualFinishAction* Hold = F.HoldQueue();
		FPlayerTurnAuthorityToken Before;
		F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Before);
		TestTrue(TEXT("Busy same-turn intent available"), F.Widget->CanAcceptEndTurnIntent());
		TestTrue(TEXT("Busy EndTurn accepted"), F.Widget->EndTurn());
		TestTrue(TEXT("Exactly one EndTurn stored"), F.Widget->HasAcceptedBufferedEndTurn());
		TestFalse(TEXT("Second physical EndTurn rejected"), F.Widget->EndTurn());
		TestFalse(TEXT("No speculative card buffer during Gameplay busy"), F.Widget->TryBufferCardSelection(F.Card(TEXT("B"))->GetRuntimeId()));
		Hold->CompleteManually();
		F.Gameplay.FlushReady();
		FTSTicker::GetCoreTicker().Tick(0.0f);
		F.Gameplay.FlushReady();
		F.FinishPlayback();
		FPlayerTurnAuthorityToken After;
		TestTrue(TEXT("Same turn command reached next turn"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(After));
		TestEqual(TEXT("Buffered EndTurn executes once"), After.PlayerTurnSerial, Before.PlayerTurnSerial + 1);
		TestFalse(TEXT("Consumed EndTurn storage is empty"), F.Widget->HasAcceptedBufferedEndTurn());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BTransientEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.TransientReplacementAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BTransientEndTurnTest::RunTest(const FString&)
{
	for (ECardTargetType Target : {ECardTargetType::None, ECardTargetType::Enemy})
	{
		FShadowFixture F(false, false, Target);
		const int32 B = F.Card(TEXT("B"))->GetRuntimeId();
		TestTrue(TEXT("Normal transient selection"), F.Gameplay.ViewModel->SelectCardByRuntimeId(B));
		if (Target == ECardTargetType::Enemy)
		{
			TestFalse(TEXT("Disabled G9 preserves G8 rejection"), F.Widget->EndTurn());
			TestEqual(TEXT("Rejected EndTurn preserves selected card"), F.Gameplay.ViewModel->SelectedCardRuntimeId, B);
		}
		F.Widget->SetBufferedPlayerInputEnabled(true);
		const EBattleHUDInteractionState SelectedState = F.Gameplay.ViewModel->InteractionState;
		F.Gameplay.ViewModel->InteractionState = EBattleHUDInteractionState::PresentationUnavailable;
		TestFalse(TEXT("Unsafe authority rejects before transient cancellation"), F.Widget->EndTurn());
		TestEqual(TEXT("Rejected G9 EndTurn preserves selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, B);
		F.Gameplay.ViewModel->InteractionState = SelectedState;
		TestTrue(TEXT("G9 accepts transient replacement"), F.Widget->EndTurn());
		TestEqual(TEXT("Accepted EndTurn clears transient selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
		TestTrue(TEXT("Legal targets retired"), F.Gameplay.ViewModel->LegalTargets.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BStaleAndMandatoryTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.StaleMandatoryAndDisable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BStaleAndMandatoryTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 4; ++Case)
	{
		FShadowFixture F(false, true);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		UPhase6UIA0ManualFinishAction* Hold = F.HoldQueue();
		TestTrue(TEXT("Busy EndTurn accepted"), F.Widget->EndTurn());
		if (Case == 0) F.Widget->SetBufferedPlayerInputEnabled(false);
		if (Case == 1) F.Gameplay.Battle->StartBattle();
		if (Case != 1) Hold->CompleteManually();
		if (Case == 3) TestTrue(TEXT("External turn transition before deferred replay"), F.Gameplay.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
		if (Case == 2)
		{
			TestTrue(TEXT("Real mandatory choice begins before replay"), F.Gameplay.Battle->RequestPlayCard(F.Card(TEXT("A")), nullptr).IsAcceptedForResolution());
			F.Gameplay.FlushReady();
			TestFalse(TEXT("Mandatory choice kills pending EndTurn"), F.Widget->HasAcceptedBufferedEndTurn());
			TestFalse(TEXT("Mandatory choice rejects new EndTurn"), F.Widget->EndTurn());
			TestTrue(TEXT("Resolve through normal selection API"), F.Gameplay.ViewModel->SubmitPendingCardSelectionByRuntimeIds({F.Card(TEXT("B"))->GetRuntimeId()}));
		}
		F.Gameplay.FlushReady();
		FTSTicker::GetCoreTicker().Tick(0.0f);
		TestFalse(TEXT("Old intent cannot resurrect"), F.Widget->HasAcceptedBufferedEndTurn());
		FPlayerTurnAuthorityToken Turn;
		TestTrue(TEXT("Player turn retained"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn));
		TestEqual(TEXT("No old EndTurn fired"), Turn.PlayerTurnSerial, Case == 3 ? uint64(2) : uint64(1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BStableHandTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.StableHandAndHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BStableHandTest::RunTest(const FString&)
{
	FShadowFixture F(false);
	F.Gameplay.World->SetGameInstance(NewObject<UGameInstance>(F.Gameplay.World));
	UClass* NativeClass = LoadClass<UBattleHUDWidget>(nullptr,
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"));
	UBattleHUDWidget* NativeHUD = NativeClass ? CreateWidget<UBattleHUDWidget>(F.Gameplay.World, NativeClass) : nullptr;
	if (!TestNotNull(TEXT("Production Native HUD loads"), NativeHUD)) return false;
	NativeHUD->SetViewModel(F.Gameplay.ViewModel);
	UBattleHandFanPanel* NativeHand = Cast<UBattleHandFanPanel>(NativeHUD->GetWidgetFromName(TEXT("FanHand")));
	if (!TestNotNull(TEXT("Production formal fan exists"), NativeHand)
		|| !TestEqual(TEXT("Production Hand displays all cards"), NativeHand->GetChildrenCount(), 3)) return false;
	TArray<UWidget*> Before = NativeHand->GetAllChildren();
	TestTrue(TEXT("Normal selection publishes Input dirty"), F.Gameplay.ViewModel->SelectCardByRuntimeId(F.Card(TEXT("B"))->GetRuntimeId()));
	for (int32 Index = 0; Index < Before.Num(); ++Index)
		TestTrue(TEXT("Input-only publication preserves every formal Widget"), NativeHand->GetChildAt(Index) == Before[Index]);
	F.Gameplay.ViewModel->CancelSelection();
	// Exercise a live Slate tree: UObject reuse alone cannot prove that a
	// membership update preserves the card face and its Canvas placement.
	TSharedRef<SWidget> LiveHUD = NativeHUD->TakeWidget();
	TObjectPtr<UPanelSlot> SurvivorSlot = Before[1]->Slot;
	TWeakPtr<SWidget> SurvivorSlate = Before[1]->GetCachedWrappedWidget();
	if (!TestTrue(TEXT("Formal card has a live Slate tree"), SurvivorSlate.IsValid())) return false;
	TestTrue(TEXT("Normal Gameplay play removes A"), F.Gameplay.Battle->RequestPlayCard(F.Card(TEXT("A")), nullptr).IsAcceptedForResolution());
	F.Gameplay.FlushReady();
	TestEqual(TEXT("Frozen Hand loses exactly the played card"), NativeHand->GetChildrenCount(), 2);
	TestTrue(TEXT("Remaining B keeps its Canvas slot"), Before[1]->Slot == SurvivorSlot);
	TestTrue(TEXT("Remaining B keeps its live Slate tree"), SurvivorSlate.IsValid()
		&& Before[1]->GetCachedWrappedWidget() == SurvivorSlate.Pin());
	const UCanvasPanelSlot* RemainingSlot = CastChecked<UCanvasPanelSlot>(Before[1]->Slot);
	TestTrue(TEXT("Remaining card retains the full configured size"), RemainingSlot->GetSize() == NativeHUD->HandCardSize);
	TestTrue(TEXT("Remaining cards stay bottom-centered"), RemainingSlot->GetAnchors() == FAnchors(0.5f, 1.0f)
		&& RemainingSlot->GetAlignment() == FVector2D(0.5f, 1.0f));
	UPhase6UIA2NR8HUDProbe* HUD = NewObject<UPhase6UIA2NR8HUDProbe>(F.Gameplay.World);
	HUD->SetTestWorld(F.Gameplay.World);
	UHorizontalBox* Hand = NewObject<UHorizontalBox>(HUD);
	HUD->ConfigureCardSurfaces(Hand, NewObject<UOverlay>(HUD), nullptr, nullptr, nullptr);
	HUD->SetViewModel(F.Gameplay.ViewModel);
	HUD->ReconcileHandForTesting();
	UWidget* Survivor = Hand->GetChildAt(1);
	Survivor->SetVisibility(ESlateVisibility::Hidden);
	Survivor->SetIsEnabled(false);
	HUD->ReconcileHandForTesting();
	TestTrue(TEXT("Same identity keeps exact Widget"), Hand->GetChildAt(1) == Survivor);
	TestEqual(TEXT("Reconcile preserves hidden source"), Survivor->GetVisibility(), ESlateVisibility::Hidden);
	F.Gameplay.ViewModel->HandCards.RemoveAt(0);
	HUD->ReconcileHandForTesting();
	TestTrue(TEXT("Membership change reuses survivor"), Hand->GetChildAt(0) == Survivor);
	UBattleHandFanPanel* Fan = NewObject<UBattleHandFanPanel>();
	Fan->AddChild(Survivor);
	Fan->ReconcileLayout();
	UCanvasPanelSlot* Slot = CastChecked<UCanvasPanelSlot>(Survivor->Slot);
	Slot->SetPosition(FVector2D(123, 456));
	Fan->UpdateHoverAffordance(FVector2D::ZeroVector, INDEX_NONE, true, 1.0f);
	TestTrue(TEXT("Hover cannot relayout structural slot"), Slot->GetPosition() == FVector2D(123, 456));
	TestEqual(TEXT("Hover cannot reveal hidden owner"), Survivor->GetVisibility(), ESlateVisibility::Hidden);
	return true;
}
#endif
