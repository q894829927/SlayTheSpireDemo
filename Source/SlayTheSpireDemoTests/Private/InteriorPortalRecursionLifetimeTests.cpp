#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorPortalRecursionLifetime.h"

using namespace InteriorPortalRecursionLifetime;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalRecursionLifetimeModelTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.P1A2.LifetimeModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalRecursionLifetimeModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FLifetimeIdSource IdSource;
	FLifetime OldLifetime;

	const uint64 OldId = IdSource.Allocate();
	TestTrue(TEXT("First lifetime id is valid"), OldId != 0);
	TestTrue(TEXT("Unallocated slot accepts a new lifetime"), OldLifetime.BeginAllocated(OldId));
	TestEqual(TEXT("New lifetime begins allocated"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Allocated));
	TestTrue(TEXT("Allocated lifetime may submit below RequestedDepth"), OldLifetime.CanSubmit(2, 4));
	TestFalse(TEXT("Allocated lifetime may not submit at or above RequestedDepth"), OldLifetime.CanSubmit(4, 4));
	TestTrue(TEXT("Allocated lifetime can become active"), OldLifetime.MarkActive());

	const FPublicationToken ActiveToken = OldLifetime.CapturePublicationToken();
	TestTrue(TEXT("Active publication token is valid"), ActiveToken.IsValid());
	TestTrue(TEXT("Current active token may publish"), OldLifetime.CanPublish(ActiveToken));

	const uint64 StableId = OldLifetime.LifetimeId;
	TestTrue(TEXT("Repeated active transition is stable"), OldLifetime.MarkActive());
	TestEqual(TEXT("Stable active lifetime preserves identity"), OldLifetime.LifetimeId, StableId);

	FPublicationToken RetiredPublication;
	TestTrue(TEXT("Active lifetime can begin retirement"), OldLifetime.BeginRetirement(RetiredPublication));
	TestTrue(TEXT("Retirement captures the previously active publication"), RetiredPublication == ActiveToken);
	TestEqual(TEXT("Retiring lifetime reports RETIRING"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Retiring));
	TestFalse(TEXT("Retiring lifetime receives no new submissions"), OldLifetime.CanSubmit(0, 4));
	TestFalse(TEXT("Retiring lifetime cannot publish its stale active token"), OldLifetime.CanPublish(ActiveToken));
	TestTrue(TEXT("Retiring lifetime recognizes its older publication for ordered retirement"),
		OldLifetime.ShouldRetirePublication(ActiveToken));

	FLifetime NewLifetime;
	const uint64 NewId = IdSource.Allocate();
	TestTrue(TEXT("Runtime rebuild receives a distinct lifetime id"), NewId != OldId);
	TestTrue(TEXT("New slot accepts rebuilt lifetime"), NewLifetime.BeginAllocated(NewId));
	TestTrue(TEXT("Rebuilt lifetime can become active"), NewLifetime.MarkActive());
	const FPublicationToken NewToken = NewLifetime.CapturePublicationToken();
	TestTrue(TEXT("New lifetime publishes only its own token"), NewLifetime.CanPublish(NewToken));
	TestFalse(TEXT("Old extraction token cannot publish into rebuilt lifetime"), NewLifetime.CanPublish(ActiveToken));
	TestFalse(TEXT("Old retirement token cannot target rebuilt lifetime"),
		NewLifetime.ShouldRetirePublication(ActiveToken));

	TestTrue(TEXT("Retiring old lifetime can become reclaimable"), OldLifetime.MarkReclaimable());
	TestEqual(TEXT("Old lifetime reports RECLAIMABLE"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Reclaimable));
	TestTrue(TEXT("Old publication remains identifiable while reclaimable"),
		OldLifetime.ShouldRetirePublication(ActiveToken));
	TestTrue(TEXT("Reclaimable lifetime can return to unallocated"), OldLifetime.ResetUnallocated());
	TestEqual(TEXT("Reset lifetime reports UNALLOCATED"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Unallocated));
	TestEqual(TEXT("Reset lifetime clears identity"), OldLifetime.LifetimeId, uint64(0));
	TestEqual(TEXT("Reset lifetime clears publication generation"), OldLifetime.PublicationGeneration, uint64(0));

	return true;
}

#endif
