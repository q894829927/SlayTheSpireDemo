#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalMath.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalSystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Math/PerspectiveMatrix.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalMappingTest,
	"SlayTheSpireDemo.Interior.Portals.RigidMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalMappingTest::RunTest(const FString& Parameters)
{
	const FTransform A(FRotator(0, 90, 0), FVector(10, 20, 130));
	for (const FRotator Orientation : {FRotator(0, -90, 0), FRotator(90, 20, 0), FRotator(-90, 0, 0), FRotator(25, 173, 47)})
	{
		const FTransform B(Orientation, FVector(1700, -850, 300));
		const FVector P(35, -22, 78), V(500, -130, -900);
		const FVector Mapped = InteriorPortalMath::Position(P, A, B);
		TestTrue(TEXT("Round trip restores world point"), InteriorPortalMath::Position(Mapped, B, A).Equals(P, .001));
		const FQuat Q = InteriorPortalMath::Rotation(A, B);
		TestTrue(TEXT("Momentum magnitude preserved"), FMath::IsNearlyEqual(Q.RotateVector(V).Size(), V.Size(), .001));
		TestTrue(TEXT("Inward normal becomes outward normal"), Q.RotateVector(-A.GetUnitAxis(EAxis::X)).Equals(B.GetUnitAxis(EAxis::X), .001));
		TestTrue(TEXT("Handedness preserved"), FVector::CrossProduct(Q.GetAxisX(), Q.GetAxisY()).Equals(Q.GetAxisZ(), .001));
	}
	FVector Intersection;
	TestTrue(TEXT("High-speed swept crossing detected"), InteriorPortalMath::Crossed(FVector(1000,0,0), FVector(-2000,0,0), FTransform::Identity,65,115,Intersection));
	TestTrue(TEXT("Crossing point lies on plane"), Intersection.IsNearlyZero());
	TestFalse(TEXT("Back-to-front passage rejected"), InteriorPortalMath::Crossed(FVector(-2,0,0), FVector(2,0,0), FTransform::Identity,65,115,Intersection));
	TestFalse(TEXT("Outside ellipse is not a teleport"), InteriorPortalMath::Crossed(FVector(2,60,100), FVector(-2,60,100), FTransform::Identity,65,115,Intersection));
	TestFalse(TEXT("Resting on plane does not retrigger"), InteriorPortalMath::Crossed(FVector(0,0,0), FVector(-2,0,0), FTransform::Identity,65,115,Intersection));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalProjectionTest,
	"SlayTheSpireDemo.Interior.Portals.ObliqueProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalProjectionTest::RunTest(const FString& Parameters)
{
	const FMatrix Base = FReversedZPerspectiveMatrix(PI/4, 1920, 1080, 1);
	for (const FVector4 Plane : {FVector4(0,0,1,-50), FVector4(.2,-.1,1,-50)})
	{
		FMatrix Projection;
		TestTrue(TEXT("Valid oblique plane builds a projection"), InteriorPortalMath::TryObliqueProjection(Base, Plane, Projection));
		const FVector4 OnPlane(20,10,50-20*Plane.X-10*Plane.Y,1);
		const FVector4 Clip = Projection.TransformFVector4(OnPlane);
		TestTrue(TEXT("Exit plane is reversed-Z near boundary"), FMath::IsNearlyEqual(Clip.Z, Clip.W,.001));
		const FVector4 Behind = Projection.TransformFVector4(FVector4(OnPlane.X, OnPlane.Y, OnPlane.Z-10,1));
		const FVector4 Front = Projection.TransformFVector4(FVector4(OnPlane.X, OnPlane.Y, OnPlane.Z+10,1));
		TestTrue(TEXT("Geometry behind exit is clipped"), Behind.Z > Behind.W);
		TestTrue(TEXT("Exit room remains visible"), Front.Z < Front.W && Front.Z > 0);
		TestTrue(TEXT("Screen alignment unchanged"), FMath::IsNearlyEqual(Clip.X,Base.TransformFVector4(OnPlane).X,.001));
	}
	FMatrix InvalidProjection;
	TestFalse(TEXT("Degenerate clip plane is rejected instead of silently unclipping"),
		InteriorPortalMath::TryObliqueProjection(Base, FVector4(0,0,0,0), InvalidProjection));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalPlacementTest,
	"SlayTheSpireDemo.Interior.Portals.PlacementAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalPlacementTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	AInteriorPortalSystem* System=World->SpawnActor<AInteriorPortalSystem>();
	AInteriorPortal* A=World->SpawnActor<AInteriorPortal>();
	AInteriorPortal* B=World->SpawnActor<AInteriorPortal>();
	const FTransform LogicalFrame(FRotator(0,35,0), FVector(100,200,300));
	A->SetActorTransform(LogicalFrame);
	A->SurfaceVisualBias = .75f;
	A->RefreshAppearance();
	TestTrue(TEXT("Portal actor transform remains the logical aperture frame"), A->GetLogicalFrame().Equals(LogicalFrame,.001));
	TestTrue(TEXT("Visual surface bias does not move the logical frame"),
		A->Surface->GetComponentLocation().Equals(LogicalFrame.TransformPosition(FVector(.75f,0,0)),.001));
	TestTrue(TEXT("Portal surface faces the crossing frame normal"), A->Surface->GetUpVector().Equals(A->GetLogicalFrame().GetUnitAxis(EAxis::X),.001));
	TestTrue(TEXT("Native SceneCapture clipping is the default P1 path"), System->RenderClipMode == EInteriorPortalRenderClipMode::NativeClipPlane);
	System->BluePortal=A; System->OrangePortal=B;
	TestTrue(TEXT("Explicit placed pair is linked"),System->IsLinked());
	System->OrangePortal=A;
	TestFalse(TEXT("Self link rejected"),System->IsLinked());
	System->OrangePortal=B;
	FTransform Frame; FString Reason;
	TestFalse(TEXT("Miss does not create portal"),System->ValidatePlacement(FHitResult(),FVector::RightVector,A,Frame,Reason));
	TestFalse(TEXT("No player cannot shoot"),System->FirePortal(nullptr,false));
	System->ResetPortals();
	TestFalse(TEXT("Clearing pair disables traversal"),System->IsLinked());
	TestFalse(TEXT("Clearing hides blue endpoint"),A->bPlaced);
	TestFalse(TEXT("Clearing hides orange endpoint"),B->bPlaced);
	World->DestroyWorld(false);
	return true;
}
#endif
