#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PresentationTypes.h"
#include "BattlePresentationRecorder.generated.h"

class ABattleManager;
class UBattlePresentationRecorder;

DECLARE_DELEGATE_RetVal_TwoParams(
	bool,
	FSelectionDirectOutcomeAcceptanceAccess,
	uint64, // BattleId
	int64   // SelectionBoundaryRevision
);

struct SLAYTHESPIREDEMO_API FPresentationRecordWriter
{
	bool IsAvailable() const;
	bool Append(FPresentationRecord Record) const;
	bool InvalidateCurrentResolution() const;

	// G1 metadata is writer-scoped so stale capabilities cannot attach an older
	// Selection to a newer Resolution.
	bool TryAcceptSelectionOutcome() const;
	bool TryRecordSelectionOutcome(int64 SelectionBoundaryRevision) const;
	bool TryAllocatePresentationGroupId(int64& OutGroupId) const;
	bool TryDeclarePresentationGroup(const FPresentationGroupDeclaration& Declaration) const;

	uint64 GetBattleId() const
	{
		return BattleId;
	}

	uint64 GetResolutionId() const
	{
		return ResolutionId;
	}

	int64 GetSelectionBoundaryRevision() const
	{
		return SelectionBoundaryRevision;
	}

private:
	friend class ABattleManager;
	friend class UBattlePresentationRecorder;

	TWeakObjectPtr<UBattlePresentationRecorder> Recorder;
	uint64 BattleId = 0;
	uint64 ResolutionId = 0;
	int64 SelectionBoundaryRevision = 0;
	FSelectionDirectOutcomeAcceptanceAccess DirectOutcomeAcceptance;
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
		bool bTerminalRecordAppended = false;
		uint64 BattleId = 0;
		uint64 ResolutionId = 0;
		EPresentationResolutionOrigin Origin = EPresentationResolutionOrigin::System;
		int64 NextPresentationGroupId = 1;
		TArray<FPresentationRecord> Records;
		TArray<FPresentationGroupDeclaration> PresentationGroups;
		TArray<FSelectionPresentationOutcomeReceipt> SelectionOutcomes;
	};

	static bool IsTerminalRecordType(EBattlePresentationRecordType Type);
	bool IsWriterCurrentAndValid(uint64 WriterBattleId, uint64 WriterResolutionId) const;
	bool InvalidateWriterResolution(uint64 WriterBattleId, uint64 WriterResolutionId);
	bool RecordSelectionOutcome(uint64 WriterBattleId, uint64 WriterResolutionId, int64 SelectionBoundaryRevision);
	bool AllocatePresentationGroupId(uint64 WriterBattleId, uint64 WriterResolutionId, int64& OutGroupId);
	bool DeclarePresentationGroup(uint64 WriterBattleId, uint64 WriterResolutionId, const FPresentationGroupDeclaration& Declaration);
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
