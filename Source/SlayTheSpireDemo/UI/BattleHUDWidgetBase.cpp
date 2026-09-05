#include "BattleHUDWidgetBase.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"
#include "../Presentation/PresentationCardView.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Containers/Ticker.h"

namespace
{
	bool DoesDiagnosticCardViewMatchSnapshot(
		const FBattleHUDCardView& View,
		const FPresentationCardSnapshot& Snapshot)
	{
		return View.RuntimeId == Snapshot.RuntimeId
			&& View.CardId == Snapshot.CardId
			&& View.DisplayName.EqualTo(Snapshot.DisplayName)
			&& View.bUpgraded == Snapshot.bUpgraded
			&& View.Cost == Snapshot.Cost
			&& View.CardType == Snapshot.CardType
			&& View.Rarity == Snapshot.Rarity
			&& View.CardColor == Snapshot.CardColor
			&& View.TargetType == Snapshot.TargetType
			&& View.Description.EqualTo(Snapshot.Description)
			&& View.CardArt.Get() == Snapshot.CardArt.Get();
	}

	bool IsDiagnosticCardSnapshotValid(const FPresentationCardSnapshot& Snapshot)
	{
		const bool bCardTypeValid = Snapshot.CardType == ECardType::Attack
			|| Snapshot.CardType == ECardType::Skill
			|| Snapshot.CardType == ECardType::Power
			|| Snapshot.CardType == ECardType::Status
			|| Snapshot.CardType == ECardType::Curse;
		const bool bRarityValid = Snapshot.Rarity == ECardRarity::Basic
			|| Snapshot.Rarity == ECardRarity::Common
			|| Snapshot.Rarity == ECardRarity::Uncommon
			|| Snapshot.Rarity == ECardRarity::Rare
			|| Snapshot.Rarity == ECardRarity::Special
			|| Snapshot.Rarity == ECardRarity::Curse;
		const bool bCardColorValid = Snapshot.CardColor == ECardColor::Red
			|| Snapshot.CardColor == ECardColor::Green
			|| Snapshot.CardColor == ECardColor::Blue
			|| Snapshot.CardColor == ECardColor::Purple
			|| Snapshot.CardColor == ECardColor::Colorless
			|| Snapshot.CardColor == ECardColor::Curse;
		const bool bTargetTypeValid = Snapshot.TargetType == ECardTargetType::None
			|| Snapshot.TargetType == ECardTargetType::Self
			|| Snapshot.TargetType == ECardTargetType::Enemy;
		return Snapshot.RuntimeId != INDEX_NONE
			&& !Snapshot.CardId.IsNone()
			&& !Snapshot.DisplayName.IsEmpty()
			&& Snapshot.Cost >= 0
			&& bCardTypeValid
			&& bRarityValid
			&& bCardColorValid
			&& bTargetTypeValid;
	}

	bool IsDiagnosticKnownPresentationId(
		const UBattleHUDViewModel* InViewModel,
		FName PresentationId)
	{
		if (!IsValid(InViewModel) || PresentationId.IsNone())
		{
			return false;
		}
		const bool bPlayer = InViewModel->Player.PresentationId == PresentationId;
		const bool bEnemy = InViewModel->Enemy.PresentationId == PresentationId;
		return bPlayer != bEnemy;
	}

	FString BuildPanelChildSummary(const UPanelWidget* Panel)
	{
		if (!IsValid(Panel))
		{
			return TEXT("<invalid>");
		}

		FString Result;
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = Panel->GetChildAt(Index);
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += FString::Printf(
				TEXT("%d:%s/%s"),
				Index,
				*GetNameSafe(IsValid(Child) ? Child->GetClass() : nullptr),
				*GetNameSafe(Child));
		}
		return Result.IsEmpty() ? TEXT("<empty>") : Result;
	}
}

void UBattleHUDWidgetBase::SetViewModel(UBattleHUDViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		HandleViewModelChanged();
		return;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.RemoveDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	ViewModel = InViewModel;
	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.AddDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	HandleViewModelChanged();
}

void UBattleHUDWidgetBase::SetPresentationController(
	UBattlePresentationController* InController
)
{
	PresentationController = InController;
}

bool UBattleHUDWidgetBase::SelectCard(int32 RuntimeId)
{
	return IsValid(ViewModel) && ViewModel->SelectCardByRuntimeId(RuntimeId);
}

void UBattleHUDWidgetBase::CancelSelection()
{
	if (IsValid(ViewModel))
	{
		ViewModel->CancelSelection();
	}
}

