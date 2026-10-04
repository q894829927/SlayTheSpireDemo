#pragma once

#include "InteriorPortalBodyGeometry.h"

namespace InteriorPortalPhysics
{
	/** IDs are scoped to one registry epoch; array indices and UObject addresses are not IDs. */
	struct FTravellerHandle
	{
		uint64 Epoch = 0;
		uint64 Id = 0;
		uint64 Generation = 0;
		bool operator==(const FTravellerHandle& Other) const
		{ return Epoch == Other.Epoch && Id == Other.Id && Generation == Other.Generation; }
	};
	struct FTravellerSnapshot
	{
		FTravellerHandle Handle;
		FGeometry Geometry;
	};
	/** Game-thread identity/configuration only; no solver-thread UObject access or pose authority. */
	class SLAYTHESPIREDEMO_API FTravellerRegistry
	{
	public:
		FTravellerRegistry();
		FTravellerRegistry(const FTravellerRegistry&) = delete;
		FTravellerRegistry& operator=(const FTravellerRegistry&) = delete;
		bool Register(UPrimitiveComponent* Body, EGeometryResult& OutReason);
		bool Unregister(UPrimitiveComponent* Body);
		bool Refresh(UPrimitiveComponent* Body);
		bool Capture(const UPrimitiveComponent* Body, FTravellerSnapshot& Out) const;
		bool IsCurrent(const FTravellerHandle& Handle) const;
		void Reset();
	private:
		struct FRecord
		{
			TWeakObjectPtr<UPrimitiveComponent> Body;
			FTravellerSnapshot Snapshot;
			bool Active = false;
		};
		TArray<FRecord> Records;
		uint64 Epoch;
		uint64 NextId = 1;
	};
}
