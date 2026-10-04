#include "InteriorPortalWorldPassageQuery.h"
#include "InteriorPortalMath.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

namespace InteriorPortalPhysics
{
	FWorldPassageObservation ObserveWorldPassage(const FWorldPassageRequest& R, const FTravellerRegistry& Registry)
	{
		FWorldPassageObservation O;
		O.Body = R.Traveller.Handle; O.PairGeneration = R.PairGeneration; O.Revision = R.Revision;
		if (!IsInGameThread() || !IsValid(R.World) || !IsValid(R.Body)
			|| !IsValid(R.EntrySupport) || !IsValid(R.ExitSupport)
			|| R.Body == R.EntrySupport || R.Body == R.ExitSupport || R.EntrySupport == R.ExitSupport
			|| R.Body->GetWorld() != R.World || R.EntrySupport->GetWorld() != R.World || R.ExitSupport->GetWorld() != R.World
			|| !R.Body->IsRegistered() || !R.EntrySupport->IsRegistered() || !R.ExitSupport->IsRegistered()
			|| !R.PairGeneration || !R.Revision || R.LocalCOM.ContainsNaN()
			|| !FMath::IsFinite(R.MarginCm) || R.MarginCm < 0
			|| !FMath::IsFinite(R.MaxIntervalTranslationCm) || R.MaxIntervalTranslationCm <= 0)
		{ return O; }
		FTravellerSnapshot Current; FGeometry Geometry;
		if (!Registry.Capture(R.Body, Current) || !Registry.IsCurrent(R.Traveller.Handle)
			|| !(Current.Handle == R.Traveller.Handle) || !(Current.Geometry == R.Traveller.Geometry)
			|| ExtractGeometry(R.Body, Geometry) != EGeometryResult::Fits || !(Geometry == Current.Geometry))
		{ O.Result = EWorldPassageResult::StaleTraveller; return O; }
		FGeometry Supports[2];
		if (ExtractStaticSupportGeometry(R.EntrySupport, Supports[0]) != EGeometryResult::Fits
			|| ExtractStaticSupportGeometry(R.ExitSupport, Supports[1]) != EGeometryResult::Fits)
		{ O.Result = EWorldPassageResult::UnsupportedSupport; return O; }
		const auto SourceFit = EvaluateTranslation(Geometry, R.From, R.To, R.Entry, R.HalfWidth, R.HalfHeight, R.MarginCm);
		if (!SourceFit.Fits())
		{
			O.Result = SourceFit.Result == EGeometryResult::RotationSweepUnsupported
				? EWorldPassageResult::UnsupportedMotion : EWorldPassageResult::OutsideAperture;
			return O;
		}
		if (FVector::Dist(R.From.GetLocation(), R.To.GetLocation()) > R.MaxIntervalTranslationCm
			|| R.Entry.InverseTransformPositionNoScale(R.From.TransformPositionNoScale(R.LocalCOM)).X <= 0)
		{ O.Result = EWorldPassageResult::UnsupportedMotion; return O; }
		const auto EntrySpan = EvaluatePose(Supports[0], R.EntrySupport->GetComponentTransform(), R.Entry, 1.e100, 1.e100);
		const auto ExitSpan = EvaluatePose(Supports[1], R.ExitSupport->GetComponentTransform(), R.Exit, 1.e100, 1.e100);
		if (!EntrySpan.Fits() || !ExitSpan.Fits() || EntrySpan.MinNormal > 0 || EntrySpan.MaxNormal < 0
			|| ExitSpan.MinNormal > 0 || ExitSpan.MaxNormal < 0)
		{ O.Result = EWorldPassageResult::UnsupportedSupport; return O; }
		O.EntryOutwardDepthCm = EntrySpan.MaxNormal; O.ExitOutwardDepthCm = ExitSpan.MaxNormal;
		// Fixed lateral pose at the requested endpoint, extended normally to the COM crossing plane.
		O.SourceAtPlane = R.To;
		O.SourceAtPlane.AddToTranslation(-R.Entry.GetUnitAxis(EAxis::X)
			* R.Entry.InverseTransformPositionNoScale(R.To.TransformPositionNoScale(R.LocalCOM)).X);
		const FQuat Map = InteriorPortalMath::Rotation(R.Entry, R.Exit);
		const auto Mapped = [&](const FTransform& P)
		{ return FTransform(Map * P.GetRotation(), InteriorPortalMath::Position(P.GetLocation(), R.Entry, R.Exit), Geometry.BakedScale); };
		O.DestinationAtPlane = Mapped(O.SourceAtPlane);
		const FTransform MappedTo = Mapped(R.To);
		const auto DestinationSpan = EvaluatePose(Geometry, O.DestinationAtPlane, R.Exit, R.ExitHalfWidth, R.ExitHalfHeight, R.MarginCm);
		O.DestinationClear = O.DestinationAtPlane;
		O.DestinationClear.AddToTranslation(R.Exit.GetUnitAxis(EAxis::X)
			* (FMath::Max(0., ExitSpan.MaxNormal - DestinationSpan.MinNormal + R.MarginCm) + R.MaxIntervalTranslationCm));
		if (!EvaluateTranslation(Geometry, R.To, O.SourceAtPlane, R.Entry, R.HalfWidth, R.HalfHeight, R.MarginCm).Fits()
			|| !EvaluateTranslation(Geometry, Mapped(R.From), MappedTo, R.Exit, R.ExitHalfWidth, R.ExitHalfHeight, R.MarginCm).Fits()
			|| !EvaluateTranslation(Geometry, O.DestinationAtPlane, O.DestinationClear, R.Exit, R.ExitHalfWidth, R.ExitHalfHeight, R.MarginCm).Fits())
		{ O.Result = EWorldPassageResult::OutsideAperture; return O; }
		// No actor-wide ignore: other colliders on the held body's owner still block.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalWorldPassage), false);
		Params.AddIgnoredComponent(R.Body);
		const FCollisionResponseParams Responses(R.Body->GetCollisionResponseToChannels());
		const auto Blocked = [&](const FTransform& A, const FTransform& B, UPrimitiveComponent* Support) -> bool
		{
			FCollisionQueryParams LocalParams = Params;
			LocalParams.AddIgnoredComponent(Support);
			for (int32 I = 0; I < Geometry.Primitives.Num(); ++I)
			{
				const auto& P = Geometry.Primitives[I];
				FCollisionShape Shape; FVector Center = P.Center; FQuat Q = A.GetRotation();
				if (P.Shape == EShape::Sphere) { Shape = FCollisionShape::MakeSphere(P.Radius + R.MarginCm); }
				else if (P.Shape == EShape::Capsule)
				{
					Shape = FCollisionShape::MakeCapsule(P.Radius + R.MarginCm, P.HalfSegment.Size() + P.Radius + R.MarginCm);
					if (!P.HalfSegment.IsNearlyZero()) { Q *= FQuat::FindBetweenNormals(FVector::UpVector, P.HalfSegment.GetSafeNormal()); }
				}
				else
				{
					FBox Bounds(ForceInit); for (const auto& V : P.Vertices) { Bounds += V; }
					Center = Bounds.GetCenter(); Shape = FCollisionShape::MakeBox(Bounds.GetExtent() + FVector(R.MarginCm));
					// A rotated authored box or convex hull may conservatively over-block; never under-cover it.
					O.ConservativeHullBounds = true;
				}
				const FVector Start = A.TransformPositionNoScale(Center), End = B.TransformPositionNoScale(Center);
				FHitResult Hit;
				if (R.World->OverlapBlockingTestByChannel(Start, Q, R.Body->GetCollisionObjectType(), Shape, LocalParams, Responses)
					|| R.World->SweepSingleByChannel(Hit, Start, End, Q, R.Body->GetCollisionObjectType(), Shape, LocalParams, Responses))
				{ O.BlockingPrimitive = I; return true; }
			}
			return false;
		};
		if (Blocked(R.From, R.To, R.EntrySupport) || Blocked(R.To, O.SourceAtPlane, R.EntrySupport))
		{ O.Result = EWorldPassageResult::SourceBlocked; return O; }
		if (Blocked(Mapped(R.From), MappedTo, R.ExitSupport)
			|| Blocked(O.DestinationAtPlane, O.DestinationClear, R.ExitSupport))
		{ O.Result = EWorldPassageResult::DestinationBlocked; return O; }
		O.Result = EWorldPassageResult::ClearAtQuery;
		return O;
	}
}
