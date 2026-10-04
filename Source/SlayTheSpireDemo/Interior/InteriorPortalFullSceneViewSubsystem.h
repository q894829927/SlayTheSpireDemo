#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InteriorPortalFullSceneViewSubsystem.generated.h"

class AInteriorPlayerController;
class AInteriorPortalSystem;
class FInteriorPortalViewExtension;

/**
 * STEP 1B.5 feasibility driver.
 *
 * This remains an explicit opt-in diagnostic. It renders one transformed portal
 * view through a standalone full FSceneViewFamily, then hands the resulting HDR
 * render target to the already-proven BeforeDOF aperture composition extension.
 * It intentionally does not implement recursion, temporal histories, stencil or
 * depth-continuous occlusion yet.
 */
UCLASS()
class SLAYTHESPIREDEMO_API UInteriorPortalFullSceneViewSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Called after AInteriorPortalSystem::RenderViews from the local camera update. */
	void Render(AInteriorPlayerController* Player, AInteriorPortalSystem* PortalSystem);

private:
	void Deactivate();

	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> CompositionExtension;
	uint64 LastSubmittedFrame = MAX_uint64;
	uint64 LastDiagnosticsFrame = 0;
};