bool UBattleHUDWidgetBase::SelectTarget(int32 TargetId)
{
	if (!IsValid(ViewModel))
	{
		return false;
	}

	// A3 pre-commit ownership ends before the authoritative request is entered.
	ViewModel->ClearPreviewTarget();
	return ViewModel->SelectTargetById(TargetId);
}

bool UBattleHUDWidgetBase::ConfirmSelectedCard()
{
	return IsValid(ViewModel) && ViewModel->ConfirmSelectedCard();
}

bool UBattleHUDWidgetBase::EndTurn()
{
	return IsValid(ViewModel) && ViewModel->RequestEndTurn();
}

FBattleHUDCardView UBattleHUDWidgetBase::MakePresentationCardView(
	const FPresentationCardSnapshot& Snapshot
) const
{
	return PresentationCardView::MakePresentationOnlyCardView(Snapshot);
}

FBattleHUDStatusView UBattleHUDWidgetBase::MakePresentationStatusView(
	const FStatusChangedPresentationPayload& StatusChanged
) const
{
	FBattleHUDStatusView View;

	View.StatusId = StatusChanged.StatusId;
	View.RuntimeSequence = StatusChanged.RuntimeSequence;

	View.DisplayName = StatusChanged.DisplayName;
	View.Description = StatusChanged.DescriptionAfter;
	View.Amount = StatusChanged.AmountAfter;

	View.bUseAtlasIcon = StatusChanged.bUseAtlasIcon;
	View.UVOffset = StatusChanged.UVOffset;
	View.UVScale = StatusChanged.UVScale;
	View.TrimOffset = StatusChanged.TrimOffset;
	View.TrimScale = StatusChanged.TrimScale;

	return View;
}

bool UBattleHUDWidgetBase::PlayPresentationRecord(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token
)
{
	// Controller guarantees one active Record at a time. Cancel defensively if a
	// replacement visual is offered anyway, then establish ownership before
	// entering Blueprint so even a synchronous misuse is associated with Token.
	CancelTrackedPresentationPlayback();
	TrackedPresentationPlaybackToken = Token;
	bHasTrackedPresentationPlayback = true;

	const bool bAccepted = BeginPresentationRecordPlayback(Record, Token);
	if (!bAccepted)
	{
		LogPresentationRecordRejection(Record, Token);
		ClearTrackedPresentationPlayback(Token);
	}
	return bAccepted;
}

bool UBattleHUDWidgetBase::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& /*Token*/
)
{
	return false;
}

void UBattleHUDWidgetBase::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& /*Token*/
)
{
}

