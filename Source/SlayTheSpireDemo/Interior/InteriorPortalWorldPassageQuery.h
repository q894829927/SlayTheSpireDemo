#pragma once

#include "InteriorPortalTravellerRegistry.h"

class UWorld;

namespace InteriorPortalPhysics
{
	enum class EWorldPassageResult : uint8
	{
		ClearAtQuery, InvalidRequest, StaleTraveller, UnsupportedSupport,
		UnsupportedMotion, OutsideAperture, SourceBlocked, DestinationBlocked
	};
	/** GT observation of fixed-orientation paths. Never a physics-substep permission. */
	struct FWorldPassageRequest
	{
		UWorld* World = nullptr;
		UPrimitiveComponent* Body = nullptr;
		UPrimitiveComponent* EntrySupport = nullptr;
		UPrimitiveComponent* ExitSupport = nullptr;
		FTravellerSnapshot Traveller;
		uint64 PairGeneration = 0, Revision = 0;
		FTransform Entry, Exit, From, To;
		FVector LocalCOM = FVector::ZeroVector;
		double HalfWidth = 65, HalfHeight = 115, ExitHalfWidth = 65, ExitHalfHeight = 115;
		double MarginCm = .5, MaxIntervalTranslationCm = 2.01;
	};
	struct FWorldPassageObservation
	{
		EWorldPassageResult Result = EWorldPassageResult::InvalidRequest;
		FTravellerHandle Body;
		uint64 PairGeneration = 0, Revision = 0;
		double EntryOutwardDepthCm = 0, ExitOutwardDepthCm = 0;
		FTransform SourceAtPlane, DestinationAtPlane, DestinationClear;
		int32 BlockingPrimitive = INDEX_NONE;
		bool ConservativeHullBounds = false;
	};
	/** Full compound shape, actual collision responses, initial overlap and swept clearance.
	 * Only the selected support is excluded after full aperture containment succeeds.
	 * Query-disabled physics obstacles and future motion are outside this observation's scope. */
	SLAYTHESPIREDEMO_API FWorldPassageObservation ObserveWorldPassage(
		const FWorldPassageRequest& Request, const FTravellerRegistry& Registry);
}
