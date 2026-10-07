#include "PresentationCardReducer.h"
#include "PresentationCardView.h"

bool FPlayedCardPresentationLifecycleToken::HasHistoricalIdentity() const
{
	return BattleId > 0 && SourceResolutionId > 0 && CardPlayedPresentationSequence > 0
		&& LocalLifecycleGeneration > 0 && RuntimeId != INDEX_NONE && !CardId.IsNone();
}

bool FPlayedCardPresentationLifecycleToken::operator==(const FPlayedCardPresentationLifecycleToken& Other) const
{
	return SessionToken == Other.SessionToken && BattleId == Other.BattleId
		&& SourceResolutionId == Other.SourceResolutionId && CardPlayedPresentationSequence == Other.CardPlayedPresentationSequence
		&& LocalLifecycleGeneration == Other.LocalLifecycleGeneration && RuntimeId == Other.RuntimeId && CardId == Other.CardId;
}

bool PresentationCardReducer::IsCardSnapshotValid(const FPresentationCardSnapshot& Card)
{
	return Card.RuntimeId != INDEX_NONE && !Card.CardId.IsNone() && !Card.DisplayName.IsEmpty() && Card.Cost >= 0
		&& Card.CardType >= ECardType::Attack && Card.CardType <= ECardType::Curse
		&& Card.Rarity >= ECardRarity::Basic && Card.Rarity <= ECardRarity::Curse
		&& Card.CardColor >= ECardColor::Red && Card.CardColor <= ECardColor::Curse
		&& Card.TargetType >= ECardTargetType::None && Card.TargetType <= ECardTargetType::Enemy;
}

bool PresentationCardReducer::DoesCardViewMatch(const FBattleHUDCardView& View, const FPresentationCardSnapshot& Card)
{
	return View.RuntimeId == Card.RuntimeId && View.CardId == Card.CardId && View.DisplayName.EqualTo(Card.DisplayName)
		&& View.bUpgraded == Card.bUpgraded && View.Cost == Card.Cost && View.CardType == Card.CardType
		&& View.Rarity == Card.Rarity && View.CardColor == Card.CardColor && View.TargetType == Card.TargetType
		&& View.Description.EqualTo(Card.Description) && View.CardArt == Card.CardArt;
}

