#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace InteriorPortalFullFidelityRendererControlPrivate
{
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

	void StartFullFidelityRenderer()
	{
		UWorld* World = FindPlayableWorld();
		if (!World || !GEngine)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityRenderer: PIE/Game world unavailable."));
			return;
		}

		// The endpoint-owned multi-visible TSR path is the accepted renderer
		// correctness candidate. Keep this control surface stable while the
		// underlying validation command remains available for historical A/B.
		GEngine->Exec(World, TEXT("portal.StartMultiVisibleTSRSpike"));
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: START requested through endpoint-owned multi-visible TSR path."));
	}

	void StopFullFidelityRenderer()
	{
		UWorld* World = FindPlayableWorld();
		if (!World || !GEngine)
		{
			return;
		}

		GEngine->Exec(World, TEXT("portal.StopMultiVisibleTSRSpike"));
		UE_LOG(LogTemp, Display, TEXT("PortalFullFidelityRenderer: STOP."));
	}

	void DumpFullFidelityRenderer()
	{
		UWorld* World = FindPlayableWorld();
		if (!World || !GEngine)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityRenderer: PIE/Game world unavailable."));
			return;
		}

		GEngine->Exec(World, TEXT("portal.DumpMultiVisibleTSRSpike"));
	}

	FAutoConsoleCommand GStartFullFidelityRendererCommand(
		TEXT("portal.StartFullFidelityRenderer"),
		TEXT("Start the accepted endpoint-owned full-fidelity portal renderer candidate."),
		FConsoleCommandDelegate::CreateStatic(&StartFullFidelityRenderer));

	FAutoConsoleCommand GStopFullFidelityRendererCommand(
		TEXT("portal.StopFullFidelityRenderer"),
		TEXT("Stop the endpoint-owned full-fidelity portal renderer candidate."),
		FConsoleCommandDelegate::CreateStatic(&StopFullFidelityRenderer));

	FAutoConsoleCommand GDumpFullFidelityRendererCommand(
		TEXT("portal.DumpFullFidelityRenderer"),
		TEXT("Dump endpoint-aware full-fidelity portal renderer telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpFullFidelityRenderer));
}
