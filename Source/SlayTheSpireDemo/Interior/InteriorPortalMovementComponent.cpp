#include "InteriorPortalMovementComponent.h"
#include "InteriorPlayerController.h"
#include "InteriorPortalSystem.h"
#include "GameFramework/Character.h"

void UInteriorPortalMovementComponent::MapPortalAcceleration(const FQuat& Rotation)
{
	// Remaining movement substeps must not accelerate in the old room's input basis.
	Acceleration=FVector::VectorPlaneProject(Rotation.RotateVector(Acceleration),FVector::UpVector);
}

bool UInteriorPortalMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation,
	bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	AInteriorPlayerController* Player = CharacterOwner ? Cast<AInteriorPlayerController>(CharacterOwner->GetController()) : nullptr;
	AInteriorPortalSystem* Portals = Player ? Player->GetPortalSystem() : nullptr;
	FHitResult GateHit;
	const double Fraction = IsValid(Portals) ? Portals->ConstrainCharacterMove(CharacterOwner, Delta, GateHit) : 1.0;
	FHitResult WorldHit;
	const bool bMoved = Super::MoveUpdatedComponentImpl(Delta * Fraction, NewRotation, bSweep, &WorldHit, Teleport);
	if (WorldHit.bBlockingHit) { WorldHit.Time *= Fraction; }
	else if (Fraction < 1.0) { WorldHit = GateHit; }
	if (OutHit) { *OutHit = WorldHit; }
	if (IsValid(Portals))
	{
		Portals->UpdateCharacterTraversal(Player);
		Portals->FinishCharacterMove(CharacterOwner);
	}
	return bMoved;
}
