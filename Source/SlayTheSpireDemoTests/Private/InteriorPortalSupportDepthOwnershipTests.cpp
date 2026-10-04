#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalSupportDepthOwnership.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "SceneUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSupportDepthOwnershipTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.SupportDepthOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalSupportDepthOwnershipTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto MakeSupport = [World]()
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Owner);
		Owner->SetRootComponent(Box); Owner->AddInstanceComponent(Box);
		return Box;
	};
	UBoxComponent* A = MakeSupport();
	UBoxComponent* B = MakeSupport();
	UBoxComponent* Other = MakeSupport();
	Other->SetRenderCustomDepth(true); Other->SetCustomDepthStencilValue(1);
	A->SetCustomDepthStencilValue(91);
	A->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_2);
	IConsoleVariable* Mode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
	const int32 OriginalMode = Mode->GetInt();
	const EConsoleVariableFlags OriginalFlags = Mode->GetFlags();
	IConsoleVariable* Jitter = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepthTemporalAAJitter"));
	const int32 OriginalJitter = Jitter->GetInt();
	auto SetValue = [](IConsoleVariable* Var, int32 Value)
	{
		Var->Set(Value, EConsoleVariableFlags(Var->GetFlags() & ECVF_SetByMask));
	};
	SetValue(Jitter, 0);
	SetValue(Mode, 1);
	FInteriorPortalSupportDepthOwnership Leases;
	Leases.Refresh(World, A, B);
	const uint32 AId = Leases.GetStencil(A), BId = Leases.GetStencil(B);
	TestTrue(TEXT("Two support leases are nonzero and distinct"), AId != 0 && BId != 0 && AId != BId);
	TestTrue(TEXT("Unrelated stencil identities remain reserved"), AId != 1 && BId != 1);
	TestEqual(TEXT("The independent depth/stencil pass is enabled"), Mode->GetInt(), 3);
	TestEqual(TEXT("UE's notified renderer actually writes stencil"), GetCustomDepthMode(), ECustomDepthMode::EnabledWithStencil);
	TestEqual(TEXT("Support depth uses the receiving view's jittered rays"), Jitter->GetInt(), 1);
	TestEqual(TEXT("Console priority is unchanged by the temporary mode"), int32(Mode->GetFlags()), int32(OriginalFlags));
	Leases.Refresh(World, A, B);
	TestEqual(TEXT("Continuous rendering retains its attachment identity"), Leases.GetStencil(A), AId);
	Other->SetCustomDepthStencilValue(int32(AId));
	Leases.Refresh(World, A, B);
	TestEqual(TEXT("A newly introduced stencil alias revokes the active lease"), Leases.GetStencil(A), 0u);
	TestEqual(TEXT("An alias does not revoke the other endpoint"), Leases.GetStencil(B), BId);
	Other->SetCustomDepthStencilValue(1);
	Leases.Refresh(World, A, B);
	TestEqual(TEXT("Removing an external alias restores the original identity"), Leases.GetStencil(A), AId);
	Leases.Refresh(World, A, A);
	TestEqual(TEXT("A shared support retains one identity"), Leases.GetStencil(A), AId);
	TestFalse(TEXT("A replaced support is restored immediately"), B->bRenderCustomDepth);
	Leases.Refresh(World, nullptr, nullptr);
	TestFalse(TEXT("Unlinking restores the prior depth participation"), A->bRenderCustomDepth);
	TestEqual(TEXT("Unlinking restores the prior stencil"), A->CustomDepthStencilValue, 91);
	TestEqual(TEXT("Unlinking restores the prior write mask"), A->CustomDepthStencilWriteMask, ERendererStencilMask::ERSM_2);
	TestEqual(TEXT("Unlinking retains the backend mode without global reregistration"), Mode->GetInt(), 3);
	Leases.Reset();
	TestEqual(TEXT("Stopping restores the original global depth mode"), Mode->GetInt(), 1);
	TestEqual(TEXT("Stopping restores the original jitter policy"), Jitter->GetInt(), 0);
	TestTrue(TEXT("Unrelated custom-depth participation survives teardown"), Other->bRenderCustomDepth);

	Leases.Refresh(World, A, B);
	A->SetCustomDepthStencilValue(199); // Another feature takes ownership.
	Leases.Refresh(World, A, B);
	TestEqual(TEXT("External component mutation revokes its identity"), Leases.GetStencil(A), 0u);
	Leases.Reset();
	TestEqual(TEXT("Teardown cannot overwrite a newer owner"), A->CustomDepthStencilValue, 199);
	TestTrue(TEXT("The newer owner's custom depth remains enabled"), A->bRenderCustomDepth);
	A->SetRenderCustomDepth(false);

	Leases.Refresh(World, Other, nullptr);
	TestEqual(TEXT("Unique preexisting identity is reused without mutation"), Leases.GetStencil(Other), 1u);
	Leases.Reset();
	TestTrue(TEXT("Releasing borrowed identity preserves its owner"), Other->bRenderCustomDepth);
	B->SetRenderCustomDepth(true); B->SetCustomDepthStencilValue(1);
	Leases.Refresh(World, Other, B);
	TestEqual(TEXT("Duplicate existing identity is rejected"), Leases.GetStencil(Other), 0u);
	TestEqual(TEXT("Neither ambiguous support receives native ownership"), Leases.GetStencil(B), 0u);
	Leases.Reset();
	TestEqual(TEXT("Rejected leases do not outlive backend teardown"), Mode->GetInt(), 1);
	B->SetRenderCustomDepth(false);

	Leases.Refresh(World, A, nullptr);
	SetValue(Mode, 0);
	Leases.Refresh(World, A, nullptr);
	TestEqual(TEXT("External renderer mode change revokes composition"), Leases.GetStencil(A), 0u);
	Leases.Reset();
	TestEqual(TEXT("Teardown preserves an external renderer mode change"), Mode->GetInt(), 0);

	SetValue(Mode, 1);
	for (int32 Id = 2; Id <= 255; ++Id)
	{
		UBoxComponent* Reserved = MakeSupport();
		Reserved->SetRenderCustomDepth(true); Reserved->SetCustomDepthStencilValue(Id);
	}
	Leases.Refresh(World, A, nullptr);
	TestEqual(TEXT("Identity exhaustion fails closed rather than aliasing geometry"), Leases.GetStencil(A), 0u);
	TestFalse(TEXT("Exhaustion does not change support participation"), A->bRenderCustomDepth);
	Leases.Reset();
	SetValue(Mode, OriginalMode);
	SetValue(Jitter, OriginalJitter);
	World->DestroyWorld(false);
	return true;
}
#endif
