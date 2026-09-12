#pragma once
#include "CoreMinimal.h"

/** Transient local-player view ownership; capsule and gravity remain upright/world-space. */
struct FInteriorPortalCameraState
{
	FQuat Orientation=FQuat::Identity;
	bool bActive=false;
	void Transfer(const FQuat& Mapping,const FQuat& OrdinaryView)
	{
		Orientation=(Mapping*(bActive?Orientation:OrdinaryView)).GetNormalized();
		bActive=true;
	}
	void ApplyInput(const FRotator& Input)
	{
		// Postmultiply so mouse axes belong to the active, possibly rolled camera.
		Orientation=(Orientation*FQuat(FVector::UpVector,FMath::DegreesToRadians(Input.Yaw))
			*FQuat(FVector::RightVector,-FMath::DegreesToRadians(Input.Pitch))).GetNormalized();
	}
	void RecoverHorizon(float DeltaSeconds)
	{
		FRotator Upright=Orientation.Rotator(); Upright.Roll=0;
		const FQuat Target=Upright.Quaternion();
		Orientation=FQuat::Slerp(Orientation,Target,1-FMath::Exp(-3*FMath::Max(0.f,DeltaSeconds))).GetNormalized();
		if (Orientation.AngularDistance(Target)<FMath::DegreesToRadians(.05f)) { Orientation=Target; bActive=false; }
	}
};
