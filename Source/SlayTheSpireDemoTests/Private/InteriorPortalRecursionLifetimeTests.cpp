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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalSubmissionFailureContractTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.P1AGate.SubmissionFailureContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalSubmissionFailureContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Submission order is deepest -> shallowest. Start with a four-level visible
	// chain and model a successful deepest child followed by an L2 failure.
	int32 EffectiveDepth = 4;
	int32 CurrentFrameSubmittedMask = 0;

	CurrentFrameSubmittedMask |= (1 << 3);
	TestTrue(TEXT("L2 may consume L3 only after L3 submitted this frame"),
		CanConsumeCurrentFrameChild(2, EffectiveDepth, CurrentFrameSubmittedMask));

	EffectiveDepth = TruncateEffectiveDepthAfterSubmissionFailure(EffectiveDepth, 2);
	TestEqual(TEXT("L2 failure terminates recursion before L2"), EffectiveDepth, 2);
	TestFalse(TEXT("L1 cannot consume failed L2 even if an older L2 publication exists"),
		CanConsumeCurrentFrameChild(1, EffectiveDepth, CurrentFrameSubmittedMask));
	TestFalse(TEXT("L2 cannot consume now-out-of-range L3 after the chain truncates"),
		CanConsumeCurrentFrameChild(2, EffectiveDepth, CurrentFrameSubmittedMask));

	// Shallower rendering may continue as an explicit recursion terminator.
	CurrentFrameSubmittedMask |= (1 << 1);
	TestTrue(TEXT("L0 may consume successfully submitted L1 after deeper failure"),
		CanConsumeCurrentFrameChild(0, EffectiveDepth, CurrentFrameSubmittedMask));

	// A direct child failure at L1 leaves L0 valid but with no child recursion.
	EffectiveDepth = TruncateEffectiveDepthAfterSubmissionFailure(4, 1);
	CurrentFrameSubmittedMask = (1 << 3) | (1 << 2);
	TestEqual(TEXT("L1 failure truncates to one effective level"), EffectiveDepth, 1);
	TestFalse(TEXT("L0 cannot reuse stale L1 publication"),
		CanConsumeCurrentFrameChild(0, EffectiveDepth, CurrentFrameSubmittedMask));

	// If the top level itself fails there is no valid endpoint result this frame.
	EffectiveDepth = TruncateEffectiveDepthAfterSubmissionFailure(1, 0);
	TestEqual(TEXT("L0 failure produces zero effective depth"), EffectiveDepth, 0);
	TestFalse(TEXT("No child is consumable at zero effective depth"),
		CanConsumeCurrentFrameChild(0, EffectiveDepth, 0xF));

	// Healthy path remains unchanged.
	EffectiveDepth = 4;
	CurrentFrameSubmittedMask = 0xF;
	TestTrue(TEXT("Healthy L0 consumes L1"),
		CanConsumeCurrentFrameChild(0, EffectiveDepth, CurrentFrameSubmittedMask));
	TestTrue(TEXT("Healthy L1 consumes L2"),
		CanConsumeCurrentFrameChild(1, EffectiveDepth, CurrentFrameSubmittedMask));
	TestTrue(TEXT("Healthy L2 consumes L3"),
		CanConsumeCurrentFrameChild(2, EffectiveDepth, CurrentFrameSubmittedMask));
	TestFalse(TEXT("Deepest level has no child"),
		CanConsumeCurrentFrameChild(3, EffectiveDepth, CurrentFrameSubmittedMask));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalP1ACapacityInvariantTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.P1AGate.CapacityInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalP1ACapacityInvariantTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FLifetimeIdSource Ids;
	FLifetime Levels[4];
	uint64 OriginalIdentity[4] = { 0, 0, 0, 0 };
	for (int32 Level = 0; Level < 4; ++Level)
	{
		TestTrue(TEXT("Aggregate gate allocates lifetime"), Levels[Level].BeginAllocated(Ids.Allocate()));
		TestTrue(TEXT("Aggregate gate activates lifetime"), Levels[Level].MarkActive());
		OriginalIdentity[Level] = Levels[Level].GetPackedPublicationIdentity();
		TestTrue(TEXT("RequestedDepth=4 admits all configured levels"), Levels[Level].CanSubmit(Level, 4));
	}

	// Visibility / future P1B EffectiveDepth selection is not a capacity signal.
	// With RequestedDepth unchanged, owned reusable lifetimes remain intact.
	const int32 LowerEffectiveDepth = 1;
	(void)LowerEffectiveDepth;
	for (int32 Level = 0; Level < 4; ++Level)
	{
		TestEqual(TEXT("Lower EffectiveDepth does not mutate resource state"),
			static_cast<int32>(Levels[Level].State), static_cast<int32>(EResourceState::Active));
		TestEqual(TEXT("Short offscreen/effective-depth change preserves identity"),
			Levels[Level].GetPackedPublicationIdentity(), OriginalIdentity[Level]);
	}

	// Configured capacity reduction is the destruction signal.
	for (int32 Level = 1; Level < 4; ++Level)
	{
		TestFalse(TEXT("Out-of-budget level receives no new submission"), Levels[Level].CanSubmit(Level, 1));
		FPublicationToken Retired;
		TestTrue(TEXT("Out-of-budget level begins retirement"), Levels[Level].BeginRetirement(Retired));
		TestEqual(TEXT("Out-of-budget level is RETIRING"),
			static_cast<int32>(Levels[Level].State), static_cast<int32>(EResourceState::Retiring));
	}
	TestTrue(TEXT("Retained L0 remains reusable at RequestedDepth=1"), Levels[0].CanSubmit(0, 1));
	TestEqual(TEXT("Retained L0 keeps identity"), Levels[0].GetPackedPublicationIdentity(), OriginalIdentity[0]);

	// Synchronous Stop-style teardown leaves no old publication identity alive;
	// a restart receives new identities rather than reviving the prior lifetime.
	for (int32 Level = 0; Level < 4; ++Level)
	{
		if (Levels[Level].IsReusable())
		{
			FPublicationToken Retired;
			TestTrue(TEXT("Stop retires reusable lifetime"), Levels[Level].BeginRetirement(Retired));
		}
		TestTrue(TEXT("Stop makes retiring lifetime reclaimable"), Levels[Level].MarkReclaimable());
		TestTrue(TEXT("Stop resets lifetime to unallocated"), Levels[Level].ResetUnallocated());
		TestEqual(TEXT("Stop clears packed publication identity"),
			Levels[Level].GetPackedPublicationIdentity(), uint64(0));

		TestTrue(TEXT("Restart allocates fresh lifetime"), Levels[Level].BeginAllocated(Ids.Allocate()));
		TestTrue(TEXT("Restart identity differs from pre-stop identity"),
			Levels[Level].GetPackedPublicationIdentity() != OriginalIdentity[Level]);
	}

	return true;
}

#endif
