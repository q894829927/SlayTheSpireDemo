#pragma once

#include "CoreMinimal.h"
#include "Presentation/PresentationTypes.h"

class ABattleManager;
class ACombatant;
class FAutomationTestBase;
class UBattleHUDViewModel;
class UBattlePresentationController;
class UCardData;
class UPhase6UIA2D5PlaybackWidget;
class UWorld;

namespace Phase6UIA2D5Test
{
	struct FCapturedEnvelope
	{
		FPresentationStateSnapshot Baseline;
		FPresentationResolutionEnvelope Envelope;
	};

	struct FAcceptanceFixture
	{
		FAcceptanceFixture();
		~FAcceptanceFixture();

		FAcceptanceFixture(const FAcceptanceFixture&) = delete;
		FAcceptanceFixture& operator=(const FAcceptanceFixture&) = delete;

		bool Start(
			const TArray<UCardData*>& Definitions,
			int32 OpeningDrawCount = 0,
			int32 PlayerDrawCount = 0,
			int32 EnemyDamage = 0,
			bool bEnablePresentation = true
		);

		bool IsReady() const;
		void Flush() const;
		bool ResetAcceptanceCapture();
		bool CompleteCurrentPlayback();
		bool DrainPlayback(int32 MaxCompletions = 128);

		const FCapturedEnvelope* LastCapturedEnvelope() const;
		const FCapturedEnvelope* FindCapturedEnvelope(int64 ResolutionId) const;

		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2D5PlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		TArray<FCapturedEnvelope> CapturedEnvelopes;

	private:
		FPresentationStateSnapshot NextEnvelopeBaseline;
		FDelegateHandle EnvelopeCaptureHandle;
		bool bCaptureAcceptanceEnvelopes = false;
	};

	bool ReplayEnvelopeWithProductionReducers(
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		FPresentationStateSnapshot& OutReducedSnapshot
	);

	bool AssertReducerOwnedStateMatchesFinalSnapshot(
		FAutomationTestBase& Test,
		const FPresentationStateSnapshot& Baseline,
		const FPresentationResolutionEnvelope& Envelope,
		const FString& Context
	);

	bool AssertCapturedEnvelopeOrder(
		FAutomationTestBase& Test,
		const TArray<FCapturedEnvelope>& Captures,
		const FString& Context
	);

	bool AssertControllerPlaybackMatchesCapturedHistory(
		FAutomationTestBase& Test,
		const TArray<FCapturedEnvelope>& Captures,
		const UPhase6UIA2D5PlaybackWidget* Widget,
		const FString& Context
	);
}
