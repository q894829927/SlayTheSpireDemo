#pragma once

#include "CoreMinimal.h"

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
