#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PresentationTypes.h"
#include "BattlePresentationRecorder.generated.h"

class UBattlePresentationRecorder;

struct SLAYTHESPIREDEMO_API FPresentationRecordWriter
{
	bool IsAvailable() const;
	bool Append(FPresentationRecord Record) const;

	uint64 GetBattleId() const
	{
		return BattleId;
	}

	uint64 GetResolutionId() const
	{
		return ResolutionId;
	}

private:
	friend class UBattlePresentationRecorder;

	TWeakObjectPtr<UBattlePresentationRecorder> Recorder;
	uint64 BattleId = 0;
	uint64 ResolutionId = 0;
};

UCLASS(Transient)
class SLAYTHESPIREDEMO_API UBattlePresentationRecorder : public UObject
{
	GENERATED_BODY()

public:
	void ResetForBattle(uint64 InBattleId);

	bool BeginResolution(
		EPresentationResolutionOrigin Origin,
		FPresentationRecordWriter& OutWriter
	);

	void AbortResolution();

	bool SealResolution(
		const FPresentationStateSnapshot& FinalSnapshot,
		FPresentationResolutionEnvelope& OutEnvelope
	);

	bool AppendRecord(
		uint64 WriterBattleId,
		uint64 WriterResolutionId,
		FPresentationRecord Record
	);

	bool TryGetActiveWriter(FPresentationRecordWriter& OutWriter) const;
	bool HasActiveResolution() const;
	bool IsActiveResolutionValid() const;
	uint64 GetActiveResolutionId() const;
	EPresentationResolutionOrigin GetActiveOrigin() const;
	uint64 GetBattleId() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetForceNextAppendFailureForTesting(bool bForce);
	void SetForceNextSealFailureForTesting(bool bForce);
	int32 GetActiveRecordCountForTesting() const;
#endif

private:
	friend struct FPresentationRecordWriter;

	struct FActiveResolutionBuilder
	{
		bool bActive = false;
		bool bValid = true;
		uint64 BattleId = 0;
		uint64 ResolutionId = 0;
		EPresentationResolutionOrigin Origin = EPresentationResolutionOrigin::System;
		TArray<FPresentationRecord> Records;
	};

	bool IsWriterCurrentAndValid(uint64 WriterBattleId, uint64 WriterResolutionId) const;
	void ClearActiveBuilder();
	void InvalidateActiveBuilder();

	uint64 BattleId = 0;
	uint64 NextResolutionId = 1;
	uint64 NextPresentationSequence = 1;
	FActiveResolutionBuilder ActiveBuilder;

#if WITH_DEV_AUTOMATION_TESTS
	bool bForceNextAppendFailureForTesting = false;
	bool bForceNextSealFailureForTesting = false;
#endif
};
