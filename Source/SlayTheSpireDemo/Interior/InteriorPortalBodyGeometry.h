#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

namespace InteriorPortalPhysics
{
	enum class EShape : uint8 { Sphere, Capsule, Box, Convex };
	enum class EGeometryResult : uint8
	{
		Fits, OutsideAperture, InvalidGeometry, InvalidPose, ScaleChanged,
		UnsupportedComponent, UnsupportedCollision, UnsupportedScale, MissingConvexVertices,
		RotationSweepUnsupported, StaleIdentity, InvalidPair
	};

	/** Collision-space data with component scale baked once; never visual bounds. */
	struct SLAYTHESPIREDEMO_API FPrimitive
	{
		EShape Shape = EShape::Sphere;
		FVector Center = FVector::ZeroVector;
		double Radius = 0;
		FVector HalfSegment = FVector::ZeroVector;
		TArray<FVector> Vertices;
		bool operator==(const FPrimitive& Other) const;
	};

	struct SLAYTHESPIREDEMO_API FGeometry
	{
		FVector BakedScale = FVector::OneVector;
		TArray<FPrimitive> Primitives;
		bool operator==(const FGeometry& Other) const;
	};

	struct FFitResult
	{
		EGeometryResult Result = EGeometryResult::InvalidGeometry;
		double MinNormal = 0;
		double MaxNormal = 0;
		bool Fits() const { return Result == EGeometryResult::Fits; }
	};

	SLAYTHESPIREDEMO_API const TCHAR* Reason(EGeometryResult Result);
	/** Only simple collision on root, unwelded shape/static-mesh components. */
	SLAYTHESPIREDEMO_API EGeometryResult ExtractGeometry(UPrimitiveComponent* Body, FGeometry& Out);
	/** Convex/box vertex containment; sphere/capsule use a proven conservative ellipse bound. */
	SLAYTHESPIREDEMO_API FFitResult EvaluatePose(const FGeometry& Geometry,
		const FTransform& BodyPose, const FTransform& PortalFrame, double HalfWidth, double HalfHeight,
		double MarginCm = 0);
	/** Fixed-orientation linear translation only. Rotation explicitly fails closed. */
	SLAYTHESPIREDEMO_API FFitResult EvaluateTranslation(const FGeometry& Geometry,
		const FTransform& From, const FTransform& To, const FTransform& PortalFrame,
		double HalfWidth, double HalfHeight, double MarginCm = 0);
}
