#pragma once

#include "CoreMinimal.h"

// G9 Gameplay-owned identity for one exact authoritative player turn.  This is
// deliberately independent from PresentationSessionToken: Presentation may be
// replaced or absent while the same Gameplay turn remains authoritative.
struct SLAYTHESPIREDEMO_API FPlayerTurnAuthorityToken
{
	uint64 BattleId = 0;
	uint64 PlayerTurnSerial = 0;

	bool IsValid() const
	{
		return BattleId > 0 && PlayerTurnSerial > 0;
	}

	friend bool operator==(
		const FPlayerTurnAuthorityToken& A,
		const FPlayerTurnAuthorityToken& B)
	{
		return A.BattleId == B.BattleId
			&& A.PlayerTurnSerial == B.PlayerTurnSerial;
	}

	friend bool operator!=(
		const FPlayerTurnAuthorityToken& A,
		const FPlayerTurnAuthorityToken& B)
	{
		return !(A == B);
	}
};

enum class EGameplayRequestFailureReason : uint8
{
	None,
	InvalidBattle,
	BattleEnded,
	ResolutionFaulted,
	WrongTurn,
	ResolutionBusy,
	InvalidCard,
	CardNoLongerInHand,
	NotEnoughEnergy,
	InvalidTarget,
	QueueRejected
};

struct SLAYTHESPIREDEMO_API FGameplayValidationResult
{
	bool bAllowed = false;
	EGameplayRequestFailureReason FailureReason = EGameplayRequestFailureReason::InvalidBattle;

	static FGameplayValidationResult Allowed()
	{
		FGameplayValidationResult Result;
		Result.bAllowed = true;
		Result.FailureReason = EGameplayRequestFailureReason::None;
		return Result;
	}

	static FGameplayValidationResult Rejected(EGameplayRequestFailureReason Reason)
	{
		FGameplayValidationResult Result;
		Result.bAllowed = false;
		Result.FailureReason = Reason;
		return Result;
	}
};

enum class EGameplayRequestStatus : uint8
{
	Rejected,
	AcceptedForResolution
};

struct SLAYTHESPIREDEMO_API FGameplayRequestResult
{
	EGameplayRequestStatus Status = EGameplayRequestStatus::Rejected;
	EGameplayRequestFailureReason FailureReason = EGameplayRequestFailureReason::InvalidBattle;

	static FGameplayRequestResult Accepted()
	{
		FGameplayRequestResult Result;
		Result.Status = EGameplayRequestStatus::AcceptedForResolution;
		Result.FailureReason = EGameplayRequestFailureReason::None;
		return Result;
	}

	static FGameplayRequestResult Rejected(EGameplayRequestFailureReason Reason)
	{
		FGameplayRequestResult Result;
		Result.Status = EGameplayRequestStatus::Rejected;
		Result.FailureReason = Reason;
		return Result;
	}

	bool IsAcceptedForResolution() const
	{
		return Status == EGameplayRequestStatus::AcceptedForResolution;
	}
};
