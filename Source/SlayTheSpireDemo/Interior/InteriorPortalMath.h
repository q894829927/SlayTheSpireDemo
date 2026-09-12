#pragma once

#include "CoreMinimal.h"

/** Rigid portal frames: local +X faces out, Y is width, Z is height. Scale never changes momentum. */
namespace InteriorPortalMath
{
	inline FQuat Rotation(const FTransform& Entry, const FTransform& Exit)
	{
		return (Exit.GetRotation() * FQuat(FVector::UpVector, PI) * Entry.GetRotation().Inverse()).GetNormalized();
	}
	inline FVector Position(const FVector& Point, const FTransform& Entry, const FTransform& Exit)
	{
		return Exit.GetLocation() + Rotation(Entry, Exit).RotateVector(Point - Entry.GetLocation());
	}
	inline bool Inside(const FVector& Local, double Width, double Height, double MarginY = 0, double MarginZ = 0)
	{
		// Conservative inset ellipse; reject rather than let a capsule scrape through the rim.
		Width -= MarginY;
		Height -= MarginZ;
		return Width > 0 && Height > 0 && FMath::Square(Local.Y / Width) + FMath::Square(Local.Z / Height) <= 1.0;
	}
	inline bool Crossed(const FVector& Previous, const FVector& Current, const FTransform& Frame,
		double Width, double Height, FVector& Intersection)
	{
		const FVector A = Frame.InverseTransformPositionNoScale(Previous);
		const FVector B = Frame.InverseTransformPositionNoScale(Current);
		if (A.X <= 0 || B.X > 0 || A.X - B.X <= UE_SMALL_NUMBER) { return false; }
		const double Alpha = A.X / (A.X - B.X);
		const FVector Local = FMath::Lerp(A, B, Alpha);
		Intersection = FMath::Lerp(Previous, Current, Alpha);
		return Inside(Local, Width, Height);
	}

	/**
	 * Builds a reversed-Z oblique near projection using UE row-vector matrices.
	 * Returns false for invalid/ill-conditioned planes so production code can deliberately skip the capture
	 * instead of silently rendering an unclipped portal frame.
	 */
	inline bool TryObliqueProjection(const FMatrix& Projection, const FVector4& ViewPlane, FMatrix& OutProjection,
		double MinPositiveDenominator = 1.e-4)
	{
		const double NormalLength = FMath::Sqrt(
			ViewPlane.X * ViewPlane.X + ViewPlane.Y * ViewPlane.Y + ViewPlane.Z * ViewPlane.Z);
		if (!FMath::IsFinite(NormalLength) || NormalLength <= UE_SMALL_NUMBER) { return false; }

		const FVector4 Plane = ViewPlane / NormalLength;
		const FVector4 Corner(Plane.X >= 0 ? 1 : -1, Plane.Y >= 0 ? 1 : -1, 0, 1);
		const FVector4 Q = Projection.Inverse().TransformFVector4(Corner);
		const double Denominator = Plane.X * Q.X + Plane.Y * Q.Y + Plane.Z * Q.Z + Plane.W * Q.W;
		if (!FMath::IsFinite(Denominator) || Denominator <= MinPositiveDenominator) { return false; }

		const FVector4 C = Plane / Denominator;
		FMatrix Result = Projection;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			Result.M[Row][2] = Projection.M[Row][3] - C[Row];
			if (!FMath::IsFinite(Result.M[Row][2])) { return false; }
		}
		OutProjection = Result;
		return true;
	}

	/** Compatibility helper for tests/callers that do not need explicit failure handling. */
	inline FMatrix ObliqueProjection(const FMatrix& Projection, const FVector4& ViewPlane)
	{
		FMatrix Result;
		return TryObliqueProjection(Projection, ViewPlane, Result) ? Result : Projection;
	}
}
