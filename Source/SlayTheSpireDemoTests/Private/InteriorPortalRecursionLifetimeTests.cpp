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
	const uint64 ActivePackedIdentity = OldLifetime.GetPackedPublicationIdentity();
	TestTrue(TEXT("Active publication token is valid"), ActiveToken.IsValid());
	TestTrue(TEXT("Active packed publication identity is valid"), ActivePackedIdentity != 0);
	TestTrue(TEXT("Current active token may publish"), OldLifetime.CanPublish(ActiveToken));
	TestTrue(TEXT("Current packed identity may publish"), OldLifetime.CanPublishPacked(ActivePackedIdentity));

	const uint64 StableId = OldLifetime.LifetimeId;
	TestTrue(TEXT("Repeated active transition is stable"), OldLifetime.MarkActive());
	TestEqual(TEXT("Stable active lifetime preserves identity"), OldLifetime.LifetimeId, StableId);

	const FPublicationToken VisibilityRetiredPublication = OldLifetime.AdvancePublicationGeneration();
	TestTrue(TEXT("Visibility retirement captures prior publication"), VisibilityRetiredPublication == ActiveToken);
	TestEqual(TEXT("Visibility retirement preserves lifetime id"), OldLifetime.LifetimeId, StableId);
	TestEqual(TEXT("Visibility retirement preserves ACTIVE resource state"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Active));
	TestFalse(TEXT("Old visibility token can no longer publish"), OldLifetime.CanPublish(ActiveToken));
	TestFalse(TEXT("Old packed visibility identity can no longer publish"),
		OldLifetime.CanPublishPacked(ActivePackedIdentity));
	TestTrue(TEXT("Old packed visibility identity is eligible for publication retirement"),
		OldLifetime.ShouldRetirePackedPublication(ActivePackedIdentity));
	const FPublicationToken CurrentVisibilityToken = OldLifetime.CapturePublicationToken();
	const uint64 CurrentPackedIdentity = OldLifetime.GetPackedPublicationIdentity();
	TestTrue(TEXT("Current token remains publishable after visibility retirement"),
		OldLifetime.CanPublish(CurrentVisibilityToken));
	TestTrue(TEXT("Current packed identity remains publishable after visibility retirement"),
		OldLifetime.CanPublishPacked(CurrentPackedIdentity));

	FPublicationToken RetiredPublication;
	TestTrue(TEXT("Active lifetime can begin retirement"), OldLifetime.BeginRetirement(RetiredPublication));
	TestTrue(TEXT("Resource retirement captures current publication"),
		RetiredPublication == CurrentVisibilityToken);
	TestEqual(TEXT("Retiring lifetime reports RETIRING"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Retiring));
	TestFalse(TEXT("Retiring lifetime receives no new submissions"), OldLifetime.CanSubmit(0, 4));
	TestFalse(TEXT("Retiring lifetime cannot publish its stale active token"),
		OldLifetime.CanPublish(CurrentVisibilityToken));
	TestTrue(TEXT("Retiring lifetime recognizes its older publication for ordered retirement"),
		OldLifetime.ShouldRetirePublication(CurrentVisibilityToken));
	TestTrue(TEXT("Retiring lifetime recognizes its packed older publication"),
		OldLifetime.ShouldRetirePackedPublication(CurrentPackedIdentity));

	FLifetime NewLifetime;
	const uint64 NewId = IdSource.Allocate();
	TestTrue(TEXT("Runtime rebuild receives a distinct lifetime id"), NewId != OldId);
	TestTrue(TEXT("New slot accepts rebuilt lifetime"), NewLifetime.BeginAllocated(NewId));
	TestTrue(TEXT("Rebuilt lifetime can become active"), NewLifetime.MarkActive());
	const FPublicationToken NewToken = NewLifetime.CapturePublicationToken();
	const uint64 NewPackedIdentity = NewLifetime.GetPackedPublicationIdentity();
	TestTrue(TEXT("New lifetime publishes only its own token"), NewLifetime.CanPublish(NewToken));
	TestTrue(TEXT("New lifetime publishes only its own packed identity"),
		NewLifetime.CanPublishPacked(NewPackedIdentity));
	TestFalse(TEXT("Old extraction token cannot publish into rebuilt lifetime"), NewLifetime.CanPublish(ActiveToken));
	TestFalse(TEXT("Old packed extraction identity cannot publish into rebuilt lifetime"),
		NewLifetime.CanPublishPacked(ActivePackedIdentity));
	TestFalse(TEXT("Old retirement token cannot target rebuilt lifetime"),
		NewLifetime.ShouldRetirePublication(ActiveToken));
	TestFalse(TEXT("Old packed retirement identity cannot target rebuilt lifetime"),
		NewLifetime.ShouldRetirePackedPublication(ActivePackedIdentity));
	TestTrue(TEXT("Packed identity changes across rebuilt lifetime"),
		NewPackedIdentity != ActivePackedIdentity);

	TestTrue(TEXT("Retiring old lifetime can become reclaimable"), OldLifetime.MarkReclaimable());
	TestEqual(TEXT("Old lifetime reports RECLAIMABLE"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Reclaimable));
	TestTrue(TEXT("Old publication remains identifiable while reclaimable"),
		OldLifetime.ShouldRetirePublication(CurrentVisibilityToken));
	TestTrue(TEXT("Reclaimable lifetime can return to unallocated"), OldLifetime.ResetUnallocated());
	TestEqual(TEXT("Reset lifetime reports UNALLOCATED"),
		static_cast<int32>(OldLifetime.State), static_cast<int32>(EResourceState::Unallocated));
	TestEqual(TEXT("Reset lifetime clears identity"), OldLifetime.LifetimeId, uint64(0));
	TestEqual(TEXT("Reset lifetime clears publication generation"), OldLifetime.PublicationGeneration, uint64(0));
	TestEqual(TEXT("Reset lifetime clears packed identity"), OldLifetime.GetPackedPublicationIdentity(), uint64(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalCapacityReversalTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.P1A4.CapacityReversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalCapacityReversalTest::RunTest(const FString& Parameters)
{
	FLifetimeIdSource Ids;
	FLifetime Retained;
	Retained.BeginAllocated(Ids.Allocate());
	Retained.MarkActive();
	const uint64 RetainedIdentity = Retained.GetPackedPublicationIdentity();
	for (int32 Level = 1; Level < 4; ++Level)
	{
		FLifetime Old, Replacement;
		Old.BeginAllocated(Ids.Allocate());
		Old.MarkActive();
		const uint64 OldIdentity = Old.GetPackedPublicationIdentity();
		TestFalse(TEXT("Depth reduction forbids submission before retirement"), Old.CanSubmit(Level, 1));
		FPublicationToken Token;
		TestTrue(TEXT("Offscreen or visible ownership retires on capacity shrink"), Old.BeginRetirement(Token));
		TestFalse(TEXT("Delayed old extraction cannot publish"), Old.CanPublishPacked(OldIdentity));
		TestTrue(TEXT("First reversal can allocate before completion"), CanAllocateReplacement(1));
		Replacement.BeginAllocated(Ids.Allocate());
		Replacement.MarkActive();
		const uint64 NewIdentity = Replacement.GetPackedPublicationIdentity();
		TestTrue(TEXT("New lifetime may submit while old retires"), Replacement.CanSubmit(Level, 4));
		TestFalse(TEXT("Old retirement cannot clear new identity"), Old.ShouldRetirePackedPublication(NewIdentity));
		TestFalse(TEXT("Retiring predecessor cannot be reactivated"), Old.MarkActive());
		TestTrue(TEXT("Second shrink retires replacement"), Replacement.BeginRetirement(Token));
		TestFalse(TEXT("Two pending generations enforce backpressure"), CanAllocateReplacement(2));
		TestFalse(TEXT("No reuse during backpressure"), Replacement.CanSubmit(Level, 4));
		Old.MarkReclaimable();
		Old.ResetUnallocated();
		TestTrue(TEXT("Completion frees capacity without waiting for second generation"), CanAllocateReplacement(1));
		Old.BeginAllocated(Ids.Allocate());
		TestTrue(TEXT("Reallocated identity remains unique"), Old.GetPackedPublicationIdentity() != OldIdentity
			&& Old.GetPackedPublicationIdentity() != NewIdentity);
		TestFalse(TEXT("Second old callback cannot publish after retirement"), Replacement.CanPublishPacked(NewIdentity));
		Replacement.MarkReclaimable();
		Replacement.ResetUnallocated();
	}
	TestTrue(TEXT("Retained L0 can submit at reduced depth"), Retained.CanSubmit(0, 1));
	TestEqual(TEXT("Retained L0 identity unchanged"), Retained.GetPackedPublicationIdentity(), RetainedIdentity);
	return true;
}

#endif
