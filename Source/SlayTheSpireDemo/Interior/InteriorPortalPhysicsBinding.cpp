#include "InteriorPortalPhysicsBinding.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"

namespace InteriorPortalPhysics
{
	EPhysicsBindingResult PreparePhysicsBinding_GameThread(const FPhysicsBindingRequest& R, FPreparedPhysicsBinding& Out)
	{
		Out = {};
		if (!IsInGameThread() || !IsValid(R.World) || !IsValid(R.Body) || !R.Registry
			|| !IsValid(R.Supports[0]) || !IsValid(R.Supports[1])
			|| R.Body == R.Supports[0] || R.Body == R.Supports[1] || R.Supports[0] == R.Supports[1]
			|| R.Body->GetWorld() != R.World || !R.Body->IsRegistered() || !R.Body->IsSimulatingPhysics()
			|| !R.Command.PairGeneration || !R.Command.Revision || !R.Command.MaxReuseSteps
			|| !FMath::IsFinite(R.Command.MaxStepSeconds) || R.Command.MaxStepSeconds <= 0
			|| !FMath::IsFinite(R.Command.MaxTranslationCm) || R.Command.MaxTranslationCm <= 0
			|| !FMath::IsFinite(R.Command.MarginCm) || R.Command.MarginCm < 0
			|| !R.Command.Entry.IsValid() || !R.Command.Exit.IsValid()
			|| !R.Command.Entry.GetScale3D().Equals(FVector::OneVector,1.e-6)
			|| !R.Command.Exit.GetScale3D().Equals(FVector::OneVector,1.e-6))
		{ return EPhysicsBindingResult::InvalidRequest; }
		for (int32 I=0; I<2; ++I)
		{
			if (R.Supports[I]->GetWorld() != R.World || !R.Supports[I]->IsRegistered())
			{ return EPhysicsBindingResult::InvalidRequest; }
			const FVector2D A = R.EndpointApertures[I];
			if (!FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y) || A.X <= 0 || A.Y <= 0
				|| A.X != R.Command.HalfWidth || A.Y != R.Command.HalfHeight)
			{ return EPhysicsBindingResult::IncompatibleAperture; }
		}
		FTravellerSnapshot Current;
		if (!R.Registry->Capture(R.Body,Current) || !(Current.Handle == R.Command.Traveller.Handle)
			|| !(Current.Geometry == R.Command.Traveller.Geometry))
		{ return EPhysicsBindingResult::StaleTraveller; }
		const FBodyInstance* Body = R.Body->GetBodyInstance();
		if (!Body || !Body->IsInstanceSimulatingPhysics() || !Body->GetPhysicsActor())
		{ return EPhysicsBindingResult::StaleTraveller; }
		FPreparedPhysicsBinding Prepared;
		Prepared.Command = R.Command;
		Prepared.Command.IsolatedStaticScope = false;
		Prepared.Command.ExitCorridorCertified = false;
		const FVector COM = R.Body->GetComponentTransform().InverseTransformPositionNoScale(Body->GetCOMPosition());
		if (COM.ContainsNaN()) { return EPhysicsBindingResult::StaleTraveller; }
		Prepared.Command.LocalAuthorityReference = COM;
		double SupportDepth = 0;
		for (int32 I=0; I<2; ++I)
		{
			if (ExtractStaticSupportGeometry(R.Supports[I],Prepared.SupportGeometry[I]) != EGeometryResult::Fits)
			{ return EPhysicsBindingResult::UnsupportedSupport; }
			const FTransform ComponentPose = R.Supports[I]->GetComponentTransform();
			// ExtractStaticSupportGeometry bakes component scale into FGeometry.
			// EvaluatePose still requires the matching component-scale identity;
			// the later native verifier uses already-scaled Chaos bounds and an
			// unscaled particle pose. Keep those two coordinate domains explicit.
			Prepared.SupportPose[I] = FTransform(ComponentPose.GetRotation(),ComponentPose.GetLocation());
			const auto Span = EvaluatePose(Prepared.SupportGeometry[I],ComponentPose,
				I == 0 ? R.Command.Entry : R.Command.Exit,1.e100,1.e100);
			if (!Span.Fits() || Span.MinNormal > 0 || Span.MaxNormal < 0
				|| !FMath::IsFinite(Span.MaxNormal))
			{ return EPhysicsBindingResult::UnsupportedSupport; }
			SupportDepth = FMath::Max(SupportDepth,Span.MaxNormal);
		}
		Prepared.Command.SupportHalfThicknessCm = SupportDepth;
		Out = MoveTemp(Prepared);
		return EPhysicsBindingResult::Ready;
	}
}
