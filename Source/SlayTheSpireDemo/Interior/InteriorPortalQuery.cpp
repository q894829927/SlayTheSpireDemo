#include "InteriorPortalQuery.h"

#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalSystem.h"
#include "Engine/World.h"

namespace
{
	struct FPortalCrossing
	{
		const AInteriorPortal* Portal = nullptr;
		float Time = 0.0f;
		FVector LocalPoint = FVector::ZeroVector;
	};

	bool FindCrossing(const AInteriorPortalSystem* System, const FVector& Start, const FVector& End,
		float ApertureRadius, FPortalCrossing& OutCrossing)
	{
		if (!System || !System->IsLinked()) { return false; }
		const FVector Delta = End - Start;
		if (Delta.SizeSquared() <= UE_SMALL_NUMBER) { return false; }

		constexpr double PlaneEpsilon = 1.e-4;
		bool bFound = false;
		for (const AInteriorPortal* Portal : {System->BluePortal.Get(), System->OrangePortal.Get()})
		{
			if (!IsValid(Portal) || !Portal->bPlaced) { continue; }
			const FTransform Frame = Portal->GetLogicalFrame();
			const FVector A = Frame.InverseTransformPositionNoScale(Start);
			const FVector B = Frame.InverseTransformPositionNoScale(End);
			const double PlaneDelta = B.X - A.X;
			if (FMath::Abs(PlaneDelta) <= PlaneEpsilon) { continue; }

			const bool bFrontToBack = A.X > PlaneEpsilon && B.X < -PlaneEpsilon;
			const bool bBackToFront = A.X < -PlaneEpsilon && B.X > PlaneEpsilon;
			if (!bFrontToBack && !bBackToFront) { continue; }

			const double Time = FMath::Clamp(-A.X / PlaneDelta, 0.0, 1.0);
			const FVector LocalPoint = FMath::Lerp(A, B, Time);
			if (!InteriorPortalMath::Inside(LocalPoint, Portal->HalfWidth, Portal->HalfHeight,
				ApertureRadius, ApertureRadius)) { continue; }
			if (!bFound || Time < OutCrossing.Time)
			{
				bFound = true;
				OutCrossing.Portal = Portal;
				OutCrossing.Time = float(Time);
				OutCrossing.LocalPoint = LocalPoint;
			}
		}
		return bFound;
	}

	float SupportNormalExtent(const AInteriorPortal* Portal, const FVector& Normal)
	{
		if (!Portal || !IsValid(Portal->Support)) { return 0.0f; }
		const FVector Extent = Portal->Support->Bounds.BoxExtent;
		const FVector AbsNormal(FMath::Abs(Normal.X), FMath::Abs(Normal.Y), FMath::Abs(Normal.Z));
		return FVector::DotProduct(Extent, AbsNormal);
	}

