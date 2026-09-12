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
#include "InputCoreTypes.h"

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

	bool AreHUDGroupTagsEquivalent(
		const FPresentationGroupTag& Left,
		const FPresentationGroupTag& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.GroupId == Right.GroupId
			&& Left.ExpectedMemberCount == Right.ExpectedMemberCount;
	}
}

void UBattleHUDWidgetBase::SetViewModel(UBattleHUDViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		HandleNativeViewModelChanged(EBattleHUDDirtyFlags::All);
		return;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->OnNativeChanged.RemoveAll(this);
	}

	ViewModel = InViewModel;
	if (IsValid(ViewModel))
	{
		ViewModel->OnNativeChanged.AddUObject(
			this,
			&UBattleHUDWidgetBase::HandleNativeViewModelChanged);
	}

	HandleNativeViewModelChanged(EBattleHUDDirtyFlags::All);
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
	const FPresentationPlaybackToken& Token,
	int32 RecordIndex
)
{
	if (!Token.IsValid()
		|| Token.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| Token.GroupId != 0
		|| Record.BattleId != Token.BattleId
		|| Record.ResolutionId != Token.ResolutionId
		|| Record.PresentationSequence != Token.PresentationSequence)
	{
		return false;
	}

	CancelTrackedPresentationPlayback();
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	TrackedPresentationPlaybackUnit.Token = Token;
	if (RecordIndex != INDEX_NONE)
	{
		TrackedPresentationPlaybackUnit.RecordIndices.Add(RecordIndex);
	}
	bHasTrackedPresentationPlayback = true;

	const bool bAccepted = BeginPresentationRecordPlayback(Record, Token);
	if (!bAccepted)
	{
		LogPresentationRecordRejection(Record, Token);
		ClearTrackedPresentationPlayback(Token);
	}
	return bAccepted;
}

bool UBattleHUDWidgetBase::PlayPresentationGroup(
	const TArray<FPresentationRecord>& Records,
	const FPresentationGroupTag& Group,
	const TArray<int32>& RecordIndices,
	const FPresentationPlaybackToken& Token
)
{
	if (!Group.IsValid()
		|| Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Group.ExpectedMemberCount <= 1
		|| Records.Num() != Group.ExpectedMemberCount
		|| RecordIndices.Num() != Group.ExpectedMemberCount
		|| !Token.IsValid()
		|| Token.UnitKind != EPresentationPlaybackUnitKind::Group
		|| Token.GroupId != Group.GroupId)
	{
		return false;
	}

	int32 PreviousRecordIndex = INDEX_NONE;
	int64 PreviousSequence = 0;
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FPresentationRecord& Record = Records[Index];
		const int32 RecordIndex = RecordIndices[Index];
		if (RecordIndex < 0
			|| (Index > 0 && RecordIndex <= PreviousRecordIndex)
			|| Record.BattleId != Token.BattleId
			|| Record.ResolutionId != Token.ResolutionId
			|| Record.PresentationSequence <= 0
			|| (PreviousSequence > 0 && Record.PresentationSequence <= PreviousSequence)
			|| !AreHUDGroupTagsEquivalent(Record.Group, Group))
		{
			return false;
		}
		PreviousRecordIndex = RecordIndex;
		PreviousSequence = Record.PresentationSequence;
	}
	if (Records[0].PresentationSequence != Token.PresentationSequence)
	{
		return false;
	}

	CancelTrackedPresentationPlayback();
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	TrackedPresentationPlaybackUnit.Token = Token;
	TrackedPresentationPlaybackUnit.Group = Group;
	TrackedPresentationPlaybackUnit.RecordIndices = RecordIndices;
	bHasTrackedPresentationPlayback = true;

	const bool bAccepted = BeginPresentationGroupPlayback(Records, Group, Token);
	if (!bAccepted)
	{
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

bool UBattleHUDWidgetBase::BeginPresentationGroupPlayback(
	const TArray<FPresentationRecord>& /*Records*/,
	const FPresentationGroupTag& /*Group*/,
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

void UBattleHUDWidgetBase::CancelPresentationGroupPlayback(
	const FPresentationGroupTag& /*Group*/,
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
	UPanelWidget* Hand = nullptr;
	UOverlay* PlayArea = nullptr;
	if (IsValid(WidgetTree))
	{
		Hand = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("FanHand")));
		if (!Hand) Hand = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("HB_Hand")));
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
	if (!ClearTrackedPresentationPlayback(Token))
	{
		return;
	}

	if (!IsValid(PresentationController))
	{
		return;
	}

	TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
	PresentationController->NotifyPresentationFinished(Token);
}

