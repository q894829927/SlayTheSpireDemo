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
	/** Reversed-Z oblique near plane (positive half-space retained), using UE row-vector matrices.
	 * This works without enabling the project-wide global clip-plane shader permutation. */
	inline FMatrix ObliqueProjection(const FMatrix& Projection, const FVector4& ViewPlane)
	{
		const FVector4 Corner(ViewPlane.X >= 0 ? 1 : -1, ViewPlane.Y >= 0 ? 1 : -1, 0, 1);
		const FVector4 Q = Projection.Inverse().TransformFVector4(Corner);
		const double Denominator = ViewPlane.X * Q.X + ViewPlane.Y * Q.Y + ViewPlane.Z * Q.Z + ViewPlane.W * Q.W;
		if (Denominator <= UE_SMALL_NUMBER) { return Projection; }
		const FVector4 C = ViewPlane / Denominator;
		FMatrix Result = Projection;
		for (int32 Row = 0; Row < 4; ++Row) { Result.M[Row][2] = Projection.M[Row][3] - C[Row]; }
		return Result;
	}
}