void UBattleHUDWidgetBase::LogPresentationRecordRejection(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token) const
{
	if (Record.Type != EBattlePresentationRecordType::CardPlayed)
	{
		return;
	}

	const FCardPlayedPresentationPayload& Payload = Record.CardPlayed;
	const UBattleHUDViewModel* VM = ViewModel;
	UHorizontalBox* Hand = nullptr;
	UOverlay* PlayArea = nullptr;
	if (IsValid(WidgetTree))
	{
		Hand = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("HB_Hand")));
		PlayArea = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("OV_PlayArea")));
	}

	const bool bViewModelValid = IsValid(VM);
	const bool bHandValid = IsValid(Hand);
	const bool bPlayAreaValid = IsValid(PlayArea);
	const bool bCardSnapshotValid = IsDiagnosticCardSnapshotValid(Payload.Card);
	const bool bSourceValid = IsDiagnosticKnownPresentationId(VM, Payload.SourcePresentationId);
	const bool bTargetValid = Payload.TargetPresentationId.IsNone()
		|| IsDiagnosticKnownPresentationId(VM, Payload.TargetPresentationId);
	const bool bHandIndexValid = Payload.HandIndexBefore >= 0;
	const bool bPlayAreaIndexValid = Payload.PlayAreaIndexAfter == 0;
	const bool bEnergyBeforeValid = Payload.EnergyBefore >= 0;
	const bool bEnergyAfterValid = Payload.EnergyAfter >= 0;
	const bool bEnergyOrderValid = Payload.EnergyAfter <= Payload.EnergyBefore;
	const bool bCostPaidNonNegative = Payload.CostPaid >= 0;
	const bool bCostDeltaValid = Payload.CostPaid == Payload.EnergyBefore - Payload.EnergyAfter;
	const bool bCostMatchesCard = Payload.CostPaid == Payload.Card.Cost;
	const bool bViewModelEnergyMatches = bViewModelValid && VM->Energy == Payload.EnergyBefore;
	const int32 PlayAreaChildren = bPlayAreaValid ? PlayArea->GetChildrenCount() : INDEX_NONE;
	const bool bPlayAreaChildrenMatch = bPlayAreaValid
		&& PlayAreaChildren == Payload.PlayAreaIndexAfter;

	bool bViewModelHandIndexMatches = false;
	bool bHandWidgetCountMatchesViewModel = false;
	bool bRequiredHandWidgetMatches = false;
	int32 ViewModelRuntimeMatches = 0;
	int32 HandWidgetRuntimeMatches = 0;

	if (bViewModelValid)
	{
		for (const FBattleHUDCardView& CardView : VM->HandCards)
		{
			ViewModelRuntimeMatches += CardView.RuntimeId == Payload.Card.RuntimeId ? 1 : 0;
		}
		if (VM->HandCards.IsValidIndex(Payload.HandIndexBefore))
		{
			bViewModelHandIndexMatches = DoesDiagnosticCardViewMatchSnapshot(
				VM->HandCards[Payload.HandIndexBefore],
				Payload.Card);
		}
	}

	if (bHandValid)
	{
		bHandWidgetCountMatchesViewModel = bViewModelValid
			&& Hand->GetChildrenCount() == VM->HandCards.Num();
		for (int32 Index = 0; Index < Hand->GetChildrenCount(); ++Index)
		{
			const UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(Hand->GetChildAt(Index));
			if (IsValid(CardWidget))
			{
				HandWidgetRuntimeMatches += CardWidget->GetRuntimeId() == Payload.Card.RuntimeId ? 1 : 0;
			}
		}
		if (Payload.HandIndexBefore >= 0 && Payload.HandIndexBefore < Hand->GetChildrenCount())
		{
			const UBattleCardWidget* RequiredWidget = Cast<UBattleCardWidget>(Hand->GetChildAt(Payload.HandIndexBefore));
			bRequiredHandWidgetMatches = IsValid(RequiredWidget)
				&& DoesDiagnosticCardViewMatchSnapshot(RequiredWidget->GetCardView(), Payload.Card);
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleHUD][CardPlayedReject] Token(Battle=%lld Resolution=%lld Seq=%lld Gen=%lld) Record(Battle=%lld Resolution=%lld Seq=%lld) Card=%s#%d HandIndex=%d PlayAreaIndexAfter=%d | VM=%d Hand=%d PlayArea=%d CardSnapshot=%d Source=%d Target=%d HandIndexNonNegative=%d PlayAreaIndexZero=%d EnergyBeforeNonNegative=%d EnergyAfterNonNegative=%d EnergyOrder=%d CostPaidNonNegative=%d CostDelta=%d CostMatchesCard=%d VMEnergyMatches=%d PlayAreaChildrenMatch=%d VMHandIndexMatches=%d HandCountMatchesVM=%d RequiredHandWidgetMatches=%d VMRuntimeMatches=%d HandWidgetRuntimeMatches=%d | Energy VM=%d Before=%d After=%d CostPaid=%d CardCost=%d | HandChildren=%d VMHand=%d PlayAreaChildren=%d"),
		static_cast<long long>(Token.BattleId),
		static_cast<long long>(Token.ResolutionId),
		static_cast<long long>(Token.PresentationSequence),
		static_cast<long long>(Token.LocalPlaybackGeneration),
		static_cast<long long>(Record.BattleId),
		static_cast<long long>(Record.ResolutionId),
		static_cast<long long>(Record.PresentationSequence),
		*Payload.Card.CardId.ToString(),
		Payload.Card.RuntimeId,
		Payload.HandIndexBefore,
		Payload.PlayAreaIndexAfter,
		bViewModelValid ? 1 : 0,
		bHandValid ? 1 : 0,
		bPlayAreaValid ? 1 : 0,
		bCardSnapshotValid ? 1 : 0,
		bSourceValid ? 1 : 0,
		bTargetValid ? 1 : 0,
		bHandIndexValid ? 1 : 0,
		bPlayAreaIndexValid ? 1 : 0,
		bEnergyBeforeValid ? 1 : 0,
		bEnergyAfterValid ? 1 : 0,
		bEnergyOrderValid ? 1 : 0,
		bCostPaidNonNegative ? 1 : 0,
		bCostDeltaValid ? 1 : 0,
		bCostMatchesCard ? 1 : 0,
		bViewModelEnergyMatches ? 1 : 0,
		bPlayAreaChildrenMatch ? 1 : 0,
		bViewModelHandIndexMatches ? 1 : 0,
		bHandWidgetCountMatchesViewModel ? 1 : 0,
		bRequiredHandWidgetMatches ? 1 : 0,
		ViewModelRuntimeMatches,
		HandWidgetRuntimeMatches,
		bViewModelValid ? VM->Energy : INDEX_NONE,
		Payload.EnergyBefore,
		Payload.EnergyAfter,
		Payload.CostPaid,
		Payload.Card.Cost,
		bHandValid ? Hand->GetChildrenCount() : INDEX_NONE,
		bViewModelValid ? VM->HandCards.Num() : INDEX_NONE,
		PlayAreaChildren);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleHUD][CardPlayedReject] HandChildren=[%s] PlayAreaChildren=[%s]"),
		*BuildPanelChildSummary(Hand),
		*BuildPanelChildSummary(PlayArea));
}

