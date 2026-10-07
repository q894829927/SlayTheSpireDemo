#include "BattlePresentationController.h"

bool UBattlePresentationController::TryGetPlayedCardLifecycleForRecord(const FPresentationRecord& Record,
	FPlayedCardPresentationLifecycleToken& OutToken) const
{
	OutToken = {};
	if (!bHasWorkingPresentationSnapshot || !bHasActiveEnvelope || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| !ActivePresentationSessionToken.IsValid()) return false;
	const auto& Current = ActiveEnvelope.Records[ActiveRecordIndex];
	if (Record.BattleId != Current.BattleId || Record.ResolutionId != Current.ResolutionId
		|| Record.PresentationSequence != Current.PresentationSequence || Record.Type != Current.Type) return false;
	auto Candidate = WorkingPresentationSnapshot; auto CandidateHistory = CardHistoryState;
	return PresentationCardReducer::TryApplyRecord(Candidate, CandidateHistory, Record, ActivePresentationSessionToken, &OutToken)
		&& OutToken.IsValid();
}
