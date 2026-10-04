#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InteriorPortalMovementComponent.generated.h"

/** Enforces the portal aperture on every CharacterMovement sweep, including slide/step submoves. */
UCLASS()
class SLAYTHESPIREDEMO_API UInteriorPortalMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	void MapPortalAcceleration(const FQuat& Rotation);
protected:
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep,
		FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None) override;
};
