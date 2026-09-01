#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PlayInEditorDataTypes.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDPresenter.h"
#include "UI/BattleHUDViewModel.h"
#include "UI/BattleHUDWidgetBase.h"
#include "UnrealEdGlobals.h"

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
			if (!IsValid(World)
				|| (World->WorldType != EWorldType::PIE
					&& World->WorldType != EWorldType::Game))
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

	struct FR11PIEHUDContext
	{
		TWeakObjectPtr<ABattleManager> Battle;
		TWeakObjectPtr<UBattleHUDWidgetBase> Widget;
		TWeakObjectPtr<UBattleHUDViewModel> ViewModel;
		TWeakObjectPtr<UBattlePresentationController> Controller;
	};

	bool TryFindActivePIEHUDContext(FR11PIEHUDContext& OutContext)
	{
		OutContext = FR11PIEHUDContext{};
		ABattleManager* Battle = FindActivePIEBattleManager();
		if (!IsValid(Battle))
		{
			return false;
		}

		UWorld* World = Battle->GetWorld();
		for (TActorIterator<ABattleHUDPresenter> It(World); It; ++It)
		{
			ABattleHUDPresenter* Presenter = *It;
			if (!IsValid(Presenter)
				|| Presenter->BattleManager != Battle
				|| !IsValid(Presenter->WidgetInstance)
				|| !IsValid(Presenter->ViewModel)
				|| !IsValid(Presenter->PresentationController))
			{
				continue;
			}

			OutContext.Battle = Battle;
			OutContext.Widget = Presenter->WidgetInstance;
			OutContext.ViewModel = Presenter->ViewModel;
			OutContext.Controller = Presenter->PresentationController;
			return true;
		}

		return false;
	}

	bool DoCardViewsMatch(
		const TArray<FBattleHUDCardView>& Actual,
		const TArray<FBattleHUDCardView>& Expected)
	{
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Actual.Num(); ++Index)
		{
			if (Actual[Index].RuntimeId != Expected[Index].RuntimeId
				|| Actual[Index].CardId != Expected[Index].CardId
				|| !Actual[Index].DisplayName.EqualTo(Expected[Index].DisplayName)
				|| Actual[Index].Cost != Expected[Index].Cost
				|| Actual[Index].CardType != Expected[Index].CardType
				|| Actual[Index].TargetType != Expected[Index].TargetType
				|| !Actual[Index].Description.EqualTo(Expected[Index].Description)
				|| Actual[Index].CardArt != Expected[Index].CardArt
				|| Actual[Index].bGameplayPlayable != Expected[Index].bGameplayPlayable
				|| !Actual[Index].UnplayableReason.EqualTo(Expected[Index].UnplayableReason))
			{
				return false;
			}
		}

		return true;
	}

	bool DoStatusViewsMatch(
		const TArray<FBattleHUDStatusView>& Actual,
		const TArray<FBattleHUDStatusView>& Expected)
	{
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Actual.Num(); ++Index)
		{
			if (Actual[Index].StatusId != Expected[Index].StatusId
				|| Actual[Index].RuntimeSequence != Expected[Index].RuntimeSequence
				|| !Actual[Index].DisplayName.EqualTo(Expected[Index].DisplayName)
				|| !Actual[Index].Description.EqualTo(Expected[Index].Description)
				|| Actual[Index].Amount != Expected[Index].Amount
				|| Actual[Index].bUseAtlasIcon != Expected[Index].bUseAtlasIcon
				|| Actual[Index].UVOffset != Expected[Index].UVOffset
				|| Actual[Index].UVScale != Expected[Index].UVScale
				|| Actual[Index].TrimOffset != Expected[Index].TrimOffset
				|| Actual[Index].TrimScale != Expected[Index].TrimScale)
			{
				return false;
			}
		}

		return true;
	}

	bool DoesCombatantViewMatch(
		const FBattleHUDCombatantView& Actual,
		const FBattleHUDCombatantView& Expected)
	{
		return Actual.PresentationId == Expected.PresentationId
			&& Actual.bPlayer == Expected.bPlayer
			&& Actual.DisplayName.EqualTo(Expected.DisplayName)
			&& Actual.HP == Expected.HP
			&& Actual.MaxHP == Expected.MaxHP
			&& Actual.Block == Expected.Block
			&& Actual.bDead == Expected.bDead
			&& DoStatusViewsMatch(Actual.Statuses, Expected.Statuses);
	}

	bool DoesIntentViewMatch(
		const FBattleHUDIntentView& Actual,
		const FBattleHUDIntentView& Expected)
	{
		return Actual.Type == Expected.Type
			&& Actual.DisplayName.EqualTo(Expected.DisplayName)
			&& Actual.BaseAmount == Expected.BaseAmount
			&& Actual.bHasCurrentResolvedDamageAmount == Expected.bHasCurrentResolvedDamageAmount
			&& Actual.CurrentResolvedDamageAmount == Expected.CurrentResolvedDamageAmount;
	}

	bool DoesViewModelMatchSnapshot(
		const UBattleHUDViewModel* ViewModel,
		const FPresentationStateSnapshot& Snapshot)
	{
		return IsValid(ViewModel)
			&& ViewModel->BattleId == Snapshot.BattleId
			&& ViewModel->StateRevision == Snapshot.StateRevision
			&& ViewModel->Outcome == Snapshot.Outcome
			&& ViewModel->Energy == Snapshot.Energy
			&& ViewModel->MaxEnergy == Snapshot.MaxEnergy
			&& ViewModel->bCanEndTurn == Snapshot.bCanEndTurn
			&& ViewModel->DrawCount == Snapshot.DrawCount
			&& ViewModel->DiscardCount == Snapshot.DiscardCount
			&& ViewModel->ExhaustCount == Snapshot.ExhaustCount
			&& DoesCombatantViewMatch(ViewModel->Player, Snapshot.Player)
			&& DoesCombatantViewMatch(ViewModel->Enemy, Snapshot.Enemy)
			&& DoCardViewsMatch(ViewModel->HandCards, Snapshot.HandCards)
			&& DoesIntentViewMatch(ViewModel->EnemyIntent, Snapshot.EnemyIntent);
	}

	enum class ER11TemporalTestMode : uint8
	{
		Skip,
		CancelStale
	};

	enum class ER11TemporalTestStage : uint8
	{
		WaitForTokenA,
		WaitForTokenB,
		WaitForStaleDelivery,
		WaitForCatchUp
	};

	struct FR11TemporalPIETestState
	{
		ER11TemporalTestMode Mode = ER11TemporalTestMode::Skip;
		ER11TemporalTestStage Stage = ER11TemporalTestStage::WaitForTokenA;
		FR11PIEHUDContext Context;
		FPresentationPlaybackToken TokenA;
		FPresentationPlaybackToken TokenB;
		FString StackName;
		double DeadlineSeconds = 0.0;
		int32 StaleDeliveryTicks = 0;
	};

	TSharedPtr<FR11TemporalPIETestState> GActiveTemporalPIETest;
	bool GTemporalTestAutoExit = false;

	const TCHAR* GetTemporalTestName(ER11TemporalTestMode Mode)
	{
		return Mode == ER11TemporalTestMode::Skip
			? TEXT("TestSkip")
			: TEXT("TestCancelStale");
	}

	void FinishTemporalPIETest(
		const TSharedRef<FR11TemporalPIETestState>& State,
		bool bPassed,
		const FString& Detail)
	{
		if (bPassed)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[R11 PIE][%s][%s] PASS: %s"),
				GetTemporalTestName(State->Mode),
				*State->StackName,
				*Detail);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[R11 PIE][%s][%s] FAIL: %s"),
				GetTemporalTestName(State->Mode),
				*State->StackName,
				*Detail);
		}
		GActiveTemporalPIETest.Reset();
		if (GTemporalTestAutoExit)
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	bool TickTemporalPIETest(
		const TSharedRef<FR11TemporalPIETestState>& State,
		float /*DeltaTime*/)
	{
		ABattleManager* Battle = State->Context.Battle.Get();
		UBattleHUDWidgetBase* Widget = State->Context.Widget.Get();
		UBattleHUDViewModel* ViewModel = State->Context.ViewModel.Get();
		UBattlePresentationController* Controller = State->Context.Controller.Get();
		if (!IsValid(Battle) || !IsValid(Widget) || !IsValid(ViewModel) || !IsValid(Controller))
		{
			FinishTemporalPIETest(State, false, TEXT("PIE HUD context became invalid."));
			return false;
		}
		if (FPlatformTime::Seconds() > State->DeadlineSeconds)
		{
			FinishTemporalPIETest(State, false, TEXT("Timed out waiting for the required playback/catch-up state."));
			return false;
		}

		switch (State->Stage)
		{
		case ER11TemporalTestStage::WaitForTokenA:
			if (!Controller->IsWaitingForCompletionForTesting())
			{
				return true;
			}
			State->TokenA = Controller->GetActivePlaybackTokenForTesting();
			if (State->TokenA.PresentationSequence <= 0)
			{
				FinishTemporalPIETest(State, false, TEXT("Controller reported waiting without a valid active Token A."));
				return false;
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("[R11 PIE][%s][%s] Active Token A captured: Resolution=%lld Sequence=%lld Generation=%lld."),
				GetTemporalTestName(State->Mode),
				*State->StackName,
				State->TokenA.ResolutionId,
				State->TokenA.PresentationSequence,
				State->TokenA.LocalPlaybackGeneration);

			if (State->Mode == ER11TemporalTestMode::Skip)
			{
				Widget->SkipPresentation();
				if (Controller->IsWaitingForCompletionForTesting()
					|| Controller->GetBacklogCountForTesting() != 0)
				{
					FinishTemporalPIETest(State, false, TEXT("Skip did not synchronously clear active playback/backlog."));
					return false;
				}
				State->Stage = ER11TemporalTestStage::WaitForCatchUp;
				return true;
			}

			Controller->ExpireActivePlaybackForTesting();
			State->Stage = ER11TemporalTestStage::WaitForTokenB;
			return true;

		case ER11TemporalTestStage::WaitForTokenB:
			if (!Controller->IsWaitingForCompletionForTesting())
			{
				return true;
			}
			State->TokenB = Controller->GetActivePlaybackTokenForTesting();
			if (State->TokenB == State->TokenA)
			{
				return true;
			}
			if (State->TokenB.PresentationSequence <= 0)
			{
				FinishTemporalPIETest(State, false, TEXT("Controller advanced to an invalid Token B."));
				return false;
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("[R11 PIE][TestCancelStale][%s] Exact timeout advanced Token A to Token B; injecting stale Token A callback."),
				*State->StackName);
			Widget->NotifyPresentationFinished(State->TokenA);
			State->StaleDeliveryTicks = 0;
			State->Stage = ER11TemporalTestStage::WaitForStaleDelivery;
			return true;

		case ER11TemporalTestStage::WaitForStaleDelivery:
			++State->StaleDeliveryTicks;
			if (State->StaleDeliveryTicks < 2)
			{
				return true;
			}
			if (!Controller->IsWaitingForCompletionForTesting()
				|| Controller->GetActivePlaybackTokenForTesting() != State->TokenB)
			{
				FinishTemporalPIETest(State, false, TEXT("Stale Token A callback disturbed or prematurely finished Token B."));
				return false;
			}
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[R11 PIE][TestCancelStale][%s] Stale Token A callback was an exact no-op; Token B remains active."),
				*State->StackName);
			State->Stage = ER11TemporalTestStage::WaitForCatchUp;
			return true;

		case ER11TemporalTestStage::WaitForCatchUp:
			if (Controller->IsWaitingForCompletionForTesting()
				|| Controller->GetBacklogCountForTesting() != 0)
			{
				return true;
			}

			FPresentationStateSnapshot LatestSnapshot;
			if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestSnapshot))
			{
				FinishTemporalPIETest(State, false, TEXT("Could not read the latest frozen FinalSnapshot."));
				return false;
			}
			if (!DoesViewModelMatchSnapshot(ViewModel, LatestSnapshot))
			{
				FinishTemporalPIETest(State, false, TEXT("Caught-up ViewModel does not exactly match the latest FinalSnapshot."));
				return false;
			}
			if (Controller->GetLastCompletedResolutionIdForTesting()
				< static_cast<int64>(Battle->GetLatestFrozenPresentationBaselineResolutionId()))
			{
				FinishTemporalPIETest(State, false, TEXT("Controller completion watermark did not catch up."));
				return false;
			}
			if (ViewModel->bInputLocked
				|| ViewModel->InteractionState != EBattleHUDInteractionState::Idle
				|| !ViewModel->bCanEndTurn
				|| !Battle->QueryEndPlayerTurn().bAllowed)
			{
				FinishTemporalPIETest(State, false, TEXT("Input did not unlock at the caught-up PlayerTurn."));
				return false;
			}

			if (!Widget->EndTurn())
			{
				FinishTemporalPIETest(State, false, TEXT("A real post-catch-up Widget EndTurn request was rejected."));
				return false;
			}
			// The accepted second request proves live input binding restoration. Skip
			// its playback immediately so the temporary harness leaves a stable PIE.
			Widget->SkipPresentation();
			FinishTemporalPIETest(
				State,
				true,
				TEXT("active playback observed; catch-up matched FinalSnapshot; queue caught up; stale ownership was isolated where applicable; real EndTurn input succeeded after unlock."));
			return false;
		}

		FinishTemporalPIETest(State, false, TEXT("Reached an unknown temporal-test stage."));
		return false;
	}

	void StartTemporalPIETest(ER11TemporalTestMode Mode)
	{
		if (GActiveTemporalPIETest.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[R11 PIE] A temporal test is already running."));
			return;
		}

		FR11PIEHUDContext Context;
		if (!TryFindActivePIEHUDContext(Context))
		{
			UE_LOG(LogTemp, Error, TEXT("[R11 PIE][%s] No complete active PIE HUD/Controller context found."), GetTemporalTestName(Mode));
			return;
		}

		ABattleManager* Battle = Context.Battle.Get();
		UBattleHUDWidgetBase* Widget = Context.Widget.Get();
		if (!Battle->QueryEndPlayerTurn().bAllowed)
		{
			UE_LOG(LogTemp, Error, TEXT("[R11 PIE][%s] Requires an idle PlayerTurn with legal EndTurn input."), GetTemporalTestName(Mode));
			return;
		}
		if (!Widget->EndTurn())
		{
			UE_LOG(LogTemp, Error, TEXT("[R11 PIE][%s] Real Widget EndTurn request was rejected."), GetTemporalTestName(Mode));
			return;
		}

		GActiveTemporalPIETest = MakeShared<FR11TemporalPIETestState>();
		GActiveTemporalPIETest->Mode = Mode;
		GActiveTemporalPIETest->Context = Context;
		GActiveTemporalPIETest->StackName = Widget->GetClass()->GetName();
		GActiveTemporalPIETest->DeadlineSeconds = FPlatformTime::Seconds() + 30.0;
		const TSharedRef<FR11TemporalPIETestState> State = GActiveTemporalPIETest.ToSharedRef();
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[State](float DeltaTime)
				{
					return TickTemporalPIETest(State, DeltaTime);
				}),
			0.0f);

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[R11 PIE][%s][%s] Started through a real Widget EndTurn request; waiting for active Token A."),
			GetTemporalTestName(Mode),
			*GActiveTemporalPIETest->StackName);
	}

	void TestSkipForR11PIE()
	{
		StartTemporalPIETest(ER11TemporalTestMode::Skip);
	}

	void TestCancelStaleForR11PIE()
	{
		StartTemporalPIETest(ER11TemporalTestMode::CancelStale);
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

	FAutoConsoleCommand GTestSkipForR11PIECommand(
		TEXT("A2N.R11.TestSkip"),
		TEXT("R11 PIE only: issue real EndTurn, wait for active playback, Skip, verify FinalSnapshot catch-up and input unlock."),
		FConsoleCommandDelegate::CreateStatic(&TestSkipForR11PIE));

	FAutoConsoleCommand GTestCancelStaleForR11PIECommand(
		TEXT("A2N.R11.TestCancelStale"),
		TEXT("R11 PIE only: issue real EndTurn, timeout Token A, inject stale callback after Token B, then verify catch-up/input unlock."),
		FConsoleCommandDelegate::CreateStatic(&TestCancelStaleForR11PIE));

	struct FR11TemporalCommandLineBootstrap
	{
		FR11TemporalCommandLineBootstrap()
		{
			FString RequestedTest;
			if (!FParse::Value(FCommandLine::Get(), TEXT("R11TemporalTest="), RequestedTest))
			{
				return;
			}

			ER11TemporalTestMode Mode;
			if (RequestedTest.Equals(TEXT("Skip"), ESearchCase::IgnoreCase))
			{
				Mode = ER11TemporalTestMode::Skip;
			}
			else if (RequestedTest.Equals(TEXT("CancelStale"), ESearchCase::IgnoreCase))
			{
				Mode = ER11TemporalTestMode::CancelStale;
			}
			else
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[R11 PIE][CommandLineBootstrap] Unknown R11TemporalTest value '%s'."),
					*RequestedTest);
				return;
			}

			GTemporalTestAutoExit = FParse::Param(FCommandLine::Get(), TEXT("R11TemporalAutoExit"));
			FString RequestedMap;
			FParse::Value(FCommandLine::Get(), TEXT("R11TemporalMap="), RequestedMap);
			const double BootstrapDeadlineSeconds = FPlatformTime::Seconds() + 45.0;
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda(
					[Mode, RequestedMap, BootstrapDeadlineSeconds, bPlayRequested = false](float /*DeltaTime*/) mutable
					{
						FR11PIEHUDContext Context;
						if (TryFindActivePIEHUDContext(Context)
							&& IsValid(Context.Battle.Get())
							&& IsValid(Context.Widget.Get())
							&& IsValid(Context.ViewModel.Get())
							&& IsValid(Context.Controller.Get())
							&& Context.Widget->ViewModel == Context.ViewModel.Get()
							&& Context.Widget->PresentationController == Context.Controller.Get()
							&& !Context.ViewModel->bInputLocked
							&& Context.ViewModel->InteractionState == EBattleHUDInteractionState::Idle
							&& Context.ViewModel->bCanEndTurn
							&& !Context.Controller->IsWaitingForCompletionForTesting()
							&& Context.Controller->GetBacklogCountForTesting() == 0
							&& Context.Battle->QueryEndPlayerTurn().bAllowed)
						{
							StartTemporalPIETest(Mode);
							return false;
						}

						if (!bPlayRequested
							&& !RequestedMap.IsEmpty()
							&& GUnrealEd != nullptr
							&& GUnrealEd->PlayWorld == nullptr
							&& !GUnrealEd->GetPlaySessionRequest().IsSet())
						{
							FRequestPlaySessionParams PlaySessionParams;
							PlaySessionParams.GlobalMapOverride = RequestedMap;
							PlaySessionParams.bAllowOnlineSubsystem = false;
							GUnrealEd->RequestPlaySession(PlaySessionParams);
							bPlayRequested = true;
							UE_LOG(
								LogTemp,
								Display,
								TEXT("[R11 PIE][%s][CommandLineBootstrap] Requested in-process PIE for %s."),
								GetTemporalTestName(Mode),
								*RequestedMap);
						}

						if (FPlatformTime::Seconds() <= BootstrapDeadlineSeconds)
						{
							return true;
						}

						UE_LOG(
							LogTemp,
							Error,
							TEXT("[R11 PIE][%s][CommandLineBootstrap] FAIL: Timed out waiting for an idle PlayerTurn HUD context."),
							GetTemporalTestName(Mode));
						if (GTemporalTestAutoExit)
						{
							FPlatformMisc::RequestExit(false);
						}
						return false;
					}),
				0.0f);
		}
	};

	FR11TemporalCommandLineBootstrap GTemporalCommandLineBootstrap;
}

#endif
