#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace InteriorPortalFullFidelityRendererControlPrivate
{
	struct FCompositionPolicyState
	{
		bool bApplied = false;
		bool bSavedProjectiveAperture = false;
		bool bSavedDepthAwareComposition = false;
		int32 PreviousProjectiveAperture = 1;
		int32 PreviousDepthAwareComposition = 0;
	};

	FCompositionPolicyState GCompositionPolicy;

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

	void ApplyFullFidelityCompositionPolicy()
	{
		if (GCompositionPolicy.bApplied)
		{
			return;
		}

		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		if (IConsoleVariable* ProjectiveAperture = ConsoleManager.FindConsoleVariable(
			TEXT("portal.ProjectiveAperture")))
		{
			GCompositionPolicy.PreviousProjectiveAperture = ProjectiveAperture->GetInt();
			GCompositionPolicy.bSavedProjectiveAperture = true;
			ProjectiveAperture->Set(1, ECVF_SetByCode);
		}

		if (IConsoleVariable* DepthAwareComposition = ConsoleManager.FindConsoleVariable(
			TEXT("portal.DepthAwareComposition")))
		{
			GCompositionPolicy.PreviousDepthAwareComposition = DepthAwareComposition->GetInt();
			GCompositionPolicy.bSavedDepthAwareComposition = true;
			DepthAwareComposition->Set(1, ECVF_SetByCode);
		}

		GCompositionPolicy.bApplied = true;
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: composition policy ProjectiveAperture=1 DepthAwareComposition=1; real main-view foreground depth wins in front of the entry plane."));
	}

	void RestoreFullFidelityCompositionPolicy()
	{
		if (!GCompositionPolicy.bApplied)
		{
			return;
		}

		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		if (GCompositionPolicy.bSavedProjectiveAperture)
		{
			if (IConsoleVariable* ProjectiveAperture = ConsoleManager.FindConsoleVariable(
				TEXT("portal.ProjectiveAperture")))
			{
				ProjectiveAperture->Set(GCompositionPolicy.PreviousProjectiveAperture, ECVF_SetByCode);
			}
		}

		if (GCompositionPolicy.bSavedDepthAwareComposition)
		{
			if (IConsoleVariable* DepthAwareComposition = ConsoleManager.FindConsoleVariable(
				TEXT("portal.DepthAwareComposition")))
			{
				DepthAwareComposition->Set(GCompositionPolicy.PreviousDepthAwareComposition, ECVF_SetByCode);
			}
		}

		GCompositionPolicy = FCompositionPolicyState();
	}

	void StartFullFidelityRenderer()
	{
		UWorld* World = FindPlayableWorld();
		if (!World || !GEngine)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityRenderer: PIE/Game world unavailable."));
			return;
		}

		// The accepted projective aperture plus main SceneDepth foreground gate is
		// part of the production full-fidelity contract. Without it, first-person
		// geometry (for example the flashlight/portal gun) can be overwritten by
		// portal RGB even while it is physically in front of the entry plane.
		ApplyFullFidelityCompositionPolicy();

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
		if (World && GEngine)
		{
			GEngine->Exec(World, TEXT("portal.StopMultiVisibleTSRSpike"));
			UE_LOG(LogTemp, Display, TEXT("PortalFullFidelityRenderer: STOP."));
		}
		RestoreFullFidelityCompositionPolicy();
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
