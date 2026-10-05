#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "SelectionPresentationG9TestFixture.h"
#include "CardSelectionPresentationTestTypes.h"
#include "NativeHandLayoutTestUtils.h"
#include "UI/BattleCardWidget.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "UObject/GarbageCollection.h"

// Exported only in editor-test builds; keeps existing dependency boundaries.
SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget* Widget, const FGeometry& Geometry);

namespace G9TurnEndDiscardTest
{
	// Headless fixture injection only: no claim that these cached geometries prove
	// PIE. Card base geometry itself comes from the production Slate fan panel.
	void CacheGeometry(UWidget* Widget, const FGeometry& Geometry)
	{
		CacheG9TestWidgetGeometry(Widget, Geometry);
	}
	struct FNativeFixture
	{
		SelectionPresentationG9ATest::FShadowFixture Source;
		UCardSelectionPresentationHUDProbe* HUD = nullptr;
		UBattleHandFanPanel* Hand = nullptr;
		UOverlay* Area = nullptr;
		UTextBlock* Discard = nullptr;
		FPresentationResolutionEnvelope Captured;
		TArray<UObject*> FixtureRoots;
		FNativeFixture()
		{
			FixtureRoots = {Source.Gameplay.World, Source.Gameplay.Player, Source.Gameplay.Enemy,
				Source.Gameplay.Battle, Source.Gameplay.ViewModel, Source.Controller, Source.Widget};
			for (UObject* Object : FixtureRoots) Object->AddToRoot();
			HUD = NewObject<UCardSelectionPresentationHUDProbe>(Source.Gameplay.World);
			HUD->AddToRoot();
			HUD->SetTestWorld(Source.Gameplay.World);
			Hand = NewObject<UBattleHandFanPanel>(HUD);
			Area = NewObject<UOverlay>(HUD);
			Discard = NewObject<UTextBlock>(HUD);
			HUD->ConfigureSelectionSurfaces(Source.Gameplay.ViewModel, Hand, Area,
				NewObject<UTextBlock>(HUD), Discard, NewObject<UTextBlock>(HUD));
			HUD->SetPresentationController(Source.Controller);
			Source.Controller->SetWidget(HUD);
			HUD->SetBufferedPlayerInputEnabled(true);
			HUD->ReconcileHandForTesting();
			Layout(FVector2D(1280, 210));
			Source.Gameplay.Battle->OnPresentationResolutionReady.AddLambda([this](const auto& Envelope) { Captured = Envelope; });
		}
		~FNativeFixture()
		{
			HUD->SkipPresentation(); HUD->RemoveFromRoot();
			for (UObject* Object : FixtureRoots) Object->RemoveFromRoot();
		}
		void Layout(FVector2D Size)
		{
			const auto Cards = NativeHandLayoutTest::Arrange(Hand, Size);
			for (const auto& Card : Cards) CacheGeometry(Card.Key, Card.Value);
			CacheGeometry(Area, FGeometry::MakeRoot(FVector2D(Size.X, 720), FSlateLayoutTransform()));
			CacheGeometry(Discard, FGeometry::MakeRoot(FVector2D(40, 40), FSlateLayoutTransform(FVector2D(Size.X - 80, 620))));
		}
		bool EndTurn()
		{
			if (!Source.Gameplay.Battle->RequestEndPlayerTurn().IsAcceptedForResolution()) return false;
			Source.Gameplay.FlushReady();
			if (Captured.PresentationGroups.Num() != 1) return false;
			int64 LeaderSequence = MAX_int64;
			for (const auto& Record : Captured.Records)
				if (Record.Group.Kind == EPresentationGroupKind::TurnEndDiscard) { LeaderSequence = Record.PresentationSequence; break; }
			for (int32 Index = 0; Index < 8 && Source.Controller->IsWaitingForCompletionForTesting(); ++Index)
			{
				const auto Token = Source.Controller->GetActivePlaybackTokenForTesting();
				if (Token.PresentationSequence >= LeaderSequence) break;
				HUD->FinishNativeForTesting(Token);
				FTSTicker::GetCoreTicker().Tick(0.0f);
			}
			return Captured.PresentationGroups.Num() == 1;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9TurnEndNativeTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.TurnEndDiscard.NativeHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9TurnEndNativeTest::RunTest(const FString& Parameters)
{
	using namespace G9TurnEndDiscardTest;
	for (int32 Mode = 0; Mode < 8; ++Mode)
	{
		FNativeFixture Fixture;
		const auto Frozen = Fixture.Source.Gameplay.ViewModel->HandCards;
		TArray<UWidget*> Originals = Fixture.Hand->GetAllChildren();
		const FGeometry SourceGeometry = Originals[0]->GetCachedGeometry();
		const FVector2D FrozenSourceCenter = SourceGeometry.LocalToAbsolute(SourceGeometry.GetLocalSize() * 0.5f);
		if (Mode == 3) CacheGeometry(Originals[2], FGeometry::MakeRoot(FVector2D::ZeroVector, FSlateLayoutTransform())); // all-or-nothing decline
		if (!TestTrue(TEXT("Turn request and explicit group declaration"), Fixture.EndTurn())) return false;
		const auto& Declaration = Fixture.Captured.PresentationGroups[0];
		TestEqual(TEXT("Dedicated kind, no fake Selection"), Declaration.Group.Kind, EPresentationGroupKind::TurnEndDiscard);
		TestEqual(TEXT("Frozen canonical count"), Declaration.CanonicalSelectedRuntimeIds.Num(), Frozen.Num());
		int32 MemberIndex = 0;
		for (const auto& Record : Fixture.Captured.Records)
		{
			if (Record.Group.Kind != EPresentationGroupKind::TurnEndDiscard) continue;
			TestEqual(TEXT("Producer freezes Hand order"), Record.CardZoneChanged.Card.RuntimeId, Frozen[MemberIndex++].RuntimeId);
			TestEqual(TEXT("Discard chronology removes index zero each time"), Record.CardZoneChanged.FromIndex, 0);
		}
		TestEqual(TEXT("Every intended action stamped exactly once"), MemberIndex, Frozen.Num());
		const auto Token = Fixture.Source.Controller->GetActivePlaybackTokenForTesting();
		if (Mode == 3)
		{
			TestEqual(TEXT("Missing geometry keeps ordinary serial token"), Token.UnitKind, EPresentationPlaybackUnitKind::SingleRecord);
			TestEqual(TEXT("No partial group children"), Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 1);
			TestTrue(TEXT("Unstarted member stays visible"), Originals[1]->IsVisible());
			Fixture.HUD->SkipPresentation();
			continue;
		}
		TestEqual(TEXT("Hand-source group actually starts"), Token.UnitKind, EPresentationPlaybackUnitKind::Group);
		TestEqual(TEXT("Three independent moving children"), Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 3);
		TestEqual(TEXT("Host owns all clones"), Fixture.Area->GetChildrenCount(), 3);
		TestEqual(TEXT("No early reducer"), Fixture.Source.Gameplay.ViewModel->HandCards.Num(), 3);
		TestTrue(TEXT("Different fan positions produce different paths"), Fixture.Area->GetChildAt(0)->GetRenderTransform().Translation != Fixture.Area->GetChildAt(2)->GetRenderTransform().Translation);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TestTrue(TEXT("Formal child identity survives"), Fixture.Hand->GetChildAt(Index) == Originals[Index]);
			TestEqual(TEXT("Historical slots Hidden"), Originals[Index]->GetVisibility(), ESlateVisibility::Hidden);
			TestFalse(TEXT("Historical input suppressed"), Originals[Index]->GetIsEnabled());
			TestEqual(TEXT("No Selection ownership fabricated"), Fixture.Source.Gameplay.ViewModel->GetCardPresentationOwner(Frozen[Index].RuntimeId), ECardPresentationOwner::Hand);
		}
		CollectGarbage(RF_NoFlags);
		TestEqual(TEXT("GC retains all moving children"), Fixture.Area->GetChildrenCount(), 3);
		Fixture.Layout(FVector2D(900, 210));
		Fixture.HUD->InvokeNativeTickForTesting(0.1f);
		const FVector2D ExpectedPosition = FMath::Lerp(FrozenSourceCenter - FVector2D(450, 360), FVector2D(390, 280),
			FMath::InterpEaseOut(0.0f, 1.0f, 0.2f, 3.0f));
		TestTrue(TEXT("Viewport rebase uses captured source and current destination"), Fixture.Area->GetChildAt(0)->GetRenderTransform().Translation.Equals(ExpectedPosition, 0.1f));
		const auto RefreshHandle = Fixture.Source.Gameplay.ViewModel->OnNativeChanged.AddLambda(
			[this, &Fixture](EBattleHUDDirtyFlags)
			{
				if (Fixture.Source.Gameplay.ViewModel->HandCards.Num() != 2) return;
				Fixture.HUD->ReconcileHandForTesting();
				for (UWidget* Card : Fixture.Hand->GetAllChildren())
					TestEqual(TEXT("Leader reduction cannot redisplay visually consumed future members"), Card->GetVisibility(), ESlateVisibility::Hidden);
			});
		if (Mode == 1) Fixture.HUD->SkipPresentation();
		else if (Mode == 2) Fixture.Source.Controller->ExpireActivePlaybackForTesting();
		else if (Mode == 4) Fixture.HUD->SetBufferedPlayerInputEnabled(false);
		else if (Mode == 5) Fixture.Source.Controller->SetWidget(Fixture.Source.Widget);
		else if (Mode == 6) { Fixture.Source.Gameplay.Battle->StartBattle(); Fixture.Source.Gameplay.FlushReady(); }
		else if (Mode == 7) Fixture.HUD->DestructSelectionForTesting();
		else
		{
			Fixture.HUD->FinishNativeCardTransitionGroupForTesting(Token);
			FTSTicker::GetCoreTicker().Tick(0.0f);
		}
		Fixture.Source.Gameplay.ViewModel->OnNativeChanged.Remove(RefreshHandle);
		TestEqual(TEXT("Every clone retired on finish / cancel / timeout"), Fixture.Area->GetChildrenCount(), 0);
		TestEqual(TEXT("Final hand reduced or new Battle retained"), Fixture.Source.Gameplay.ViewModel->HandCards.Num(), Mode == 6 ? 3 : 0);
		TestEqual(TEXT("Three discards committed once; new Battle starts clean"), Fixture.Source.Gameplay.ViewModel->DiscardCount, Mode == 6 ? 0 : 3);
		Fixture.HUD->FinishNativeCardTransitionGroupForTesting(Token);
		TestEqual(TEXT("Old token has no effect"), Fixture.Source.Gameplay.ViewModel->DiscardCount, Mode == 6 ? 0 : 3);
	}
	return true;
}
#endif
