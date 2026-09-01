#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	ABattleManager* FindActivePIEBattleManager()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (!IsValid(World) || World->WorldType != EWorldType::PIE)
			{
				continue;
			}

			for (TActorIterator<ABattleManager> It(World); It; ++It)
			{
				if (IsValid(*It))
				{
					return *It;
				}
			}
		}

		return nullptr;
	}

	void ForceResolutionFaultForR11PIE()
	{
		ABattleManager* Battle = FindActivePIEBattleManager();
		if (!IsValid(Battle))
		{
			UE_LOG(LogTemp, Error, TEXT("[R11 PIE] No active PIE BattleManager found."));
			return;
		}

		const FGameplayValidationResult Validation = Battle->QueryEndPlayerTurn();
		if (!Validation.bAllowed)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[R11 PIE] ResolutionFault injection requires a valid idle PlayerTurn EndTurn request. FailureReason=%d"),
				static_cast<int32>(Validation.FailureReason));
			return;
		}

		// Preserve the real producer path used by the sealed A2D5 fault acceptance:
		// the authoritative Player EndTurn batch commits normally, while the deferred
		// EnemyTurn batch is made structurally invalid and rejected by ActionQueue.
		Battle->SetForceInvalidEnemyTurnBatchForTesting(true);
		const FGameplayRequestResult RequestResult = Battle->RequestEndPlayerTurn();
		if (!RequestResult.IsAcceptedForResolution())
		{
			Battle->SetForceInvalidEnemyTurnBatchForTesting(false);
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[R11 PIE] EndTurn request was rejected before the ResolutionFault producer could run. FailureReason=%d"),
				static_cast<int32>(RequestResult.FailureReason));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[R11 PIE] ResolutionFault injection armed through the real EndTurn producer. Observe ordered playback and the final terminal surface."));
	}

	void ForcePresentationUnavailableForR11PIE()
	{
		ABattleManager* Battle = FindActivePIEBattleManager();
		if (!IsValid(Battle))
		{
			UE_LOG(LogTemp, Error, TEXT("[R11 PIE] No active PIE BattleManager found."));
			return;
		}

		if (!Battle->BeginSystemPresentationResolutionForTesting())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[R11 PIE] Could not begin the isolated Presentation resolution. Stop active playback and retry from a stable PIE state."));
			return;
		}

		Battle->SetForcePresentationFreezeFailureForTesting(true);
		const bool bUnexpectedlySealed = Battle->SealActivePresentationResolutionForTesting();
		Battle->SetForcePresentationFreezeFailureForTesting(false);

		if (bUnexpectedlySealed)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[R11 PIE] Forced Presentation freeze failure unexpectedly sealed an Envelope."));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[R11 PIE] PresentationUnavailable injection completed. Expect feedback/error rendering, no ResolutionFault terminal, and locked input."));
	}

	FAutoConsoleCommand GForceResolutionFaultForR11PIECommand(
		TEXT("A2N.R11.ForceResolutionFault"),
		TEXT("R11 PIE only: force a genuine framework ResolutionFault through the real EndTurn producer."),
		FConsoleCommandDelegate::CreateStatic(&ForceResolutionFaultForR11PIE));

	FAutoConsoleCommand GForcePresentationUnavailableForR11PIECommand(
		TEXT("A2N.R11.ForcePresentationUnavailable"),
		TEXT("R11 PIE only: force Presentation snapshot freeze failure and enter PresentationUnavailable."),
		FConsoleCommandDelegate::CreateStatic(&ForcePresentationUnavailableForR11PIE));
}

#endif
