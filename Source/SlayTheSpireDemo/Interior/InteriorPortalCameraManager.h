#pragma once
#include "Camera/PlayerCameraManager.h"
#include "InteriorPortalCameraManager.generated.h"

/** Small local near plane keeps the portal surface visible until the eye crosses it. */
UCLASS()
class SLAYTHESPIREDEMO_API AInteriorPortalCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
public:
	virtual void UpdateCamera(float DeltaTime) override;
};