void UBattleHUDWidgetBase::NotifyPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	// Blueprint is required to complete asynchronously, but enforce that boundary
	// here as well. A Blueprint that accidentally calls this from inside the
	// playback event therefore cannot re-enter the Controller while it is still
	// offering the current Record.
	const TWeakObjectPtr<UBattleHUDWidgetBase> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, Token](float /*DeltaTime*/)
			{
				if (UBattleHUDWidgetBase* Widget = WeakThis.Get())
				{
					Widget->ForwardPresentationFinished(Token);
				}
				return false;
			}
		),
		0.0f
	);
}

void UBattleHUDWidgetBase::ForwardPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	// A stale completion must never clear a newer visual token.
	ClearTrackedPresentationPlayback(Token);

	if (!IsValid(PresentationController))
	{
		return;
	}

	// Controller completion synchronously advances the historical ViewModel.
	// That ViewModel change is the normal completion path, not a reason to ask
	// Blueprint to cancel the visual that has just finished.
	TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
	PresentationController->NotifyPresentationFinished(Token);
}

void UBattleHUDWidgetBase::SkipPresentation()
{
	// Stop presentation-only visuals before Controller collapses to the newest
	// frozen FinalSnapshot. Stale callbacks remain harmless through token checks.
	CancelTrackedPresentationPlayback();

	if (IsValid(PresentationController))
	{
		TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
		PresentationController->SkipPresentation();
	}
}

void UBattleHUDWidgetBase::NativeDestruct()
{
	// No Blueprint cancellation event is dispatched during destruction. The Widget
	// is already leaving the tree, while Controller NotifyWidgetLost provides the
	// authoritative playback catch-up/fail-safe behavior.
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};

	// Stop observing ViewModel changes before NotifyWidgetLost. That controller
	// callback may synchronously SkipPresentation/collapse to a newer historical
	// snapshot and broadcast OnChanged; a Widget already leaving the tree must not
	// redraw Hand/Status/other UMG children during that teardown catch-up.
	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.RemoveDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	if (IsValid(PresentationController))
	{
		PresentationController->NotifyWidgetLost(this);
	}
	PresentationController = nullptr;

	Super::NativeDestruct();
}

void UBattleHUDWidgetBase::HandleViewModelChanged()
{
	// During Controller-owned playback, the ViewModel advances only after a Record
	// completes or after a fail-safe collapse/timeout/unavailable transition. If
	// the change did not originate from normal completion or explicit Skip, a
	// tracked Blueprint visual belongs to abandoned historical work and must stop
	// before the HUD redraws the new frozen state.
	if (!bSuppressPresentationCancellation)
	{
		CancelTrackedPresentationPlayback();
	}

	NativeOnBattleHUDViewModelChanged();
}

void UBattleHUDWidgetBase::NativeOnBattleHUDViewModelChanged()
{
	// Preserve the sealed Legacy Blueprint contract by default. Native concrete
	// HUD classes may override this hook and intentionally omit Super to own refresh.
	BP_OnViewModelChanged();
}

void UBattleHUDWidgetBase::CancelTrackedPresentationPlayback()
{
	if (!bHasTrackedPresentationPlayback)
	{
		return;
	}

	const FPresentationPlaybackToken CancelledToken = TrackedPresentationPlaybackToken;
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};

	// Clear ownership before entering Blueprint. A miswired cancellation callback
	// that later calls NotifyPresentationFinished therefore remains stale and cannot
	// erase a newer visual token established by another Record.
	CancelPresentationRecordPlayback(CancelledToken);
}

void UBattleHUDWidgetBase::ClearTrackedPresentationPlayback(
	const FPresentationPlaybackToken& Token
)
{
	if (bHasTrackedPresentationPlayback
		&& TrackedPresentationPlaybackToken == Token)
	{
		bHasTrackedPresentationPlayback = false;
		TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};
	}
}
