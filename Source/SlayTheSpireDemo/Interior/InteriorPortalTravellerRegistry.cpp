#include "InteriorPortalTravellerRegistry.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/ThreadSafeCounter64.h"

namespace InteriorPortalPhysics
{
	namespace { FThreadSafeCounter64 RegistryEpochs; }
	FTravellerRegistry::FTravellerRegistry() : Epoch(RegistryEpochs.Increment()) {}

	bool FTravellerRegistry::Register(UPrimitiveComponent* Body, EGeometryResult& OutReason)
	{
		OutReason = EGeometryResult::UnsupportedComponent;
		if (!IsValid(Body) || !Body->IsSimulatingPhysics()) { return false; }
		FRecord* Record = Records.FindByPredicate([Body](const auto& R) { return R.Body.Get() == Body; });
		if (Record && Record->Active) { return false; }
		FGeometry Geometry;
		OutReason = ExtractGeometry(Body, Geometry);
		if (OutReason != EGeometryResult::Fits) { return false; }
		if (!Record)
		{
			Records.RemoveAll([](const auto& R) { return !R.Body.IsValid(); });
			Record = &Records.AddDefaulted_GetRef();
			Record->Body = Body;
			Record->Snapshot.Handle = { Epoch, NextId++, 0 };
		}
		++Record->Snapshot.Handle.Generation;
		Record->Snapshot.Geometry = MoveTemp(Geometry);
		Record->Active = true;
		return true;
	}
	bool FTravellerRegistry::Unregister(UPrimitiveComponent* Body)
	{
		FRecord* R = Records.FindByPredicate([Body](const auto& Row) { return Row.Body == Body && Row.Active; });
		if (!R) { return false; }
		R->Active = false;
		R->Snapshot.Geometry = FGeometry();
		return true;
	}
	bool FTravellerRegistry::Refresh(UPrimitiveComponent* Body)
	{
		FRecord* R = Records.FindByPredicate([Body](const auto& Row) { return Row.Body == Body && Row.Active; });
		if (!R || !IsValid(Body) || !Body->IsSimulatingPhysics()) { return false; }
		FGeometry Geometry;
		if (ExtractGeometry(Body, Geometry) != EGeometryResult::Fits) { return false; }
		if (!(Geometry == R->Snapshot.Geometry))
		{
			++R->Snapshot.Handle.Generation;
			R->Snapshot.Geometry = MoveTemp(Geometry);
		}
		return true;
	}
	bool FTravellerRegistry::Capture(const UPrimitiveComponent* Body, FTravellerSnapshot& Out) const
	{
		Out = FTravellerSnapshot();
		const FRecord* R = Records.FindByPredicate([Body](const auto& Row) { return Row.Body == Body && Row.Active; });
		if (!R || !IsCurrent(R->Snapshot.Handle)) { return false; }
		Out = R->Snapshot;
		return true;
	}
	bool FTravellerRegistry::IsCurrent(const FTravellerHandle& Handle) const
	{
		const FRecord* R = Records.FindByPredicate([&Handle](const auto& Row) { return Row.Active && Row.Snapshot.Handle == Handle; });
		if (!R || !R->Body.IsValid() || !R->Body->IsSimulatingPhysics()) { return false; }
		FGeometry Current;
		return ExtractGeometry(R->Body.Get(), Current) == EGeometryResult::Fits && Current == R->Snapshot.Geometry;
	}
	void FTravellerRegistry::Reset() { Records.Reset(); /* Do not reuse IDs across reset. */ }
}
