#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalBodyGeometry.h"
#include "Interior/InteriorPortalTravellerRegistry.h"
#include "Interior/InteriorPortalQuery.h"
#include "Interior/InteriorPortalSystem.h"
#include "Interior/InteriorPortal.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
	using namespace InteriorPortalPhysics;
	using FBodyGeometry = InteriorPortalPhysics::FGeometry;
	FBodyGeometry BoxGeometry(const FVector& Half)
	{
		FBodyGeometry G; FPrimitive P; P.Shape = EShape::Box;
		for (int32 X : {-1, 1}) for (int32 Y : {-1, 1}) for (int32 Z : {-1, 1})
		{ P.Vertices.Add(FVector(X * Half.X, Y * Half.Y, Z * Half.Z)); }
		G.Primitives.Add(P); return G;
	}
	FBodyGeometry SphereGeometry(double Radius, const FVector& Center = FVector::ZeroVector)
	{
		FBodyGeometry G; FPrimitive P; P.Radius = Radius; P.Center = Center;
		G.Primitives.Add(P); return G;
	}
	struct FTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FTestWorld() { World->InitializeActorsForPlay(FURL()); }
		~FTestWorld() { World->DestroyWorld(false); }
		UBoxComponent* MakeBody(const FVector& Extent = FVector(10)) const
		{
			AActor* Owner = World->SpawnActor<AActor>();
			UBoxComponent* Body = NewObject<UBoxComponent>(Owner);
			Owner->SetRootComponent(Body); Body->SetBoxExtent(Extent);
			Body->SetCollisionProfileName(TEXT("PhysicsActor"));
			Body->RegisterComponent(); Body->SetSimulatePhysics(true); return Body;
		}
	};
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCollisionContainmentTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsFoundation.CollisionContainment", Flags)
bool FPortalCollisionContainmentTest::RunTest(const FString& Parameters)
{
	const FBodyGeometry Long = BoxGeometry(FVector(80, 8, 8));
	TestTrue(TEXT("Thin body aligned along portal normal fits"),
		EvaluatePose(Long, FTransform::Identity, FTransform::Identity, 65, 115).Fits());
	TestFalse(TEXT("Same long body rotated into aperture width is blocked"),
		EvaluatePose(Long, FTransform(FRotator(0, 90, 0)), FTransform::Identity, 65, 115).Fits());
	const FTransform Corner(FVector(0, 50, 70));
	TestTrue(TEXT("Center is inside ellipse"), EvaluatePose(SphereGeometry(0), Corner, FTransform::Identity, 65, 115).Fits());
	TestFalse(TEXT("Box corner outside ellipse blocks despite legal center and axis bounds"),
		EvaluatePose(BoxGeometry(FVector(5, 10, 10)), Corner, FTransform::Identity, 65, 115).Fits());
	TestTrue(TEXT("Naive radius-subtracted ellipse would accept unsafe sphere"),
		FMath::Square(32. / 45.) + FMath::Square(66. / 95.) < 1);
	TestTrue(TEXT("Independent point within sphere lies outside actual ellipse"),
		FVector2D(18, 8).Size() < 20 && FMath::Square(50. / 65.) + FMath::Square(74. / 115.) > 1);
	TestFalse(TEXT("Conservative sphere bound rejects that unsafe placement"),
		EvaluatePose(SphereGeometry(20, FVector(0, 32, 66)), FTransform::Identity, FTransform::Identity, 65, 115).Fits());
	FBodyGeometry Capsule = SphereGeometry(10); Capsule.Primitives[0].Shape = EShape::Capsule;
	Capsule.Primitives[0].HalfSegment = FVector(0, 0, 80);
	TestTrue(TEXT("Upright capsule fits"), EvaluatePose(Capsule, FTransform::Identity, FTransform::Identity, 65, 115).Fits());
	TestFalse(TEXT("Capsule rotated across narrow axis blocks"),
		EvaluatePose(Capsule, FTransform(FRotator(0, 0, 90)), FTransform::Identity, 65, 115).Fits());
	FBodyGeometry Compound = SphereGeometry(10);
	Compound.Primitives.Append(SphereGeometry(10, FVector(0, 60, 0)).Primitives);
	TestFalse(TEXT("Every compound element must fit, not just root center"),
		EvaluatePose(Compound, FTransform::Identity, FTransform::Identity, 65, 115).Fits());
	FBodyGeometry Convex; FPrimitive Hull; Hull.Shape = EShape::Convex;
	Hull.Vertices = { FVector(0,0,0), FVector(2,0,0), FVector(0,2,0), FVector(0,0,2), FVector(0,66,0) };
	Convex.Primitives.Add(Hull);
	TestFalse(TEXT("Distal convex vertex blocks even when first tetrahedron fits"),
		EvaluatePose(Convex, FTransform::Identity, FTransform::Identity, 65, 115).Fits());
	const FTransform Frame(FRotator(23, 107, -31), FVector(1400, -730, 450));
	const FTransform Local(FRotator(0, 0, 0), FVector(12, 20, 30));
	const auto A = EvaluatePose(Long, Local, FTransform::Identity, 65, 115);
	const auto B = EvaluatePose(Long, Local * Frame, Frame, 65, 115);
	TestTrue(TEXT("Fit invariant under shared world rotation and translation"), A.Result == B.Result && A.Fits());
	TestTrue(TEXT("Normal support interval maps into logical portal frame"),
		FMath::IsNearlyEqual(B.MinNormal, -68., 1.e-6) && FMath::IsNearlyEqual(B.MaxNormal, 92., 1.e-6));
	TestFalse(TEXT("Zero aperture fails closed"), EvaluatePose(Long, Local, Frame, 0, 115).Fits());
	FTransform Scaled = FTransform::Identity; Scaled.SetScale3D(FVector(2));
	TestTrue(TEXT("Unrefreshed component scale cannot reuse geometry"),
		EvaluatePose(Long, Scaled, FTransform::Identity, 65, 115).Result == EGeometryResult::ScaleChanged);
	TestTrue(TEXT("Nonunit portal frame fails closed"),
		EvaluatePose(Long, FTransform::Identity, Scaled, 65, 115).Result == EGeometryResult::InvalidPose);
	FBodyGeometry Broken = Long; Broken.Primitives[0].Vertices.Pop();
	TestTrue(TEXT("Malformed shape does not silently authorize passage"),
		EvaluatePose(Broken, FTransform::Identity, FTransform::Identity, 65, 115).Result == EGeometryResult::InvalidGeometry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCollisionTranslationTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsFoundation.TranslationEligibility", Flags)
bool FPortalCollisionTranslationTest::RunTest(const FString& Parameters)
{
	const FBodyGeometry Box = BoxGeometry(FVector(10));
	const FTransform From(FVector(100, -20, 10)), To(FVector(-100, 20, 10));
	const auto Fit = EvaluateTranslation(Box, From, To, FTransform::Identity, 65, 115, .5);
	TestTrue(TEXT("Fixed orientation translation covers whole convex aperture segment"), Fit.Fits());
	TestTrue(TEXT("Normal interval includes both ends"), Fit.MinNormal == -110 && Fit.MaxNormal == 110);
	for (int32 I = 0; I <= 100; ++I)
	{
		TestTrue(TEXT("Independent intermediate placement stays inside accepted sweep"),
			EvaluatePose(Box, FTransform(FMath::Lerp(From.GetLocation(), To.GetLocation(), I / 100.)),
				FTransform::Identity, 65, 115, .5).Fits());
	}
	TestFalse(TEXT("Lateral segment with illegal endpoint blocks"),
		EvaluateTranslation(Box, From, FTransform(FVector(-100, 64, 0)), FTransform::Identity, 65, 115).Fits());
	const FBodyGeometry Long = BoxGeometry(FVector(80, 8, 8));
	const FTransform Rotated(FRotator(0, 180, 0));
	TestTrue(TEXT("Both endpoints of rotating long box fit"),
		EvaluatePose(Long, FTransform::Identity, FTransform::Identity, 65, 115).Fits()
		&& EvaluatePose(Long, Rotated, FTransform::Identity, 65, 115).Fits());
	TestTrue(TEXT("Rotating sweep explicitly unsupported, never inferred from endpoints"),
		EvaluateTranslation(Long, FTransform::Identity, Rotated, FTransform::Identity, 65, 115).Result
			== EGeometryResult::RotationSweepUnsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCollisionExtractionTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsFoundation.ShapeExtraction", Flags)
bool FPortalCollisionExtractionTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture; FBodyGeometry G;
	UBoxComponent* Box = Fixture.MakeBody(FVector(7, 11, 13));
	Box->SetWorldScale3D(FVector(2, 3, 4));
	TestTrue(TEXT("Root box collision extraction succeeds"), ExtractGeometry(Box, G) == EGeometryResult::Fits);
	TestTrue(TEXT("Scale is baked exactly once from collision dimensions"),
		G.BakedScale == FVector(2, 3, 4) && G.Primitives[0].Vertices.Contains(FVector(14, 33, 52)));
	TestTrue(TEXT("Pose scale does not multiply baked support twice"),
		EvaluatePose(G, Box->GetComponentTransform(), FTransform::Identity, 65, 115).Fits());
	AActor* SphereOwner = Fixture.World->SpawnActor<AActor>();
	USphereComponent* Sphere = NewObject<USphereComponent>(SphereOwner);
	SphereOwner->SetRootComponent(Sphere); Sphere->SetSphereRadius(12);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor")); Sphere->RegisterComponent();
	Sphere->SetWorldScale3D(FVector(2, 3, 4));
	TestTrue(TEXT("Nonuniform sphere uses Chaos minimum-axis radius"),
		ExtractGeometry(Sphere, G) == EGeometryResult::Fits && FMath::IsNearlyEqual(G.Primitives[0].Radius, 24.));
	AActor* CapsuleOwner = Fixture.World->SpawnActor<AActor>();
	UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(CapsuleOwner);
	CapsuleOwner->SetRootComponent(Capsule); Capsule->SetCapsuleSize(10, 30);
	Capsule->SetCollisionProfileName(TEXT("PhysicsActor")); Capsule->RegisterComponent();
	Capsule->SetWorldScale3D(FVector(2, 3, 4));
	TestTrue(TEXT("Capsule extraction succeeds"), ExtractGeometry(Capsule, G) == EGeometryResult::Fits);
	TestTrue(TEXT("Physics capsule radius and spine follow BodySetup scaling, not visual bounds"),
		FMath::IsNearlyEqual(G.Primitives[0].Radius, 30.) && G.Primitives[0].HalfSegment.Equals(FVector(0,0,90)));
	AActor* MeshOwner = Fixture.World->SpawnActor<AActor>();
	UStaticMeshComponent* MeshBody = NewObject<UStaticMeshComponent>(MeshOwner);
	MeshOwner->SetRootComponent(MeshBody); MeshBody->SetCollisionProfileName(TEXT("PhysicsActor"));
	UStaticMesh* Mesh = NewObject<UStaticMesh>(); Mesh->CreateBodySetup();
	UBodySetup* Setup = Mesh->GetBodySetup(); Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	FKBoxElem OffsetBox(10, 14, 18); OffsetBox.Center = FVector(0, 5, 0);
	Setup->AggGeom.BoxElems.Add(OffsetBox);
	FKConvexElem Hull; Hull.VertexData = {FVector(0,0,0), FVector(2,0,0), FVector(0,3,0), FVector(0,0,4)};
	Hull.SetTransform(FTransform(FRotator(0,90,0), FVector(10,20,30), FVector(2,1,1)));
	Hull.UpdateElemBox(); Setup->AggGeom.ConvexElems.Add(Hull);
	MeshBody->SetStaticMesh(Mesh); MeshBody->SetWorldScale3D(FVector(2,3,4));
	TestTrue(TEXT("Compound authored simple collision extracts without render bounds"),
		ExtractGeometry(MeshBody, G) == EGeometryResult::Fits && G.Primitives.Num() == 2);
	TestTrue(TEXT("Convex cooking transform precedes component scale"),
		G.Primitives.Num() == 2 && G.Primitives[1].Vertices[1].Equals(FVector(20,72,120), 1.e-6));
	Setup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	TestTrue(TEXT("Complex-as-simple collision rejected explicitly"), ExtractGeometry(MeshBody,G) == EGeometryResult::UnsupportedCollision);
	Box->SetWorldScale3D(FVector(-1,1,1));
	TestTrue(TEXT("Mirrored scale outside supported contract"), ExtractGeometry(Box,G) == EGeometryResult::UnsupportedScale);
	TestTrue(TEXT("Failed extraction clears prior output"), G.Primitives.IsEmpty());
	SphereOwner->SetRootComponent(NewObject<USceneComponent>(SphereOwner));
	TestTrue(TEXT("Child collision component cannot impersonate root traveller"), ExtractGeometry(Sphere,G) == EGeometryResult::UnsupportedComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalGenerationQueryTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsFoundation.GenerationAndSharedQuery", Flags)
bool FPortalGenerationQueryTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture; auto* System = Fixture.World->SpawnActor<AInteriorPortalSystem>();
	UBoxComponent* First = Fixture.MakeBody(), *Second = Fixture.MakeBody();
	TestTrue(TEXT("Bodies register through existing system boundary"),
		System->RegisterPhysicsTraveller(First) && System->RegisterPhysicsTraveller(Second));
	FTravellerSnapshot FirstSnapshot, SecondSnapshot, Current;
	TestTrue(TEXT("Registered bodies expose frozen geometry and stable tokens"),
		System->CapturePhysicsTraveller(First, FirstSnapshot) && System->CapturePhysicsTraveller(Second, SecondSnapshot));
	TestFalse(TEXT("Duplicate registration rejected"), System->RegisterPhysicsTraveller(First));
	System->UnregisterPhysicsTraveller(First);
	TestFalse(TEXT("Unregistered token invalid immediately"), System->IsCurrentPhysicsTraveller(FirstSnapshot.Handle));
	System->CapturePhysicsTraveller(Second, Current);
	TestTrue(TEXT("Removing earlier array entry cannot renumber another body"), Current.Handle == SecondSnapshot.Handle);
	System->RegisterPhysicsTraveller(First); System->CapturePhysicsTraveller(First, Current);
	TestTrue(TEXT("Re-registration retains body ID but changes generation"),
		Current.Handle.Id == FirstSnapshot.Handle.Id && Current.Handle.Generation > FirstSnapshot.Handle.Generation);
	Second->SetWorldScale3D(FVector(2));
	TestFalse(TEXT("Scale mutation invalidates old token before next tick"), System->IsCurrentPhysicsTraveller(SecondSnapshot.Handle));
	System->Tick(0); System->CapturePhysicsTraveller(Second, Current);
	TestTrue(TEXT("Refresh versions collision geometry without changing ID"),
		Current.Handle.Id == SecondSnapshot.Handle.Id && Current.Handle.Generation > SecondSnapshot.Handle.Generation
		&& Current.Geometry.BakedScale == FVector(2));
	SecondSnapshot = Current;
	Second->SetBoxExtent(FVector(11));
	TestFalse(TEXT("Collision edit invalidates token even without scale change"), System->IsCurrentPhysicsTraveller(SecondSnapshot.Handle));
	System->Tick(0); System->CapturePhysicsTraveller(Second, Current);
	TestTrue(TEXT("Collision edit increments generation"), Current.Handle.Generation > SecondSnapshot.Handle.Generation);
	FTravellerRegistry Other; EGeometryResult Reason;
	Other.Register(Second, Reason); FTravellerSnapshot OtherSnapshot; Other.Capture(Second, OtherSnapshot);
	TestTrue(TEXT("Registry epochs prevent cross-system aliasing"), OtherSnapshot.Handle.Epoch != Current.Handle.Epoch);
	TestFalse(TEXT("Foreign registry rejects valid token from first registry"), Other.IsCurrent(Current.Handle));
	Other.Reset(); TestFalse(TEXT("Reset retires all tokens"), Other.IsCurrent(OtherSnapshot.Handle));
	Other.Register(Second, Reason); FTravellerSnapshot AfterReset; Other.Capture(Second, AfterReset);
	TestTrue(TEXT("Reset never revives old identity"), AfterReset.Handle.Id != OtherSnapshot.Handle.Id);
	auto* A = Fixture.World->SpawnActor<AInteriorPortal>();
	auto* B = Fixture.World->SpawnActor<AInteriorPortal>();
	System->BluePortal = A; System->OrangePortal = B; A->bPlaced = true; B->bPlaced = true;
	System->Tick(0); const uint64 Pair = System->GetPhysicsPairGeneration();
	const FTransform From(FQuat::Identity, FVector(100,0,0), FVector(2));
	const FTransform To(FQuat::Identity, FVector(-100,0,0), FVector(2));
	const auto Pure = EvaluateTranslation(Current.Geometry, From, To, A->GetLogicalFrame(), A->HalfWidth, A->HalfHeight, .5);
	const auto Shared = InteriorPortalQuery::EvaluateBodyPassage(System, Current, From, To, A, Pair);
	TestTrue(TEXT("Held/free consumers share registered collision eligibility"), Pair > 0 && Pure.Fits() && Shared.Result == Pure.Result);
	A->HalfWidth += 1;
	TestTrue(TEXT("Endpoint mutation invalidates pair immediately"), System->GetPhysicsPairGeneration() == 0);
	TestTrue(TEXT("Old pair query cannot consume stale aperture"),
		InteriorPortalQuery::EvaluateBodyPassage(System, Current, From, To, A, Pair).Result == EGeometryResult::InvalidPair);
	System->Tick(0); TestTrue(TEXT("New pair geometry receives newer generation"), System->GetPhysicsPairGeneration() > Pair);
	System->UnregisterPhysicsTraveller(Second);
	TestTrue(TEXT("Outstanding query rejects retired body"),
		InteriorPortalQuery::EvaluateBodyPassage(System, Current, From, To, A, System->GetPhysicsPairGeneration()).Result == EGeometryResult::StaleIdentity);
	System->CapturePhysicsTraveller(First, Current);
	const uint64 FinalPair = System->GetPhysicsPairGeneration();
	System->Destroy();
	TestTrue(TEXT("Pending-destroy owner cannot authorize an outstanding query"),
		InteriorPortalQuery::EvaluateBodyPassage(System, Current, FTransform::Identity, FTransform::Identity,
			A, FinalPair).Result == EGeometryResult::StaleIdentity);
	First->GetOwner()->Destroy();
	FTravellerSnapshot Destroyed;
	TestFalse(TEXT("Destroyed body cannot re-enter a registry"), Other.Register(First, Reason));
	return true;
}
#endif
