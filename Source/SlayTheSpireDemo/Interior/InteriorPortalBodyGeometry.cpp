#include "InteriorPortalBodyGeometry.h"
#include "Components/ShapeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"

namespace InteriorPortalPhysics
{
	bool FPrimitive::operator==(const FPrimitive& Other) const
	{
		return Shape == Other.Shape && Center == Other.Center && Radius == Other.Radius
			&& HalfSegment == Other.HalfSegment && Vertices == Other.Vertices;
	}
	bool FGeometry::operator==(const FGeometry& Other) const
	{
		return BakedScale == Other.BakedScale && Primitives == Other.Primitives;
	}
	const TCHAR* Reason(EGeometryResult Result)
	{
		switch (Result)
		{
		case EGeometryResult::Fits: return TEXT("FITS");
		case EGeometryResult::OutsideAperture: return TEXT("OUTSIDE_APERTURE");
		case EGeometryResult::InvalidPose: return TEXT("INVALID_POSE");
		case EGeometryResult::ScaleChanged: return TEXT("SCALE_CHANGED");
		case EGeometryResult::UnsupportedComponent: return TEXT("UNSUPPORTED_COMPONENT");
		case EGeometryResult::UnsupportedCollision: return TEXT("UNSUPPORTED_COLLISION");
		case EGeometryResult::UnsupportedScale: return TEXT("UNSUPPORTED_SCALE");
		case EGeometryResult::MissingConvexVertices: return TEXT("MISSING_CONVEX_VERTICES");
		case EGeometryResult::RotationSweepUnsupported: return TEXT("ROTATION_SWEEP_UNSUPPORTED");
		case EGeometryResult::StaleIdentity: return TEXT("STALE_IDENTITY");
		case EGeometryResult::InvalidPair: return TEXT("INVALID_PAIR");
		default: return TEXT("INVALID_GEOMETRY");
		}
	}