void UBattleHUDWidgetBase::SkipPresentation()
{
	CancelTrackedPresentationPlayback();

	if (IsValid(PresentationController))
	{
		TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
		PresentationController->SkipPresentation();
	}
}

void UBattleHUDWidgetBase::NativeDestruct()
{
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};

	if (IsValid(ViewModel))
	{
		ViewModel->OnNativeChanged.RemoveAll(this);
	}

	if (IsValid(PresentationController))
	{
		PresentationController->NotifyWidgetLost(this);
	}
	PresentationController = nullptr;

	Super::NativeDestruct();
}

void UBattleHUDWidgetBase::HandleNativeViewModelChanged(EBattleHUDDirtyFlags DirtyFlags)
{
	if (!bSuppressPresentationCancellation)
	{
		CancelTrackedPresentationPlayback();
	}

	TGuardValue<EBattleHUDDirtyFlags> ScopedDirtyFlags(
		CurrentNativeViewModelDirtyFlags,
		DirtyFlags);
	NativeOnBattleHUDViewModelChanged();
}

void UBattleHUDWidgetBase::NativeOnBattleHUDViewModelChanged()
{
	BP_OnViewModelChanged();
}

bool UBattleHUDWidgetBase::CancelTrackedPresentationPlayback(
	const FPresentationPlaybackToken& ExpectedToken
)
{
	if (!bHasTrackedPresentationPlayback
		|| TrackedPresentationPlaybackUnit.Token != ExpectedToken)
	{
		return false;
	}

	const FTrackedPresentationPlaybackUnit CancelledUnit = TrackedPresentationPlaybackUnit;
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	DispatchTrackedPresentationCancellation(CancelledUnit);
	return true;
}

void UBattleHUDWidgetBase::CancelTrackedPresentationPlayback()
{
	if (!bHasTrackedPresentationPlayback)
	{
		return;
	}

	const FTrackedPresentationPlaybackUnit CancelledUnit = TrackedPresentationPlaybackUnit;
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	DispatchTrackedPresentationCancellation(CancelledUnit);
}

bool UBattleHUDWidgetBase::ClearTrackedPresentationPlayback(
	const FPresentationPlaybackToken& Token
)
{
	if (!bHasTrackedPresentationPlayback
		|| TrackedPresentationPlaybackUnit.Token != Token)
	{
		return false;
	}

	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	return true;
}

void UBattleHUDWidgetBase::DispatchTrackedPresentationCancellation(
	const FTrackedPresentationPlaybackUnit& Unit
)
{
	if (Unit.Token.UnitKind == EPresentationPlaybackUnitKind::Group)
	{
		CancelPresentationGroupPlayback(Unit.Group, Unit.Token);
		return;
	}
	CancelPresentationRecordPlayback(Unit.Token);
}

FReply UBattleHUDWidgetBase::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& HandleRightMouseButtonCancelInput())
	{
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

bool UBattleHUDWidgetBase::HandleRightMouseButtonCancelInput()
{
	return HandleRightMouseButtonCancel();
}

bool UBattleHUDWidgetBase::HandleRightMouseButtonCancel()
{
	if (!IsValid(ViewModel))
	{
		return false;
	}

	const bool bHasOrdinarySelection =
		ViewModel->SelectedCardRuntimeId != INDEX_NONE
		|| ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget
		|| ViewModel->InteractionState == EBattleHUDInteractionState::ReadyToConfirm;
	if (!bHasOrdinarySelection)
	{
		return false;
	}

	CancelSelection();
	return true;
}
