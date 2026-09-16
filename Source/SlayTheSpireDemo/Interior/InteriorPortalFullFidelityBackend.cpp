#include "InteriorPortalFullFidelityBackend.h"

#include "Engine/World.h"

// The accepted renderer still lives in the historical validation translation unit.
// These declarations intentionally expose only its lifecycle functions; the renderer
// implementation and old console aliases remain private to that file.
namespace InteriorPortalMultiVisibleTSRPrivate
{
	void StartMultiVisible();
	void StopMultiVisible();
	void DumpMultiVisible();
}

namespace InteriorPortalFullFidelityBackend
{
	bool Start(UWorld* World)
	{
		if (!IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("PortalFullFidelityBackend: PIE/Game world unavailable."));
			return false;
		}

		InteriorPortalMultiVisibleTSRPrivate::StartMultiVisible();
		return true;
	}

	void Stop()
	{
		InteriorPortalMultiVisibleTSRPrivate::StopMultiVisible();
	}

	void Dump()
	{
		InteriorPortalMultiVisibleTSRPrivate::DumpMultiVisible();
	}
}
