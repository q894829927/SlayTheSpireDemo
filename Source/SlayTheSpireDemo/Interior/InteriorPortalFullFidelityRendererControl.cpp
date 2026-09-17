#include "InteriorPortalFullFidelityRendererControl.h"

#include "InteriorPortalFullFidelityBackend.h"
#include "InteriorPortalSystem.h"
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
		if (IConsoleVariable* ProjectiveAperture = ConsoleManager.FindConsoleVariable(TEXT("portal.ProjectiveAperture")))
		{
			GCompositionPolicy.PreviousProjectiveAperture = ProjectiveAperture->GetInt();
			GCompositionPolicy.bSavedProjectiveAperture = true;
			ProjectiveAperture->Set(1, ECVF_SetByCode);
		}
		if (IConsoleVariable* DepthAwareComposition = ConsoleManager.FindConsoleVariable(TEXT("portal.DepthAwareComposition")))
		{
			GCompositionPolicy.PreviousDepthAwareComposition = DepthAwareComposition->GetInt();
			GCompositionPolicy.bSavedDepthAwareComposition = true;
			DepthAwareComposition->Set(1, ECVF_SetByCode);
		}
		GCompositionPolicy.bApplied = true;
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: production composition policy active (analytic aperture + main-depth foreground preservation)."));
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
			if (IConsoleVariable* ProjectiveAperture = ConsoleManager.FindConsoleVariable(TEXT("portal.ProjectiveAperture")))
			{
				ProjectiveAperture->Set(GCompositionPolicy.PreviousProjectiveAperture, ECVF_SetByCode);
			}
		}
		if (GCompositionPolicy.bSavedDepthAwareComposition)
		{
			if (IConsoleVariable* DepthAwareComposition = ConsoleManager.FindConsoleVariable(TEXT("portal.DepthAwareComposition")))
			{
				DepthAwareComposition->Set(GCompositionPolicy.PreviousDepthAwareComposition, ECVF_SetByCode);
			}
		}
		GCompositionPolicy = FCompositionPolicyState();
	}

	void StartFromConsole() { InteriorPortalFullFidelityRenderer::Start(FindPlayableWorld()); }
	void StopFromConsole() { InteriorPortalFullFidelityRenderer::Stop(FindPlayableWorld()); }
	void DumpFromConsole() { InteriorPortalFullFidelityRenderer::Dump(FindPlayableWorld()); }
}

namespace InteriorPortalFullFidelityRenderer
{
	bool ShouldOwnRendering(
		const bool bUseFullFidelityRenderer,
		const EInteriorPortalRendererBackend RendererBackend)
	{
		return bUseFullFidelityRenderer
			&& AInteriorPortalSystem::UsesSceneCapture(RendererBackend);
	}

	bool ShouldOwnRendering(const AInteriorPortalSystem* PortalSystem)
	{
		return IsValid(PortalSystem)
			&& ShouldOwnRendering(
				PortalSystem->bUseFullFidelityRenderer,
				PortalSystem->RendererBackend);
	}

	bool Start(UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityRenderer: PIE/Game world unavailable."));
			return false;
		}

		InteriorPortalFullFidelityRendererControlPrivate::ApplyFullFidelityCompositionPolicy();
		if (!InteriorPortalFullFidelityBackend::Start(World))
		{
			InteriorPortalFullFidelityRendererControlPrivate::RestoreFullFidelityCompositionPolicy();
			return false;
		}

		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: START through native lifecycle and backend APIs."));
		return true;
	}

	void Stop(UWorld* World)
	{
		(void)World;
		InteriorPortalFullFidelityBackend::Stop();
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: STOP through native lifecycle and backend APIs."));
		InteriorPortalFullFidelityRendererControlPrivate::RestoreFullFidelityCompositionPolicy();
	}

	void Dump(UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityRenderer: PIE/Game world unavailable."));
			return;
		}
		InteriorPortalFullFidelityBackend::Dump();
	}
}

namespace InteriorPortalFullFidelityRendererControlPrivate
{
	FAutoConsoleCommand GStartFullFidelityRendererCommand(
		TEXT("portal.StartFullFidelityRenderer"),
		TEXT("Start the accepted endpoint-owned FullFidelity portal renderer."),
		FConsoleCommandDelegate::CreateStatic(&StartFromConsole));
	FAutoConsoleCommand GStopFullFidelityRendererCommand(
		TEXT("portal.StopFullFidelityRenderer"),
		TEXT("Stop the accepted endpoint-owned FullFidelity portal renderer."),
		FConsoleCommandDelegate::CreateStatic(&StopFromConsole));
	FAutoConsoleCommand GDumpFullFidelityRendererCommand(
		TEXT("portal.DumpFullFidelityRenderer"),
		TEXT("Dump endpoint/recursion-aware FullFidelity renderer telemetry."),
		FConsoleCommandDelegate::CreateStatic(&DumpFromConsole));
}