	EGeometryResult ExtractGeometry(UPrimitiveComponent* Body, FGeometry& Out)
	{
		Out = FGeometry();
		if (!IsValid(Body) || !IsValid(Body->GetOwner()) || Body->GetOwner()->GetRootComponent() != Body
			|| (!Body->IsA<UShapeComponent>() && !Body->IsA<UStaticMeshComponent>())
			|| (Body->GetBodyInstance() && Body->GetBodyInstance()->WeldParent))
		{ return EGeometryResult::UnsupportedComponent; }
		const FVector Scale = Body->GetComponentScale();
		if (Scale.ContainsNaN() || Scale.GetMin() <= UE_SMALL_NUMBER)
		{ return EGeometryResult::UnsupportedScale; }
		UBodySetup* Setup = Body->GetBodySetup();
		if (!Setup || !CollisionEnabledHasPhysics(Body->GetCollisionEnabled())
			|| Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
		{ return EGeometryResult::UnsupportedCollision; }
		const FKAggregateGeom& Agg = Setup->AggGeom;
		if (Agg.GetElementCount() != Agg.SphereElems.Num() + Agg.SphylElems.Num()
			+ Agg.BoxElems.Num() + Agg.ConvexElems.Num())
		{ return EGeometryResult::UnsupportedCollision; }
		FGeometry Geometry;
		Geometry.BakedScale = Scale;
		// Match the Engine helpers used by ChaosInterfaceUtils for scaled primitives.
		for (const auto& Elem : Agg.SphereElems)
		{
			if (!CollisionEnabledHasPhysics(Elem.GetCollisionEnabled())) { continue; }
			const auto Scaled = Elem.GetFinalScaled(Scale, FTransform::Identity);
			FPrimitive P; P.Shape = EShape::Sphere; P.Center = Scaled.Center; P.Radius = Scaled.Radius;
			Geometry.Primitives.Add(MoveTemp(P));
		}
		for (const auto& Elem : Agg.SphylElems)
		{
			if (!CollisionEnabledHasPhysics(Elem.GetCollisionEnabled())) { continue; }
			const auto Scaled = Elem.GetFinalScaled(Scale, FTransform::Identity);
			FPrimitive P; P.Shape = EShape::Capsule; P.Center = Scaled.Center; P.Radius = Scaled.Radius;
			P.HalfSegment = Scaled.Rotation.RotateVector(FVector(0, 0, Scaled.Length * .5));
			Geometry.Primitives.Add(MoveTemp(P));
		}
		for (const auto& Elem : Agg.BoxElems)
		{
			if (!CollisionEnabledHasPhysics(Elem.GetCollisionEnabled())) { continue; }
			const auto Scaled = Elem.GetFinalScaled(Scale, FTransform::Identity);
			FPrimitive P; P.Shape = EShape::Box;
			for (int32 X : {-1, 1}) for (int32 Y : {-1, 1}) for (int32 Z : {-1, 1})
			{
				P.Vertices.Add(Scaled.GetTransform().TransformPositionNoScale(
					FVector(X * Scaled.X * .5, Y * Scaled.Y * .5, Z * Scaled.Z * .5)));
			}
			Geometry.Primitives.Add(MoveTemp(P));
		}
		for (const auto& Elem : Agg.ConvexElems)
		{
			if (!CollisionEnabledHasPhysics(Elem.GetCollisionEnabled())) { continue; }
			if (Elem.VertexData.Num() < 4) { return EGeometryResult::MissingConvexVertices; }
			if (!Elem.GetTransform().IsValid()) { return EGeometryResult::InvalidGeometry; }
			FPrimitive P; P.Shape = EShape::Convex;
			// BodySetup cooking bakes the element transform into body-space vertices;
			// Chaos then applies component scale. Authored hull containment is conservative.
			for (const FVector& Vertex : Elem.VertexData)
			{ P.Vertices.Add(Elem.GetTransform().TransformPosition(Vertex) * Scale); }
			Geometry.Primitives.Add(MoveTemp(P));
		}
		if (Geometry.Primitives.IsEmpty()) { return EGeometryResult::UnsupportedCollision; }
		if (!EvaluatePose(Geometry, FTransform(FQuat::Identity, FVector::ZeroVector, Scale),
			FTransform::Identity, 1.e100, 1.e100).Fits()) { return EGeometryResult::InvalidGeometry; }
		Out = MoveTemp(Geometry);
		return EGeometryResult::Fits;
	}

	FFitResult EvaluatePose(const FGeometry& Geometry, const FTransform& Pose,
		const FTransform& Frame, double Width, double Height, double Margin)
	{
		FFitResult Result;
		if (Geometry.Primitives.IsEmpty() || Geometry.BakedScale.ContainsNaN()
			|| Geometry.BakedScale.GetMin() <= UE_SMALL_NUMBER
			|| !FMath::IsFinite(Width) || !FMath::IsFinite(Height)
			|| !FMath::IsFinite(Margin) || Width <= 0 || Height <= 0 || Margin < 0)
		{ return Result; }
		if (Pose.ContainsNaN() || Frame.ContainsNaN() || !Pose.GetRotation().IsNormalized()
			|| !Frame.GetRotation().IsNormalized() || !Frame.GetScale3D().Equals(FVector::OneVector, 1.e-6))
		{ Result.Result = EGeometryResult::InvalidPose; return Result; }
		if (!Pose.GetScale3D().Equals(Geometry.BakedScale, 1.e-6))
		{ Result.Result = EGeometryResult::ScaleChanged; return Result; }
		Result.Result = EGeometryResult::Fits;
		Result.MinNormal = TNumericLimits<double>::Max();
		Result.MaxNormal = -TNumericLimits<double>::Max();
		const auto Check = [&](const FVector& LocalPoint, double Radius)
		{
			if (LocalPoint.ContainsNaN() || !FMath::IsFinite(Radius) || Radius < 0)
			{ Result.Result = EGeometryResult::InvalidGeometry; return; }
			const FVector P = Frame.InverseTransformPositionNoScale(Pose.TransformPositionNoScale(LocalPoint));
			if (P.ContainsNaN()) { Result.Result = EGeometryResult::InvalidPose; return; }
			Result.MinNormal = FMath::Min(Result.MinNormal, P.X - Radius);
			Result.MaxNormal = FMath::Max(Result.MaxNormal, P.X + Radius);
			// Triangle inequality in ellipse-normalized coordinates bounds an entire
			// projected ball. Subtracting radius from both ellipse axes is NOT a proof.
			const double Bound = FMath::Sqrt(FMath::Square(P.Y / Width) + FMath::Square(P.Z / Height))
				+ (Radius + Margin) / FMath::Min(Width, Height);
			if (!FMath::IsFinite(Bound)) { Result.Result = EGeometryResult::InvalidGeometry; return; }
			if (Bound > 1 && Result.Result == EGeometryResult::Fits)
			{ Result.Result = EGeometryResult::OutsideAperture; }
		};
		for (const FPrimitive& P : Geometry.Primitives)
		{
			if (P.Shape == EShape::Sphere) { Check(P.Center, P.Radius); }
			else if (P.Shape == EShape::Capsule)
			{ Check(P.Center - P.HalfSegment, P.Radius); Check(P.Center + P.HalfSegment, P.Radius); }
			else if (P.Shape == EShape::Box || P.Shape == EShape::Convex)
			{
				if ((P.Shape == EShape::Box && P.Vertices.Num() != 8)
					|| (P.Shape == EShape::Convex && P.Vertices.Num() < 4))
				{ Result.Result = EGeometryResult::InvalidGeometry; return Result; }
				for (const FVector& V : P.Vertices) { Check(V, 0); }
			}
			else { Result.Result = EGeometryResult::InvalidGeometry; return Result; }
		}
		return Result;
	}

	FFitResult EvaluateTranslation(const FGeometry& Geometry, const FTransform& From,
		const FTransform& To, const FTransform& Frame, double Width, double Height, double Margin)
	{
		FFitResult A = EvaluatePose(Geometry, From, Frame, Width, Height, Margin);
		FFitResult B = EvaluatePose(Geometry, To, Frame, Width, Height, Margin);
		if (!A.Fits()) { return A; }
		if (!B.Fits()) { return B; }
		if (!From.GetRotation().Equals(To.GetRotation(), 1.e-8))
		{ A.Result = EGeometryResult::RotationSweepUnsupported; return A; }
		// With fixed orientation, each point/ball follows a straight segment in a
		// convex aperture. Legal endpoint projections bound the whole translation.
		A.MinNormal = FMath::Min(A.MinNormal, B.MinNormal);
		A.MaxNormal = FMath::Max(A.MaxNormal, B.MaxNormal);
		return A;
	}
}
