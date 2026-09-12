#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalMath.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalSystem.h"
#include "Interior/InteriorPortalCameraState.h"
#include "Interior/InteriorPortalMovementComponent.h"
#include "Interior/InteriorChildCharacter.h"
#include "Interior/InteriorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Math/PerspectiveMatrix.h"
#include "Kismet/GameplayStatics.h"

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
	TestTrue(TEXT("P2-A temporal capture is enabled by default"), System->bCaptureTemporalAA);
	TestTrue(TEXT("P2 production Lumen cache baseline is half resolution"), FMath::IsNearlyEqual(System->CaptureLumenSurfaceCacheResolution,0.5f));
	TestFalse(TEXT("P2-A exposure isolation is diagnostic-only and defaults off"), System->bExposureIsolationDiagnostic);
	TestTrue(TEXT("P2-B portal-view exposure-domain correction defaults on"), FMath::IsNearlyEqual(A->PortalViewExposureCorrection,1.0f));
	System->BluePortal=A; System->OrangePortal=B;
	TestTrue(TEXT("Explicit placed pair is linked"),System->IsLinked());
	UBoxComponent* PortalSupport = NewObject<UBoxComponent>(A, TEXT("PortalSupport"));
	A->Support = PortalSupport;
	const FTransform PortalFrame = A->GetLogicalFrame();
	FHitResult PortalWallHit;
	PortalWallHit.Component = PortalSupport;
	TestTrue(TEXT("Flashlight clearance ignores the support wall inside the portal aperture"),
		System->IsFlashlightTraceThroughPortal(PortalWallHit,
			PortalFrame.TransformPosition(FVector(8, 0, 0)),
			PortalFrame.TransformPosition(FVector(1, 0, 0)), 4.0f));
	TestFalse(TEXT("Flashlight clearance still retracts outside the portal aperture"),
		System->IsFlashlightTraceThroughPortal(PortalWallHit,
			PortalFrame.TransformPosition(FVector(8, A->HalfWidth + 20, 0)),
			PortalFrame.TransformPosition(FVector(1, A->HalfWidth + 20, 0)), 4.0f));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalSweptApertureTest,
	"SlayTheSpireDemo.Interior.Portals.SweptCapsuleAperture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalSweptApertureTest::RunTest(const FString& Parameters)
{
	for (const FVector Spine : {FVector(0,0,37),FVector(37,0,0),FVector(20,15,27)})
	{
		double Enter,Leave; FVector N;
		TestTrue(TEXT("Centered upright/rotated capsule fits"),InteriorPortalMath::CapsuleApertureInterval(
			FVector(10,0,0),FVector::ZeroVector,Spine,24,65*.94,115*.94,Enter,Leave,N));
		TestTrue(TEXT("Fast diagonal move has a bounded legal interval"),InteriorPortalMath::CapsuleApertureInterval(
			FVector(10,0,0),FVector(-1000,800,0),Spine,24,65*.94,115*.94,Enter,Leave,N));
		TestTrue(TEXT("Lateral exit is clipped before leaving solid support"),Leave>0 && Leave<.1);
		TestTrue(TEXT("Collision normal opposes lateral escape"),N.Y<0);
		// Independently verify capsule support samples densely, not the polygon implementation.
		const FVector Center=FVector(10,0,0)+FVector(-1000,800,0)*Leave;
		for (int32 I=0; I<360; ++I)
		{
			const double A=FMath::DegreesToRadians(double(I));
			const FVector Rim(0,24*FMath::Cos(A),24*FMath::Sin(A));
			if (!InteriorPortalMath::Inside(Center+Spine+Rim,65*.94,115*.94)
				|| !InteriorPortalMath::Inside(Center-Spine+Rim,65*.94,115*.94))
			{ AddError(TEXT("A clipped capsule sample escaped the true ellipse")); break; }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalQuaternionTest,
	"SlayTheSpireDemo.Interior.Portals.QuaternionCamera", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalQuaternionTest::RunTest(const FString& Parameters)
{
	FInteriorPortalCameraState State;
	const FQuat Q=InteriorPortalMath::Rotation(FTransform::Identity,FTransform(FRotator(90,45,0)));
	const FQuat Initial=FRotator(20,130,0).Quaternion();
	State.Transfer(Q,Initial);
	const FQuat Before=State.Orientation;
	State.ApplyInput(FRotator(0,12,0));
	const FVector Expected=FQuat(Before.GetUpVector(),FMath::DegreesToRadians(12.)).RotateVector(Before.GetForwardVector());
	TestTrue(TEXT("Rolled mouse yaw uses camera up"),State.Orientation.GetForwardVector().Equals(Expected,1.e-5));
	const FQuat Active=State.Orientation;
	State.Transfer(Q.Inverse(),FQuat::Identity);
	TestTrue(TEXT("Second transfer composes current quaternion without Euler reset"),State.Orientation.Equals(Q.Inverse()*Active,1.e-5));
	const FVector Direction=State.Orientation.GetForwardVector();
	for (int32 I=0; I<240; ++I) { State.RecoverHorizon(1.f/60); }
	TestTrue(TEXT("Comfort recovery preserves look direction"),State.Orientation.GetForwardVector().Equals(Direction,1.e-4));
	TestFalse(TEXT("Recovery releases transient view ownership"),State.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalMovementGateTest,
	"SlayTheSpireDemo.Interior.Portals.CharacterMovementGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalMovementGateTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	World->InitializeActorsForPlay(FURL());
	AInteriorPortalSystem* System=World->SpawnActor<AInteriorPortalSystem>();
	AInteriorPortal* A=World->SpawnActor<AInteriorPortal>();
	AInteriorPortal* B=World->SpawnActor<AInteriorPortal>();
	A->SetActorLocation(FVector(0,0,100));
	B->SetActorLocation(FVector(1000,0,100));
	for (AInteriorPortal* Portal : {A,B})
	{
		AActor* Wall=World->SpawnActor<AActor>();
		UBoxComponent* Box=NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(5,300,300));
		Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
		Wall->SetActorLocation(Portal->GetActorLocation()-FVector(5,0,0));
		Portal->Support=Box;
	}
	System->BluePortal=A; System->OrangePortal=B;
	AInteriorPlayerController* Player=World->SpawnActor<AInteriorPlayerController>();
	AInteriorChildCharacter* Pawn=World->SpawnActor<AInteriorChildCharacter>();
	Player->Possess(Pawn); Player->PortalSystem=System;
	Pawn->SetActorLocation(FVector(80,0,100));
	System->Tick(.016f);
	TestTrue(TEXT("Fixture local player is discoverable"),UGameplayStatics::GetPlayerPawn(System,0)==Pawn);
	UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
	TestNotNull(TEXT("Actual character uses portal movement component"),Cast<UInteriorPortalMovementComponent>(Movement));
	FHitResult Hit;
	Movement->MoveUpdatedComponent(FVector(-70,0,0),FQuat::Identity,true,&Hit);
	AddInfo(FString::Printf(TEXT("Entry move: %s; hit=%s; state=%d; ignore count=%d"),*Pawn->GetActorLocation().ToString(),
		*GetNameSafe(Hit.GetComponent()),int32(System->PlayerCrossingState),Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().Num()));
	TestTrue(TEXT("Full capsule enters legal aperture"),Pawn->GetActorLocation().X<12);
	Movement->MoveUpdatedComponent(FVector(-500,600,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("High-speed lateral escape produces a collision"),Hit.bBlockingHit);
	TestTrue(TEXT("Capsule cannot leave through wall beside aperture"),FMath::Abs(Pawn->GetActorLocation().Y)<42);
	System->UpdateTraversal(Player);
	TestEqual(TEXT("One swept entry commits exactly once"),System->PlayerCrossings,1);
	const FVector Exited=Pawn->GetActorLocation();
	Movement->MoveUpdatedComponent(FVector(-100,0,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Immediate reversal cannot go behind exit"),Pawn->GetActorLocation().X>=B->GetActorLocation().X);
	System->UpdateTraversal(Player);
	TestEqual(TEXT("Same-interval reversal cannot ping-pong"),System->PlayerCrossings,1);
	// Invalidate the pair while still intersecting; the system must recover before restoring contacts.
	B->bPlaced=false; System->Tick(.016f);
	TestTrue(TEXT("Invalid pair clears temporary movement ignores"),Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().IsEmpty());
	TestTrue(TEXT("Pair invalidation leaves capsule safely in front of exit"),Pawn->GetActorLocation().X>=1024);
	World->DestroyWorld(false);
	return true;
}
#endif
