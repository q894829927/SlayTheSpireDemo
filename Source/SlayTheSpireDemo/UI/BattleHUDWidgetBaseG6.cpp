#include "BattleHUDWidgetBase.h"

#include "../Presentation/BattlePresentationController.h"
#include "Containers/Ticker.h"

namespace
{
	bool AreG6GroupTagsEquivalent(
		const FPresentationGroupTag& Left,
		const FPresentationGroupTag& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.GroupId == Right.GroupId
			&& Left.ExpectedMemberCount == Right.ExpectedMemberCount;
	}
}

bool UBattleHUDWidgetBase::TryReplaceTrackedPresentationRecordWithGroup(
	const FPresentationPlaybackToken& ExpectedSingleRecordToken,
	const TArray<FPresentationRecord>& Records,
	const FPresentationGroupTag& Group,
	const TArray<int32>& RecordIndices,
	const FPresentationPlaybackToken& GroupToken)
{
	if (!bHasTrackedPresentationPlayback
		|| TrackedPresentationPlaybackUnit.Token != ExpectedSingleRecordToken
		|| ExpectedSingleRecordToken.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| ExpectedSingleRecordToken.GroupId != 0
		|| !Group.IsValid()
		|| Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Group.ExpectedMemberCount <= 1
		|| Records.Num() != Group.ExpectedMemberCount
		|| RecordIndices.Num() != Group.ExpectedMemberCount
		|| !GroupToken.IsValid()
		|| GroupToken.UnitKind != EPresentationPlaybackUnitKind::Group
		|| GroupToken.GroupId != Group.GroupId
		|| GroupToken.BattleId != ExpectedSingleRecordToken.BattleId
		|| GroupToken.ResolutionId != ExpectedSingleRecordToken.ResolutionId
		|| GroupToken.PresentationSequence != ExpectedSingleRecordToken.PresentationSequence)
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
			|| Record.BattleId != GroupToken.BattleId
			|| Record.ResolutionId != GroupToken.ResolutionId
			|| Record.PresentationSequence <= 0
			|| (PreviousSequence > 0 && Record.PresentationSequence <= PreviousSequence)
			|| !AreG6GroupTagsEquivalent(Record.Group, Group))
		{
			return false;
		}
		PreviousRecordIndex = RecordIndex;
		PreviousSequence = Record.PresentationSequence;
	}
	if (Records[0].PresentationSequence != GroupToken.PresentationSequence)
	{
		return false;
	}

	const FTrackedPresentationPlaybackUnit OriginalUnit = TrackedPresentationPlaybackUnit;
	TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
	TrackedPresentationPlaybackUnit.Token = GroupToken;
	TrackedPresentationPlaybackUnit.Group = Group;
	TrackedPresentationPlaybackUnit.RecordIndices = RecordIndices;
	bHasTrackedPresentationPlayback = true;

	if (!BeginPresentationGroupPlayback(Records, Group, GroupToken))
	{
		// The leader's concrete SingleRecord Begin has not started yet, so this is
		// a pure transactional rollback: no SingleRecord cancellation hook fires.
		TrackedPresentationPlaybackUnit = OriginalUnit;
		bHasTrackedPresentationPlayback = true;
		return false;
	}
	return true;
}

void UBattleHUDWidgetBase::NotifyPresentationGroupFinishedG6(
	const FPresentationPlaybackToken& Token)
{
	const TWeakObjectPtr<UBattleHUDWidgetBase> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, Token](float)
			{
				UBattleHUDWidgetBase* Widget = WeakThis.Get();
				if (!IsValid(Widget)
					|| !Widget->bHasTrackedPresentationPlayback
					|| Widget->TrackedPresentationPlaybackUnit.Token != Token
					|| Token.UnitKind != EPresentationPlaybackUnitKind::Group)
				{
					return false;
				}

				const TArray<int32> RecordIndices =
					Widget->TrackedPresentationPlaybackUnit.RecordIndices;
				Widget->bHasTrackedPresentationPlayback = false;
				Widget->TrackedPresentationPlaybackUnit = FTrackedPresentationPlaybackUnit{};
				if (IsValid(Widget->PresentationController))
				{
					TGuardValue<bool> SuppressCancellation(
						Widget->bSuppressPresentationCancellation,
						true);
					Widget->PresentationController->NotifyPresentationGroupFinishedG6(
						Token,
						RecordIndices);
				}
				return false;
			}),
		0.0f);
}
