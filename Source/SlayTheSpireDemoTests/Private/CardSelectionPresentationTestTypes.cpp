#include "CardSelectionPresentationTestTypes.h"

#include "Phase6UIA2NR8TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"
#include "Actions/DeferredSelectionAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardPlayContext.h"
#include "Selection/SelectionCandidateSource.h"
#include "CardExpansionWave1CTestTypes.h"

void USelectionNoDestinationEffectProbe::BuildActions(const FCardPlayContext& Context, TArray<UBattleAction*>& OutActions) const
{
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Context.ActionOuter);
	Continuation->Configure(nullptr, TEXT("G5NoDestination"), 0);
	UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(Context.ActionOuter);
	Source->Initialize(Context.Deck);
	FSelectionInteractiveBoundaryAccess Boundary;
	Boundary.BindUObject(Context.Battle, &ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary);
	UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Context.ActionOuter);
	Action->Initialize(Source, Context.Battle->GetSelectionResolver(), Continuation, 1,
		ESelectionCancelPolicy::Forbidden, TEXT("G5NoDestination"), EDeferredSelectionMode::Player, Boundary);
	OutActions.Add(Action);
}

void UCardSelectionPresentationHUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UCardSelectionPresentationHUDProbe::ConfigureSelectionSurfaces(
	UBattleHUDViewModel* InViewModel,
	UHorizontalBox* InHand,
	UOverlay* InPlayArea,
	UTextBlock* InDrawCount,
	UTextBlock* InDiscardCount,
	UTextBlock* InExhaustCount)
{
	ViewModel = InViewModel;
	HB_Hand = InHand;
	OV_PlayArea = InPlayArea;
	Txt_DrawCount = InDrawCount;
	Txt_DiscardCount = InDiscardCount;
	Txt_ExhaustCount = InExhaustCount;
	CardWidgetClass = UPhase6UIA2NR8CardProbe::StaticClass();
}

void UCardSelectionPresentationHUDProbe::InvokeNativeTickForTesting(float DeltaSeconds)
{
	NativeTick(GetCachedGeometry(), DeltaSeconds);
}

UWorld* UCardSelectionPresentationHUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

void UCardSelectionPresentationHUDProbe::BindConfirmButtonForTesting(UButton* Button)
{
	Btn_Confirm = Button;
	NativeConstruct();
}

void UCardSelectionPresentationHUDProbe::ConfigureSelectionCanvasForTesting(UBattleHUDViewModel* InViewModel)
{
	WidgetTree = NewObject<UWidgetTree>(this);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	HB_Hand = WidgetTree->ConstructWidget<UHorizontalBox>();
	OV_PlayArea = WidgetTree->ConstructWidget<UOverlay>();
	Txt_DrawCount = WidgetTree->ConstructWidget<UTextBlock>();
	Txt_DiscardCount = WidgetTree->ConstructWidget<UTextBlock>();
	Txt_ExhaustCount = WidgetTree->ConstructWidget<UTextBlock>();
	Root->AddChild(HB_Hand);
	Root->AddChild(OV_PlayArea);
	CardWidgetClass = UPhase6UIA2NR8CardProbe::StaticClass();
	bSyntheticSelectionGeometry = true;
	// Use the production fan host while preserving headless geometry injection.
	EnsureHandInteractionSurfaces();
	SetViewModel(InViewModel);
}