namespace
{
	bool ApplyCandidate(FPresentationStateSnapshot& S, FCardPresentationHistoryState& H,
		const FPresentationRecord& R, const FPresentationSessionToken& Session, FPlayedCardPresentationLifecycleToken& Out)
	{
		if (S.BattleId <= 0 || R.BattleId != S.BattleId || R.ResolutionId <= 0 || R.PresentationSequence <= 0
			|| (!Session.IsValid() && (Session.BattleId != 0 || Session.ControllerEpoch != 0 || Session.PresentationSessionGeneration != 0))
			|| (Session.IsValid() && Session.BattleId != S.BattleId) || S.DrawCount < 0 || S.DiscardCount < 0 || S.ExhaustCount < 0) return false;
		TSet<int32> HandIds;
		for (const auto& C : S.HandCards)
		{
			if (C.RuntimeId == INDEX_NONE || C.CardId.IsNone() || HandIds.Contains(C.RuntimeId)) return false;
			HandIds.Add(C.RuntimeId);
		}
		const bool bPlayed = R.Type == EBattlePresentationRecordType::CardPlayed;
		if (!bPlayed && R.Type != EBattlePresentationRecordType::CardZoneChanged) return false;
		const auto& Card = bPlayed ? R.CardPlayed.Card : R.CardZoneChanged.Card;
		if (!PresentationCardReducer::IsCardSnapshotValid(Card)) return false;
		const int32 HandIndex = S.HandCards.IndexOfByPredicate([&](const auto& C) { return C.RuntimeId == Card.RuntimeId; });
		const bool bExactHand = HandIndex != INDEX_NONE && PresentationCardReducer::DoesCardViewMatch(S.HandCards[HandIndex], Card);
		int32 PendingIndex = INDEX_NONE, Matches = 0;
		for (int32 I = 0; I < H.PendingPlays.Num(); ++I)
			if (H.PendingPlays[I].Token.RuntimeId == Card.RuntimeId) { PendingIndex = I; ++Matches; }
		if (bPlayed)
		{
			const auto& P = R.CardPlayed;
			const auto Known = [&](FName Id) { return !Id.IsNone() && (Id == S.Player.PresentationId || Id == S.Enemy.PresentationId); };
			if (!bExactHand || HandIndex != P.HandIndexBefore || P.PlayAreaIndexAfter != 0 || Matches != 0
				|| !Known(P.SourcePresentationId) || (!P.TargetPresentationId.IsNone() && !Known(P.TargetPresentationId))
				|| P.EnergyBefore != S.Energy || P.EnergyBefore < 0 || P.EnergyAfter < 0 || P.EnergyAfter > P.EnergyBefore
				|| P.CostPaid < 0 || P.CostPaid != P.EnergyBefore - P.EnergyAfter || P.CostPaid != Card.Cost
				|| H.NextLifecycleGeneration <= 0 || H.NextLifecycleGeneration == MAX_int64) return false;
			Out.SessionToken = Session; Out.BattleId = R.BattleId; Out.SourceResolutionId = R.ResolutionId;
			Out.CardPlayedPresentationSequence = R.PresentationSequence; Out.LocalLifecycleGeneration = H.NextLifecycleGeneration++;
			Out.RuntimeId = Card.RuntimeId; Out.CardId = Card.CardId;
			FPlayedCardHistoryEntry Entry; Entry.Token = Out; Entry.Card = Card; H.PendingPlays.Add(Entry);
			S.HandCards.RemoveAt(HandIndex); S.Energy = P.EnergyAfter;
			return true;
		}
		const auto& P = R.CardZoneChanged;
		if (P.FromZone == ECardZone::PlayArea)
		{
			if (Matches != 1 || HandIndex != INDEX_NONE || P.FromIndex != 0) return false;
			const auto& Entry = H.PendingPlays[PendingIndex];
			if (!Entry.Token.HasHistoricalIdentity() || Entry.Token.SessionToken != Session || Entry.Token.BattleId != R.BattleId
				|| Entry.Token.SourceResolutionId > R.ResolutionId || Entry.Token.CardPlayedPresentationSequence >= R.PresentationSequence
				|| Entry.Token.CardId != Card.CardId
				|| !PresentationCardReducer::DoesCardViewMatch(PresentationCardView::MakePresentationOnlyCardView(Entry.Card), Card)) return false;
			switch (P.ToZone)
			{
			case ECardZone::DiscardPile: if (P.ToIndex != S.DiscardCount || S.DiscardCount == MAX_int32) return false; ++S.DiscardCount; break;
			case ECardZone::ExhaustPile: if (P.ToIndex != S.ExhaustCount || S.ExhaustCount == MAX_int32) return false; ++S.ExhaustCount; break;
			case ECardZone::RemovedPile: if (P.ToIndex < 0) return false; break;
			default: return false;
			}
			Out = Entry.Token; H.PendingPlays.RemoveAt(PendingIndex);
			return true;
		}
		if (Matches != 0) return false;
		if (P.FromZone == ECardZone::DrawPile && P.ToZone == ECardZone::Hand)
		{
			if (S.DrawCount <= 0 || P.FromIndex != S.DrawCount - 1 || HandIndex != INDEX_NONE || P.ToIndex < 0 || P.ToIndex > S.HandCards.Num()) return false;
			--S.DrawCount; S.HandCards.Insert(PresentationCardView::MakePresentationOnlyCardView(Card), P.ToIndex); return true;
		}
		if (P.FromZone != ECardZone::Hand || !bExactHand || P.FromIndex != HandIndex) return false;
		switch (P.ToZone)
		{
		case ECardZone::DiscardPile: if (P.ToIndex != S.DiscardCount || S.DiscardCount == MAX_int32) return false; ++S.DiscardCount; break;
		case ECardZone::ExhaustPile: if (P.ToIndex != S.ExhaustCount || S.ExhaustCount == MAX_int32) return false; ++S.ExhaustCount; break;
		case ECardZone::DrawPile: if (P.ToIndex != S.DrawCount || S.DrawCount == MAX_int32) return false; ++S.DrawCount; break;
		default: return false;
		}
		S.HandCards.RemoveAt(HandIndex); return true;
	}
}

bool PresentationCardReducer::TryApplyRecord(FPresentationStateSnapshot& Snapshot, FCardPresentationHistoryState& History,
	const FPresentationRecord& Record, const FPresentationSessionToken& Session, FPlayedCardPresentationLifecycleToken* OutLifecycle)
{
	if (OutLifecycle) *OutLifecycle = {};
	auto Candidate = Snapshot; auto CandidateHistory = History; FPlayedCardPresentationLifecycleToken Token;
	if (!ApplyCandidate(Candidate, CandidateHistory, Record, Session, Token)) return false;
	Snapshot = MoveTemp(Candidate); History = MoveTemp(CandidateHistory);
	if (OutLifecycle) *OutLifecycle = Token;
	return true;
}
