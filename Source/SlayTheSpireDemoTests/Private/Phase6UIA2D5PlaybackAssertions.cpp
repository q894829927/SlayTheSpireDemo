#include "Phase6UIA2D5TestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestTypes.h"
#include "Misc/AutomationTest.h"

namespace Phase6UIA2D5Test
{
	bool AssertControllerPlaybackMatchesCapturedHistory(
		FAutomationTestBase& Test,
		const TArray<FCapturedEnvelope>& Captures,
		const UPhase6UIA2D5PlaybackWidget* Widget,
		const FString& Context
	)
	{
		if (!IsValid(Widget))
		{
			Test.AddError(FString::Printf(TEXT("%s playback widget is invalid."), *Context));
			return false;
		}

		int32 ExpectedRecordCount = 0;
		for (const FCapturedEnvelope& Capture : Captures)
		{
			ExpectedRecordCount += Capture.Envelope.Records.Num();
		}

		bool bOk = true;
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s visible playback call count"), *Context),
			Widget->PlayCallCount,
			ExpectedRecordCount
		);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s played record count"), *Context),
			Widget->PlayedRecords.Num(),
			ExpectedRecordCount
		);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s played token count"), *Context),
			Widget->PlayedTokens.Num(),
			ExpectedRecordCount
		);

		const int32 ComparableCount = FMath::Min(
			ExpectedRecordCount,
			FMath::Min(Widget->PlayedRecords.Num(), Widget->PlayedTokens.Num())
		);

		int32 ActualIndex = 0;
		for (const FCapturedEnvelope& Capture : Captures)
		{
			for (const FPresentationRecord& ExpectedRecord : Capture.Envelope.Records)
			{
				if (ActualIndex >= ComparableCount)
				{
					return false;
				}

				const FPresentationRecord& PlayedRecord = Widget->PlayedRecords[ActualIndex];
				const FPresentationPlaybackToken& PlayedToken = Widget->PlayedTokens[ActualIndex];
				const FString Item = FString::Printf(TEXT("%s playback[%d]"), *Context, ActualIndex);

				bOk &= Test.TestTrue(
					*FString::Printf(TEXT("%s record type"), *Item),
					PlayedRecord.Type == ExpectedRecord.Type
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s record BattleId"), *Item),
					PlayedRecord.BattleId,
					ExpectedRecord.BattleId
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s record ResolutionId"), *Item),
					PlayedRecord.ResolutionId,
					ExpectedRecord.ResolutionId
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s record PresentationSequence"), *Item),
					PlayedRecord.PresentationSequence,
					ExpectedRecord.PresentationSequence
				);

				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s token BattleId"), *Item),
					PlayedToken.BattleId,
					ExpectedRecord.BattleId
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s token ResolutionId"), *Item),
					PlayedToken.ResolutionId,
					ExpectedRecord.ResolutionId
				);
				bOk &= Test.TestEqual(
					*FString::Printf(TEXT("%s token PresentationSequence"), *Item),
					PlayedToken.PresentationSequence,
					ExpectedRecord.PresentationSequence
				);
				bOk &= Test.TestTrue(
					*FString::Printf(TEXT("%s token LocalPlaybackGeneration positive"), *Item),
					PlayedToken.LocalPlaybackGeneration > 0
				);

				++ActualIndex;
			}
		}

		return bOk && ActualIndex == ExpectedRecordCount;
	}
}

#endif
