#pragma once

#include "CoreMinimal.h"

/** Rigid portal frames: local +X faces out, Y is width, Z is height. Scale never changes momentum. */
namespace InteriorPortalMath
{
	/** Conservative normalized screen-space bounds for a portal aperture.
	 * Min/Max are in the supplied view rectangle's normalized [0,1] space.
	 * The calculation projects a rectangle enclosing the authored ellipse and
	 * clips it in homogeneous space, so the result is suitable for a future
	 * scissor/restricted viewport rather than only for visibility heuristics. */
	struct FPortalScreenBounds
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		bool bHasVisiblePortion = false;
		bool bIntersectsNearClip = false;
		bool bCameraCrossing = false;
		bool bEntirelyBehindCamera = false;
		bool bClippedToViewport = false;
	};

	/** Projects an aperture into the current view. The rectangle used for the
	 * projection encloses the portal's ellipse, making the bounds conservative.
	 * ViewProjectionMatrix must be the UE world-to-clip matrix for ViewRect. */
	inline bool ProjectPortalApertureToScreenBounds(const FTransform& ApertureFrame,
		double HalfWidth, double HalfHeight, const FMatrix& ViewProjectionMatrix,
		const FIntRect& ViewRect, FPortalScreenBounds& OutBounds,
		bool bPerspectiveProjection = true, double NearClip = 0.1)
	{
		OutBounds = FPortalScreenBounds();
		if (HalfWidth <= 0 || HalfHeight <= 0 || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
		{
			return false;
		}

		const double SafeNearClip = FMath::Max(0.001, NearClip);
		TArray<FVector4> Polygon;
		Polygon.Reserve(8);
		const FVector LocalCorners[4] =
		{
			FVector(0, -HalfWidth, -HalfHeight), FVector(0, HalfWidth, -HalfHeight),
			FVector(0, HalfWidth, HalfHeight), FVector(0, -HalfWidth, HalfHeight)
		};

		double MinDepth = TNumericLimits<double>::Max(), MaxDepth = -TNumericLimits<double>::Max();
		for (const FVector& LocalCorner : LocalCorners)
		{
			const FVector4 Clip = ViewProjectionMatrix.TransformFVector4(
				FVector4(ApertureFrame.TransformPosition(LocalCorner), 1.0));
			Polygon.Add(Clip);
			MinDepth = FMath::Min(MinDepth, double(Clip.W));
			MaxDepth = FMath::Max(MaxDepth, double(Clip.W));

			if (Clip.W > SafeNearClip)
			{
				const double InvW = 1.0 / double(Clip.W);
				const double NdcX = double(Clip.X) * InvW;
				const double NdcY = double(Clip.Y) * InvW;
				OutBounds.bClippedToViewport |= NdcX < -1.0 || NdcX > 1.0 || NdcY < -1.0 || NdcY > 1.0;
			}
		}

		OutBounds.bCameraCrossing = MinDepth <= 0.0 && MaxDepth >= 0.0;
		OutBounds.bEntirelyBehindCamera = MaxDepth < 0.0;
		OutBounds.bIntersectsNearClip = MinDepth < SafeNearClip && MaxDepth >= SafeNearClip;

		// Clip a homogeneous convex polygon against the view frustum. For a
		// perspective projection W is the forward depth in UE view space. For an
		// orthographic projection W is constant, so the near W plane is omitted.
		auto ClipPolygon = [](TArray<FVector4>& InOut, auto&& SignedDistance)
		{
			if (InOut.IsEmpty()) { return; }
			TArray<FVector4> Clipped;
			Clipped.Reserve(InOut.Num() + 4);
			for (int32 Index = 0; Index < InOut.Num(); ++Index)
			{
				const FVector4 A = InOut[Index];
				const FVector4 B = InOut[(Index + 1) % InOut.Num()];
				const double DistanceA = SignedDistance(A);
				const double DistanceB = SignedDistance(B);
				const bool bInsideA = DistanceA >= 0.0;
				const bool bInsideB = DistanceB >= 0.0;
				if (bInsideA != bInsideB)
				{
					const double Denominator = DistanceA - DistanceB;
					if (FMath::Abs(Denominator) > UE_SMALL_NUMBER)
					{
						const float Alpha = FMath::Clamp(float(DistanceA / Denominator), 0.0f, 1.0f);
						Clipped.Add(A + (B - A) * Alpha);
					}
				}
				if (bInsideB) { Clipped.Add(B); }
			}
			InOut = MoveTemp(Clipped);
		};

		if (bPerspectiveProjection)
		{
			ClipPolygon(Polygon, [SafeNearClip](const FVector4& P) { return double(P.W) - SafeNearClip; });
		}
		else
		{
			ClipPolygon(Polygon, [](const FVector4& P) { return double(P.W); });
		}
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.X + P.W); });
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.W - P.X); });
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.Y + P.W); });
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.W - P.Y); });
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.Z); });
		ClipPolygon(Polygon, [](const FVector4& P) { return double(P.W - P.Z); });

		if (Polygon.IsEmpty()) { return false; }
		FVector2D Min(1.0f, 1.0f);
		FVector2D Max(0.0f, 0.0f);
		for (const FVector4& Clip : Polygon)
		{
			if (Clip.W <= UE_SMALL_NUMBER) { continue; }
			const double InvW = 1.0 / double(Clip.W);
			const FVector2D Normalized(
				float(double(Clip.X) * InvW * 0.5 + 0.5),
				float(0.5 - double(Clip.Y) * InvW * 0.5));
			Min.X = FMath::Min(Min.X, Normalized.X);
			Min.Y = FMath::Min(Min.Y, Normalized.Y);
			Max.X = FMath::Max(Max.X, Normalized.X);
			Max.Y = FMath::Max(Max.Y, Normalized.Y);
		}
		OutBounds.Min = Min;
		OutBounds.Max = Max;
		OutBounds.bHasVisiblePortion = Max.X >= Min.X && Max.Y >= Min.Y;
		OutBounds.bClippedToViewport |= OutBounds.bIntersectsNearClip;
		return OutBounds.bHasVisiblePortion;
	}

	/** A discontinuous virtual-camera jump must not reuse temporal history. */
	inline bool IsVirtualViewDiscontinuous(const FTransform& Previous, const FTransform& Current,
		double MaxTranslation = 100.0, double MaxAngleDegrees = 45.0)
	{
		return FVector::DistSquared(Previous.GetLocation(), Current.GetLocation()) > FMath::Square(MaxTranslation)
			|| Previous.GetRotation().AngularDistance(Current.GetRotation()) > FMath::DegreesToRadians(MaxAngleDegrees);
	}

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

	/** Continuous fit of an upright capsule projected into the logical aperture.
	 * The 32-sided polygon is INSIDE the ellipse. Capsule support functions are analytic;
	 * thus this is conservative geometry, not a set of unproven capsule surface samples.
	 * Returns the segment interval whose complete capsule fits, and the inward exit normal.
	 * For an already active passage, bAllowInitialRecovery allows motion parallel to or away
	 * from an initially violated edge. It never permits increasing that edge's penetration.
	 * Placement, acquisition and transfer validation must use the strict default. */
	inline bool CapsuleApertureInterval(const FVector& Center, const FVector& Delta, const FVector& Spine,
		double Radius, double Width, double Height, double& Enter, double& Leave, FVector& InwardNormal,
		bool bAllowInitialRecovery = false)
	{
		Enter = 0; Leave = 1; InwardNormal = FVector::ZeroVector;
		if (Width <= 0 || Height <= 0 || Radius < 0) { return false; }
		constexpr int32 Sides = 32;
		const double Edge = FMath::Cos(UE_PI / Sides);
		for (int32 I=0; I<Sides; ++I)
		{
			const double Angle = (I+.5)*2*UE_PI/Sides;
			const FVector N(0,FMath::Cos(Angle)/Width,FMath::Sin(Angle)/Height);
			const double Room = Edge - FMath::Abs(FVector::DotProduct(N,Spine)) - Radius*N.Size();
			const double RawGap = Room - FVector::DotProduct(N,Center);
			const double Gap = bAllowInitialRecovery ? FMath::Max(0.0,RawGap) : RawGap;
			const double Speed = FVector::DotProduct(N,Delta);
			if (FMath::Abs(Speed) < 1.e-10) { if (Gap < -1.e-8) { return false; } continue; }
			const double T = Gap/Speed;
			if (Speed > 0 && T < Leave) { Leave=T; InwardNormal=-N.GetSafeNormal(); }
			else if (Speed < 0) { Enter=FMath::Max(Enter,T); }
			if (Enter > Leave) { return false; }
		}
		return Leave >= 0 && Enter <= 1;
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
