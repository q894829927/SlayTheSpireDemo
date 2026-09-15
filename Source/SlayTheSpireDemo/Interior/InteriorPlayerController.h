#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InteriorPortalCameraState.h"
#include "InteriorPlayerController.generated.h"

class AInteriorPortalSystem;

UCLASS()
class SLAYTHESPIREDEMO_API AInteriorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AInteriorPlayerController();
	virtual void UpdateCameraManager(float DeltaSeconds) override;
	virtual void UpdateRotation(float DeltaSeconds) override;
	void ApplyPortalView(const FQuat& Mapping);
	FQuat GetPortalView() const { return PortalCamera.bActive ? PortalCamera.Orientation : GetControlRotation().Quaternion(); }
	virtual void SetupInputComponent() override;
	bool IsPortalGunEquipped() const;
	void FireBluePortal();
	AInteriorPortalSystem* GetPortalSystem() const { return PortalSystem; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	bool bPortalGunEquipped = true;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Portals")
	TObjectPtr<AInteriorPortalSystem> PortalSystem;

private:
	FInteriorPortalCameraState PortalCamera;
	void FireOrangePortal();
	void TogglePortalGun();
	void ClearPortals();

protected:
	virtual void BeginPlay() override;
};