	template <typename TraceFn>
	bool TraceRecursive(const AInteriorPortalSystem* System, const FVector& Start, const FVector& End,
		float ApertureRadius, ECollisionChannel Channel, const FCollisionQueryParams& Params,
		FHitResult& OutHit, int32 Depth, int32 MaxPortalHops, float PortalBias, TraceFn&& TraceFnImpl)
	{
		if (!System || !System->GetWorld()) { OutHit = FHitResult(); return false; }
		FHitResult WorldHit;
		const bool bWorldHit = TraceFnImpl(System->GetWorld(), Start, End, Channel, Params, WorldHit);
		if (Depth >= MaxPortalHops)
		{
			OutHit = WorldHit;
			return bWorldHit;
		}

		FPortalCrossing Crossing;
		if (!FindCrossing(System, Start, End, ApertureRadius, Crossing))
		{
			OutHit = WorldHit;
			return bWorldHit;
		}

		const float SegmentLength = (End - Start).Size();
		const FTransform SourceFrame = Crossing.Portal->GetLogicalFrame();
		const float SupportTolerance = FMath::Max(.002f,
			(SupportNormalExtent(Crossing.Portal, SourceFrame.GetUnitAxis(EAxis::X))
				+ ApertureRadius + PortalBias + 1.0f) / FMath::Max(SegmentLength, 1.0f));
		// An unrelated object before the analytic portal event still wins. A hit on
		// the endpoint's support is the expected wall representation of the aperture
		// and is replaced only within a tolerance derived from support thickness and
		// the query shape.
		const bool bSupportHit = bWorldHit && WorldHit.GetComponent() == Crossing.Portal->Support;
		if (bWorldHit && ((bSupportHit && WorldHit.Time < Crossing.Time - SupportTolerance)
			|| (!bSupportHit && WorldHit.Time < Crossing.Time - .002f)))
		{
			OutHit = WorldHit;
			return true;
		}

		const AInteriorPortal* Exit = Crossing.Portal == System->BluePortal.Get()
			? System->OrangePortal.Get() : System->BluePortal.Get();
		if (!IsValid(Exit))
		{
			OutHit = WorldHit;
			return bWorldHit;
		}

		const FVector Segment = End - Start;
		const float Length = Segment.Size();
		if (Length <= UE_SMALL_NUMBER)
		{
			OutHit = WorldHit;
			return bWorldHit;
		}
		const FVector Direction = Segment / Length;
		const FTransform From = Crossing.Portal->GetLogicalFrame();
		const FTransform To = Exit->GetLogicalFrame();
		const FQuat Mapping = InteriorPortalMath::Rotation(From, To);
		const FVector MappedDirection = Mapping.RotateVector(Direction).GetSafeNormal();
		const FVector MappedPlanePoint = InteriorPortalMath::Position(
			FMath::Lerp(Start, End, Crossing.Time), From, To);
		const float ExitThickness = SupportNormalExtent(Exit, To.GetUnitAxis(EAxis::X));
		const float Advance = FMath::Max(0.25f, FMath::Max(PortalBias, ExitThickness + ApertureRadius + 1.0f));
		const FVector MappedStart = MappedPlanePoint + MappedDirection * Advance;
		const FVector MappedEnd = InteriorPortalMath::Position(End, From, To);
		if ((MappedEnd - MappedStart).SizeSquared() <= FMath::Square(.01f))
		{
			OutHit = FHitResult();
			return false;
		}

		FHitResult MappedHit;
		if (!TraceRecursive(System, MappedStart, MappedEnd, ApertureRadius, Channel, Params,
			MappedHit, Depth + 1, MaxPortalHops, PortalBias, Forward<TraceFn>(TraceFnImpl)))
		{
			OutHit = FHitResult();
			return false;
		}
		OutHit = MappedHit;
		OutHit.Time = Crossing.Time + (1.0f - Crossing.Time) * MappedHit.Time;
		OutHit.Distance = Length * OutHit.Time;
		return true;
	}
}

namespace InteriorPortalQuery
{
	bool LineTrace(const AInteriorPortalSystem* System, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, const FCollisionQueryParams& Params, FHitResult& OutHit,
		int32 MaxPortalHops, float PortalBias)
	{
		const auto Trace = [](UWorld* World, const FVector& QueryStart, const FVector& QueryEnd,
			ECollisionChannel QueryChannel, const FCollisionQueryParams& QueryParams, FHitResult& Hit)
		{
			return World->LineTraceSingleByChannel(Hit, QueryStart, QueryEnd, QueryChannel, QueryParams);
		};
		return TraceRecursive(System, Start, End, 0.0f, Channel, Params, OutHit, 0,
			FMath::Max(0, MaxPortalHops), PortalBias, Trace);
	}

	bool SphereSweep(const AInteriorPortalSystem* System, const FVector& Start, const FVector& End,
		float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params, FHitResult& OutHit,
		int32 MaxPortalHops, float PortalBias)
	{
		const float SafeRadius = FMath::Max(0.0f, Radius);
		const auto Trace = [SafeRadius](UWorld* World, const FVector& QueryStart, const FVector& QueryEnd,
			ECollisionChannel QueryChannel, const FCollisionQueryParams& QueryParams, FHitResult& Hit)
		{
			return World->SweepSingleByChannel(Hit, QueryStart, QueryEnd, FQuat::Identity,
				QueryChannel, FCollisionShape::MakeSphere(SafeRadius), QueryParams);
		};
		return TraceRecursive(System, Start, End, SafeRadius, Channel, Params, OutHit, 0,
			FMath::Max(0, MaxPortalHops), PortalBias, Trace);
	}
}
