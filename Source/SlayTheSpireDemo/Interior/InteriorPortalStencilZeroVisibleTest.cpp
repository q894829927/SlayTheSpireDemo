#include "InteriorPortal.h"
#include "InteriorPortalSystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace InteriorPortalStencilZeroVisibleTestPrivate
{
	TWeakObjectPtr<AInteriorPortal> GBluePortal;
	TWeakObjectPtr<AInteriorPortal> GOrangePortal;
	bool GBlueWasPlaced = false;
	bool GOrangeWasPlaced = false;
	bool GForcedZeroVisible = false;

	UWorld* FindPlayableWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
			{
				return World;
			}
		}
		return nullptr;
	}

	AInteriorPortalSystem* FindPortalSystem(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	void BeginZeroVisibleTest()
	{
		if (GForcedZeroVisible)
		{
			UE_LOG(LogTemp, Display,
				TEXT("PortalStencilZeroVisibleTest: already forcing zero visible apertures."));
			return;
		}

		UWorld* World = FindPlayableWorld();
		AInteriorPortalSystem* PortalSystem = FindPortalSystem(World);
		if (!IsValid(PortalSystem))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalStencilZeroVisibleTest: no playable linked portal system found."));
			return;
		}

		AInteriorPortal* Blue = PortalSystem->BluePortal.Get();
		AInteriorPortal* Orange = PortalSystem->OrangePortal.Get();
		if (!IsValid(Blue) && !IsValid(Orange))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PortalStencilZeroVisibleTest: portal endpoints are unavailable."));
			return;
		}

		GBluePortal = Blue;
		GOrangePortal = Orange;
		GBlueWasPlaced = IsValid(Blue) ? Blue->bPlaced : false;
		GOrangeWasPlaced = IsValid(Orange) ? Orange->bPlaced : false;

		if (IsValid(Blue))
		{
			Blue->bPlaced = false;
		}
		if (IsValid(Orange))
		{
			Orange->bPlaced = false;
		}

		GForcedZeroVisible = true;
		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilZeroVisibleTest: BEGIN. Temporarily forced both portal endpoints unplaced. Keep stencil identity + post-tonemap proof running; expected telemetry is VisibleApertures=0, ZeroVisibleClear=1 with ZeroVisibleClears increasing, and no cyan. Run portal.EndStencilZeroVisibleTest to restore endpoint placement."));
	}

	void EndZeroVisibleTest()
	{
		if (!GForcedZeroVisible)
		{
			UE_LOG(LogTemp, Display,
				TEXT("PortalStencilZeroVisibleTest: not active."));
			return;
		}

		if (AInteriorPortal* Blue = GBluePortal.Get())
		{
			Blue->bPlaced = GBlueWasPlaced;
		}
		if (AInteriorPortal* Orange = GOrangePortal.Get())
		{
			Orange->bPlaced = GOrangeWasPlaced;
		}

		UE_LOG(LogTemp, Display,
			TEXT("PortalStencilZeroVisibleTest: END. Restored endpoint placement Blue=%d Orange=%d. Expected stencil identity telemetry should return to VisibleApertures>0 when an endpoint is in view."),
			GBlueWasPlaced ? 1 : 0,
			GOrangeWasPlaced ? 1 : 0);

		GBluePortal.Reset();
		GOrangePortal.Reset();
		GBlueWasPlaced = false;
		GOrangeWasPlaced = false;
		GForcedZeroVisible = false;
	}

	FAutoConsoleCommand GBeginStencilZeroVisibleTestCommand(
		TEXT("portal.BeginStencilZeroVisibleTest"),
		TEXT("STEP 1B.12D-A validation helper: temporarily set both linked portal endpoints bPlaced=false so zero-visible stencil cleanup can be tested without moving the player camera."),
		FConsoleCommandDelegate::CreateStatic(&BeginZeroVisibleTest));

	FAutoConsoleCommand GEndStencilZeroVisibleTestCommand(
		TEXT("portal.EndStencilZeroVisibleTest"),
		TEXT("Restore portal endpoint placement after portal.BeginStencilZeroVisibleTest."),
		FConsoleCommandDelegate::CreateStatic(&EndZeroVisibleTest));
}
