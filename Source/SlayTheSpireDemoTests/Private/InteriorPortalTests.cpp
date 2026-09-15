#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalMath.h"
#include "Interior/InteriorPortalQuery.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalSystem.h"
#include "Interior/InteriorPortalRenderer.h"
#include "Interior/InteriorPortalCameraState.h"
#include "Interior/InteriorPortalMovementComponent.h"
#include "Interior/InteriorChildCharacter.h"
#include "Interior/InteriorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Math/PerspectiveMatrix.h"
#include "Kismet/GameplayStatics.h"
#include "SceneManagement.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalQueryTest,
	"SlayTheSpireDemo.Interior.Portals.PortalAwareQuery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalQueryTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	AInteriorPortalSystem* System=World->SpawnActor<AInteriorPortalSystem>();
	AInteriorPortal* A=World->SpawnActor<AInteriorPortal>();
	AInteriorPortal* B=World->SpawnActor<AInteriorPortal>();
	A->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(0,0,100)));
	B->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(1000,0,100)));
	auto MakeBox = [World](const FVector& Location,const FVector& Extent,const TCHAR* Name)
	{
		AActor* Owner=World->SpawnActor<AActor>();
		UBoxComponent* Box=NewObject<UBoxComponent>(Owner,Name);
		Owner->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Owner->SetActorLocation(Location);
		return Box;
	};
	A->Support=MakeBox(FVector(-5,0,100),FVector(5,300,300),TEXT("EntrySupport"));
	B->Support=MakeBox(FVector(995,0,100),FVector(5,300,300),TEXT("ExitSupport"));
	UBoxComponent* Target=MakeBox(FVector(1080,0,100),FVector(12,12,12),TEXT("PortalTarget"));
	System->BluePortal=A;
	System->OrangePortal=B;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalQueryTest),false);
	FHitResult Hit;
	TestTrue(TEXT("Line query continues through an aperture"),InteriorPortalQuery::LineTrace(
		System,FVector(100,0,100),FVector(-100,0,100),ECC_Visibility,Params,Hit,3,1.0f));
	TestTrue(TEXT("Line query returns the destination object"),Hit.GetComponent()==Target);
	TestTrue(TEXT("Mapped hit retains a bounded source-segment time"),Hit.Time>0.5f && Hit.Time<1.0f);
	TestTrue(TEXT("Sphere query continues through an aperture"),InteriorPortalQuery::SphereSweep(
		System,FVector(100,0,100),FVector(-100,0,100),5.0f,ECC_Visibility,Params,Hit,3,1.0f));
	TestTrue(TEXT("Sphere query returns the destination object"),Hit.GetComponent()==Target);
	TestTrue(TEXT("Support wall remains solid outside the aperture"),InteriorPortalQuery::LineTrace(
		System,FVector(100,100,100),FVector(-100,100,100),ECC_Visibility,Params,Hit,3,1.0f));
	TestTrue(TEXT("Outside-aperture query returns the support wall"),Hit.GetComponent()==A->Support);
	UBoxComponent* Obstacle=MakeBox(FVector(50,0,100),FVector(5,12,12),TEXT("PrePortalObstacle"));
	TestTrue(TEXT("Unrelated object before a portal wins"),InteriorPortalQuery::LineTrace(
		System,FVector(100,0,100),FVector(-100,0,100),ECC_Visibility,Params,Hit,3,1.0f));
	TestTrue(TEXT("Unrelated pre-portal hit is preserved"),Hit.GetComponent()==Obstacle);
	World->DestroyWorld(false);
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
	TestTrue(TEXT("P2-B portal-view exposure-domain correction is diagnostic-only and defaults off"),
		FMath::IsNearlyZero(A->PortalViewExposureCorrection));
	TestTrue(TEXT("SceneColorLinear is the explicit default capture mode"),
		System->CaptureColorMode == EInteriorPortalCaptureColorMode::SceneColorLinear);
	TestTrue(TEXT("FinalColorHDR maps to SCS_FinalColorHDR"),
		AInteriorPortalSystem::GetCaptureSourceForColorMode(EInteriorPortalCaptureColorMode::FinalColorHDR) == SCS_FinalColorHDR);
	TestTrue(TEXT("SceneColorLinear maps to SCS_SceneColorHDRNoAlpha"),
		AInteriorPortalSystem::GetCaptureSourceForColorMode(EInteriorPortalCaptureColorMode::SceneColorLinear) == SCS_SceneColorHDRNoAlpha);
	TestTrue(TEXT("FinalColorHDR enables capture eye adaptation"),
		AInteriorPortalSystem::UsesCaptureEyeAdaptation(EInteriorPortalCaptureColorMode::FinalColorHDR));
	TestFalse(TEXT("SceneColorLinear disables capture eye adaptation"),
		AInteriorPortalSystem::UsesCaptureEyeAdaptation(EInteriorPortalCaptureColorMode::SceneColorLinear));
	A->EnsureCaptureViews(3);
	TestEqual(TEXT("Three recursion capture components are created"), A->CaptureViews.Num(), 3);
	TestTrue(TEXT("Each recursion level owns a distinct SceneCapture component"),
		A->GetCaptureForDepth(0) != A->GetCaptureForDepth(1)
		&& A->GetCaptureForDepth(1) != A->GetCaptureForDepth(2)
		&& A->GetCaptureForDepth(0) != A->GetCaptureForDepth(2));
	FSceneViewStateInterface* State0 = A->GetCaptureForDepth(0)->GetViewState(0);
	FSceneViewStateInterface* State1 = A->GetCaptureForDepth(1)->GetViewState(0);
	FSceneViewStateInterface* State2 = A->GetCaptureForDepth(2)->GetViewState(0);
	TestNotNull(TEXT("Depth 0 owns a persistent ViewState"), State0);
	TestNotNull(TEXT("Depth 1 owns a persistent ViewState"), State1);
	TestNotNull(TEXT("Depth 2 owns a persistent ViewState"), State2);
	if (State0 && State1 && State2)
	{
		TestTrue(TEXT("Recursion ViewStates have distinct identities"),
			State0 != State1 && State1 != State2 && State0 != State2);
	}
	A->ResetCaptureHistory(1);
	TestTrue(TEXT("Resetting one recursion history requests a camera cut"),
		A->GetCaptureForDepth(1)->bCameraCutThisFrame);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalScreenBoundsTest,
	"SlayTheSpireDemo.Interior.Portals.ProjectedScreenBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalScreenBoundsTest::RunTest(const FString& Parameters)
{
	const FMatrix ViewPlanes(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FMatrix Projection = ViewPlanes * FReversedZPerspectiveMatrix(PI / 4, 1920, 1080, 1);
	const FIntRect ViewRect(0, 0, 1920, 1080);
	auto Project = [&Projection, &ViewRect](const FTransform& Frame, double HalfWidth, double HalfHeight,
		InteriorPortalMath::FPortalScreenBounds& OutBounds)
	{
		return InteriorPortalMath::ProjectPortalApertureToScreenBounds(Frame, HalfWidth, HalfHeight,
			Projection, ViewRect, OutBounds, true, 1.0);
	};

	InteriorPortalMath::FPortalScreenBounds Bounds;
	TestTrue(TEXT("Centered aperture projects to visible bounds"),
		Project(FTransform(FRotator::ZeroRotator, FVector(100, 0, 0)), 20, 20, Bounds));
	TestTrue(TEXT("Centered bounds are inside normalized viewport"),
		Bounds.Min.X >= 0 && Bounds.Min.Y >= 0 && Bounds.Max.X <= 1 && Bounds.Max.Y <= 1);

	TestTrue(TEXT("Partially offscreen aperture remains conservatively visible"),
		Project(FTransform(FRotator::ZeroRotator, FVector(100, 120, 0)), 30, 20, Bounds));
	TestTrue(TEXT("Partially offscreen bounds are clipped to the viewport"),
		Bounds.bClippedToViewport && FMath::IsNearlyEqual(Bounds.Max.X, 1.0f));

	TestTrue(TEXT("Near-edge aperture remains visible"),
		Project(FTransform(FRotator::ZeroRotator, FVector(100, 80, 0)), 20, 20, Bounds));
	TestTrue(TEXT("Near-edge bounds retain a positive visible area"),
		Bounds.Max.X > Bounds.Min.X && Bounds.Max.Y > Bounds.Min.Y);

	TestTrue(TEXT("Grazing-angle aperture remains finite and visible"),
		Project(FTransform(FRotator(0, 89, 0), FVector(100, 0, 0)), 25, 25, Bounds));
	TestTrue(TEXT("Grazing-angle bounds are finite"),
		FMath::IsFinite(Bounds.Min.X) && FMath::IsFinite(Bounds.Min.Y)
		&& FMath::IsFinite(Bounds.Max.X) && FMath::IsFinite(Bounds.Max.Y));

	TestFalse(TEXT("Aperture behind the camera is rejected"),
		Project(FTransform(FRotator::ZeroRotator, FVector(-100, 0, 0)), 20, 20, Bounds));
	TestTrue(TEXT("Behind-camera diagnostic is explicit"), Bounds.bEntirelyBehindCamera);

	TestFalse(TEXT("Aperture crossing the camera plane is not treated as a valid screen region"),
		Project(FTransform::Identity, 20, 20, Bounds));
	TestTrue(TEXT("Camera crossing diagnostic is explicit"), Bounds.bCameraCrossing);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AInteriorPortal* Portal = World->SpawnActor<AInteriorPortal>();
	Portal->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(100, 0, 0)));
	InteriorPortalMath::FPortalScreenBounds BoundsBeforeBias;
	TestTrue(TEXT("Logical portal frame supplies the screen-bound aperture"),
		Project(Portal->GetLogicalFrame(), Portal->HalfWidth, Portal->HalfHeight, BoundsBeforeBias));
	Portal->SurfaceVisualBias = 40.0f;
	Portal->RefreshAppearance();
	InteriorPortalMath::FPortalScreenBounds BoundsAfterBias;
	TestTrue(TEXT("Logical portal frame remains the screen-bound aperture after visual bias"),
		Project(Portal->GetLogicalFrame(), Portal->HalfWidth, Portal->HalfHeight, BoundsAfterBias));
	TestTrue(TEXT("Visual surface bias does not change screen-bound spatial contract"),
		BoundsBeforeBias.Min.Equals(BoundsAfterBias.Min, 0.001f)
		&& BoundsBeforeBias.Max.Equals(BoundsAfterBias.Max, 0.001f));
	const FTransform LogicalFrame = Portal->GetLogicalFrame();
	TestTrue(TEXT("Visual surface bias leaves the logical frame unchanged"),
		LogicalFrame.GetLocation().Equals(FVector(100, 0, 0), 0.001f));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalTravellerRegistryTest,
	"SlayTheSpireDemo.Interior.Portals.TravellerRegistry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalTravellerRegistryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	AInteriorPortalSystem* System = World->SpawnActor<AInteriorPortalSystem>();
	AActor* Owner = World->SpawnActor<AActor>();
	UBoxComponent* Body = NewObject<UBoxComponent>(Owner, TEXT("RuntimeTraveller"));
	Owner->SetRootComponent(Body);
	Body->SetBoxExtent(FVector(20, 12, 12));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->RegisterComponent();
	Body->SetSimulatePhysics(true);
	TestTrue(TEXT("A simulated primitive can register as a portal traveller"),
		System->RegisterPhysicsTraveller(Body));
	TestFalse(TEXT("Duplicate traveller registration is rejected"),
		System->RegisterPhysicsTraveller(Body));
	Body->ComponentTags.Add(TEXT("PortalTraveller"));
	TestTrue(TEXT("Runtime traveller unregister removes transient state"),
		System->UnregisterPhysicsTraveller(Body));
	TestFalse(TEXT("Unregistering an absent traveller is harmless"),
		System->UnregisterPhysicsTraveller(Body));
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
	// A real rim contact followed by a small external pose correction used to lock all
	// movement, including retreat. Exercise the movement component, not just the fit math.
	Movement->MoveUpdatedComponent(FVector(0,300,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Rim blocks lateral movement"),Hit.bBlockingHit);
	Pawn->SetActorLocation(Pawn->GetActorLocation()+FVector(0,.25,0),false,nullptr,ETeleportType::TeleportPhysics);
	const FVector CorrectedCenter=Pawn->GetActorLocation();
	Movement->MoveUpdatedComponent(FVector(8,0,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Slightly invalid footprint can retreat without a teleport"),
		Pawn->GetActorLocation().Equals(CorrectedCenter+FVector(8,0,0),.01));
	const FVector BeforeBlockedMove=Pawn->GetActorLocation();
	Movement->MoveUpdatedComponent(FVector(-8,1,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Invalid footprint cannot enter deeper into wall"),
		Pawn->GetActorLocation().Equals(BeforeBlockedMove,.01) && Hit.bBlockingHit);
	// Inverse rotation can leave a tiny normal residual in an otherwise tangent move.
	Movement->MoveUpdatedComponent(FVector(-1.e-12,-10,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Invalid footprint can move back towards aperture center"),
		FMath::IsNearlyEqual(Pawn->GetActorLocation().Y,BeforeBlockedMove.Y-10,.01));
	Movement->MoveUpdatedComponent(FVector(100,0,0),FQuat::Identity,true,&Hit);
	TestTrue(TEXT("Retreat releases support ignore"),Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().IsEmpty());
	TestEqual(TEXT("Retreat restores Outside state"),System->PlayerCrossingState,EInteriorPortalCrossingState::Outside);
	Pawn->SetActorLocation(FVector(80,0,100)); System->Tick(.016f);
	Movement->MoveUpdatedComponent(FVector(-70,0,0),FQuat::Identity,true,&Hit);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalRendererBackendTest,
	"SlayTheSpireDemo.Interior.Portals.RendererBackendContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalRendererBackendTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AInteriorPortalSystem* System = World->SpawnActor<AInteriorPortalSystem>();
	TestTrue(TEXT("SceneCapture is the explicit default renderer backend"),
		System->RendererBackend == EInteriorPortalRendererBackend::SceneCapture);
	TestTrue(TEXT("SceneCapture backend is identified independently"),
		AInteriorPortalSystem::UsesSceneCapture(EInteriorPortalRendererBackend::SceneCapture));
	TestFalse(TEXT("SceneCapture is not the MainView stencil backend"),
		AInteriorPortalSystem::UsesMainViewStencil(EInteriorPortalRendererBackend::SceneCapture));
	TestFalse(TEXT("SceneCapture is not the CustomRenderPass backend"),
		AInteriorPortalSystem::UsesCustomRenderPass(EInteriorPortalRendererBackend::SceneCapture));
	System->RendererBackend = EInteriorPortalRendererBackend::MainViewStencilSpike;
	TestTrue(TEXT("MainView stencil backend can be explicitly selected"),
		AInteriorPortalSystem::UsesMainViewStencil(System->RendererBackend));
	TestFalse(TEXT("MainView stencil selection does not use SceneCapture"),
		AInteriorPortalSystem::UsesSceneCapture(System->RendererBackend));
	System->RendererBackend = EInteriorPortalRendererBackend::CustomRenderPassSpike;
	TestTrue(TEXT("CustomRenderPass backend can be explicitly selected"),
		AInteriorPortalSystem::UsesCustomRenderPass(System->RendererBackend));
	TestFalse(TEXT("CustomRenderPass selection is not MainView stencil selection"),
		AInteriorPortalSystem::UsesMainViewStencil(System->RendererBackend));
	TestFalse(TEXT("CustomRenderPass selection does not use SceneCapture"),
		AInteriorPortalSystem::UsesSceneCapture(System->RendererBackend));
	System->RendererBackend = EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike;
	TestTrue(TEXT("CustomRenderPass composition spike can be explicitly selected"),
		AInteriorPortalSystem::UsesCustomRenderPass(System->RendererBackend));
	TestTrue(TEXT("Composition spike is distinguished from the separate CRP proof target"),
		AInteriorPortalSystem::UsesCustomRenderPassComposition(System->RendererBackend));
	TestFalse(TEXT("Composition spike is not SceneCapture"),
		AInteriorPortalSystem::UsesSceneCapture(System->RendererBackend));
	TestFalse(TEXT("Composition spike is not MainView stencil"),
		AInteriorPortalSystem::UsesMainViewStencil(System->RendererBackend));
	System->CaptureColorMode = EInteriorPortalCaptureColorMode::FinalColorHDR;
	TestTrue(TEXT("CaptureColorMode remains independently configurable"),
		AInteriorPortalSystem::GetCaptureSourceForColorMode(System->CaptureColorMode) == SCS_FinalColorHDR);
	TestTrue(TEXT("Changing renderer backend requires a history reset"),
		AInteriorPortalSystem::RequiresRendererHistoryReset(
			EInteriorPortalRendererBackend::SceneCapture,
			EInteriorPortalRendererBackend::MainViewStencilSpike));
	TestTrue(TEXT("Returning to SceneCapture also requires a history reset"),
		AInteriorPortalSystem::RequiresRendererHistoryReset(
			EInteriorPortalRendererBackend::MainViewStencilSpike,
			EInteriorPortalRendererBackend::SceneCapture));
	TestFalse(TEXT("Keeping one backend does not require a backend reset"),
		AInteriorPortalSystem::RequiresRendererHistoryReset(
			EInteriorPortalRendererBackend::SceneCapture,
			EInteriorPortalRendererBackend::SceneCapture));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalCompositionBoundaryContractTest,
	"SlayTheSpireDemo.Interior.Portals.CompositionBoundaryContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalCompositionBoundaryContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A valid one-layer request with an imported CRP target can enter the composition gate"),
		InteriorPortalRenderer::CanSubmitCustomRenderPassCompositionRequest(
			true, true, true, true, true, 1));
	TestFalse(TEXT("Composition gate rejects an absent CRP target identity"),
		InteriorPortalRenderer::CanSubmitCustomRenderPassCompositionRequest(
			true, true, true, true, false, 1));
	TestFalse(TEXT("Composition gate rejects an unlinked pair"),
		InteriorPortalRenderer::CanSubmitCustomRenderPassCompositionRequest(
			false, true, true, true, true, 1));
	TestFalse(TEXT("Composition gate rejects a portal outside the view"),
		InteriorPortalRenderer::CanSubmitCustomRenderPassCompositionRequest(
			true, false, true, true, true, 1));
	TestFalse(TEXT("Composition gate rejects recursion beyond the one-layer spike"),
		InteriorPortalRenderer::CanSubmitCustomRenderPassCompositionRequest(
			true, true, true, true, true, 2));
	TestTrue(TEXT("Composition backend switch invalidates renderer history"),
		AInteriorPortalSystem::RequiresRendererHistoryReset(
			EInteriorPortalRendererBackend::CustomRenderPassSpike,
			EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike));
	TestTrue(TEXT("Returning from composition to SceneCapture invalidates renderer history"),
		AInteriorPortalSystem::RequiresRendererHistoryReset(
			EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike,
			EInteriorPortalRendererBackend::SceneCapture));
	TestTrue(TEXT("The composition contract keeps the player as final exposure authority"),
		FInteriorPortalRenderRequest().bPlayerExposureAuthority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalCustomRenderPassContractTest,
	"SlayTheSpireDemo.Interior.Portals.CustomRenderPassContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalCustomRenderPassContractTest::RunTest(const FString& Parameters)
{
	FInteriorPortalCustomRenderPass Pass(TEXT("PortalCustomRenderPassTest"), nullptr, FIntPoint(320, 180));
	TestTrue(TEXT("Custom pass uses the UE 5.8 DepthAndBasePass mode"),
		Pass.GetRenderMode() == FCustomRenderPassBase::ERenderMode::DepthAndBasePass);
	TestTrue(TEXT("Custom pass requests scene color without alpha"),
		Pass.GetRenderOutput() == FCustomRenderPassBase::ERenderOutput::SceneColorNoAlpha);
	TestTrue(TEXT("Custom pass maps to SCS_SceneColorHDRNoAlpha"),
		Pass.GetSceneCaptureSource() == SCS_SceneColorHDRNoAlpha);
	TestTrue(TEXT("Custom pass includes translucency in its declared scene-color contract"),
		Pass.IsTranslucentIncluded());
	TestFalse(TEXT("A null target is rejected by the render-pass target contract"), Pass.HasRenderTarget());

	const FTransform PlayerView(FRotator(0.0f, 180.0f, 0.0f), FVector(200.0f, 20.0f, 80.0f));
	const FTransform EntryFrame(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 100.0f));
	const FTransform ExitFrame(FRotator::ZeroRotator, FVector(500.0f, 50.0f, 100.0f));
	const FMatrix PortalViewPlanes(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FMatrix Projection = FReversedZPerspectiveMatrix(PI / 4.0f, 1920.0f, 1080.0f, 1.0f);
	const FIntRect ViewRect(0, 0, 1920, 1080);
	const FMatrix ViewProjection = FTranslationMatrix(-PlayerView.GetLocation())
		* FInverseRotationMatrix(PlayerView.Rotator()) * PortalViewPlanes * Projection;
	FInteriorPortalRenderRequest Request;
	TestTrue(TEXT("Custom pass request reuses the portal virtual-view builder"),
		FInteriorPortalRenderRequest::Build(9, 0, 0, PlayerView, EntryFrame, ExitFrame,
			65.0, 115.0, ViewProjection, ViewRect, Projection, true, 1.0, 0.5, 4, Request));

	FSceneViewStateReference PortalViewState;
	PortalViewState.Allocate(ERHIFeatureLevel::SM6);
	FSceneViewStateInterface* State = PortalViewState.GetReference();
	TestNotNull(TEXT("Custom pass owns an independent portal ViewState identity"), State);
	if (State)
	{
		FSceneInterface::FCustomRenderPassRendererInput Input;
		TestTrue(TEXT("Immutable request maps to FCustomRenderPassRendererInput"),
			InteriorPortalRenderer::BuildCustomRenderPassInput(Request, State, &Pass, Input));
		TestTrue(TEXT("Renderer input uses the mapped virtual location"),
			Input.ViewLocation.Equals(Request.ViewLocation, 0.001f));
		TestTrue(TEXT("Renderer input uses the mapped virtual view rotation matrix"),
			Input.ViewRotationMatrix.Equals(Request.ViewRotationMatrix, 0.001f));
		TestTrue(TEXT("Renderer input uses the clip-encoded projection matrix"),
			Input.ProjectionMatrix.Equals(Request.ProjectionMatrix, 0.001f));
		TestTrue(TEXT("Renderer input keeps the portal ViewState separate from the player"),
			Input.ViewStateInterface == State);
		TestTrue(TEXT("Renderer input is a real main-renderer custom pass request"),
			!Input.bIsSceneCapture && Input.bUseMainViewFamilyShowFlags
			&& Input.CustomRenderPass == &Pass);
	}
	PortalViewState.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalVirtualViewRequestTest,
	"SlayTheSpireDemo.Interior.Portals.VirtualViewRequest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalVirtualViewRequestTest::RunTest(const FString& Parameters)
{
	const FTransform PlayerView(FRotator(17.0f, -31.0f, 8.0f), FVector(31.0f, -22.0f, 74.0f));
	const FTransform EntryFrame(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, 20.0f, 130.0f));
	const FTransform ExitFrame(FRotator(25.0f, 173.0f, 47.0f), FVector(1700.0f, -850.0f, 300.0f));
	const FTransform Expected(
		InteriorPortalMath::Rotation(EntryFrame, ExitFrame) * PlayerView.GetRotation(),
		InteriorPortalMath::Position(PlayerView.GetLocation(), EntryFrame, ExitFrame));
	const FTransform Actual = InteriorPortalMath::BuildVirtualViewTransform(PlayerView, EntryFrame, ExitFrame);
	TestTrue(TEXT("Virtual view location uses the existing portal position mapping"),
		Actual.GetLocation().Equals(Expected.GetLocation(), 0.001f));
	TestTrue(TEXT("Virtual view rotation uses the existing portal quaternion mapping"),
		Actual.GetRotation().Equals(Expected.GetRotation(), 0.001f));
	const FTransform RoundTrip = InteriorPortalMath::BuildVirtualViewTransform(Actual, ExitFrame, EntryFrame);
	TestTrue(TEXT("Virtual view mapping round trips through the paired logical frames"),
		RoundTrip.Equals(PlayerView, 0.001f));

	const FMatrix PortalViewPlanes(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FMatrix Projection = FReversedZPerspectiveMatrix(PI / 4.0f, 1920.0f, 1080.0f, 1.0f);
	const FIntRect ViewRect(0, 0, 1920, 1080);
	const FTransform SimplePlayer(FRotator(0.0f, 180.0f, 0.0f), FVector(200.0f, 0.0f, 100.0f));
	const FTransform SimpleEntry(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 100.0f));
	const FTransform SimpleExit(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 100.0f));
	const FTransform SimpleVirtual = InteriorPortalMath::BuildVirtualViewTransform(SimplePlayer, SimpleEntry, SimpleExit);
	const FMatrix SimplePlayerViewProjection = FTranslationMatrix(-SimplePlayer.GetLocation())
		* FInverseRotationMatrix(SimplePlayer.Rotator()) * PortalViewPlanes * Projection;
	FInteriorPortalRenderRequest Request;
	TestTrue(TEXT("A valid one-layer immutable render request is constructible"),
		FInteriorPortalRenderRequest::Build(7, 0, 0, SimplePlayer, SimpleEntry, SimpleExit,
			65.0, 115.0, SimplePlayerViewProjection, ViewRect, Projection, true, 1.0, 0.5, 12, Request));
	TestTrue(TEXT("Constructed request has a valid activation contract"), Request.IsValid());
	TestTrue(TEXT("Request copies the transformed virtual location"),
		Request.ViewLocation.Equals(Request.VirtualView.GetLocation(), 0.001f));
	TestTrue(TEXT("Request carries the transformed view rotation matrix"),
		Request.ViewRotationMatrix.Equals(
			FInverseRotationMatrix(Request.VirtualView.Rotator()) * PortalViewPlanes, 0.001f));
	TestTrue(TEXT("Request carries either the clip-encoded or unmodified player projection"),
		Request.bExitClipEncodedInProjection || Request.ProjectionMatrix.Equals(Projection, 0.001f));
	TestTrue(TEXT("Request history identity includes the renderer generation"),
		Request.HistoryIdentity == FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 0, 12));
	const FTransform SavedEntry = Request.EntryFrame;
	const FIntRect SavedScissor = Request.ScissorRect;
	const uint64 SavedHistoryIdentity = Request.HistoryIdentity;
	const FMatrix SavedProjection = Request.ProjectionMatrix;
	FTransform MutableEntry = SimpleEntry;
	MutableEntry.SetLocation(FVector(9000.0f, 9000.0f, 9000.0f));
	TestTrue(TEXT("Request retains copied logical transforms after source mutation"),
		Request.EntryFrame.Equals(SavedEntry, 0.001f));
	TestTrue(TEXT("Request retains copied scissor after source mutation"), Request.ScissorRect == SavedScissor);
	TestTrue(TEXT("Request retains copied history identity after source mutation"),
		Request.HistoryIdentity == SavedHistoryIdentity);
	TestTrue(TEXT("Request retains copied projection after source mutation"),
		Request.ProjectionMatrix.Equals(SavedProjection, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalScreenBoundsPixelRectTest,
	"SlayTheSpireDemo.Interior.Portals.ScreenBoundsPixelRect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalScreenBoundsPixelRectTest::RunTest(const FString& Parameters)
{
	InteriorPortalMath::FPortalScreenBounds Bounds;
	FIntRect PixelRect;
	Bounds.bHasVisiblePortion = true;
	Bounds.Min = FVector2D(0.25f, 0.25f);
	Bounds.Max = FVector2D(0.75f, 0.75f);
	TestTrue(TEXT("Centered normalized bounds convert to pixels"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect));
	TestTrue(TEXT("Centered pixel rect uses conservative floor/ceil"),
		PixelRect == FIntRect(480, 270, 1440, 810));

	Bounds.Min = FVector2D(0.0f, 0.2f); Bounds.Max = FVector2D(0.12f, 0.8f);
	TestTrue(TEXT("Partial left edge is retained"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect)
		&& PixelRect.Min.X == 0);
	Bounds.Min = FVector2D(0.88f, 0.2f); Bounds.Max = FVector2D(1.0f, 0.8f);
	TestTrue(TEXT("Partial right edge is retained"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect)
		&& PixelRect.Max.X == 1920);
	Bounds.Min = FVector2D(0.2f, 0.0f); Bounds.Max = FVector2D(0.8f, 0.03f);
	TestTrue(TEXT("Partial top edge is retained"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect)
		&& PixelRect.Min.Y == 0);
	Bounds.Min = FVector2D(0.2f, 0.97f); Bounds.Max = FVector2D(0.8f, 1.0f);
	TestTrue(TEXT("Partial bottom edge is retained"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect)
		&& PixelRect.Max.Y == 1080);
	Bounds.Min = FVector2D(0.9994f, 0.9994f); Bounds.Max = FVector2D(0.9999f, 0.9999f);
	TestTrue(TEXT("Tiny visible portion expands to at least one conservative pixel"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect)
		&& PixelRect.Width() >= 1 && PixelRect.Height() >= 1);
	Bounds.Min = FVector2D(0.25f, 0.25f); Bounds.Max = FVector2D(0.75f, 0.75f);
	TestTrue(TEXT("Non-zero view rect origins are preserved"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(100, 50, 1700, 950), PixelRect));
	TestTrue(TEXT("Non-zero view rect conversion does not reset to zero origin"),
		PixelRect == FIntRect(500, 275, 1300, 725));
	Bounds.bHasVisiblePortion = false;
	TestFalse(TEXT("Fully outside aperture produces an invalid pixel rect"),
		InteriorPortalMath::ScreenBoundsToPixelRect(Bounds, FIntRect(0, 0, 1920, 1080), PixelRect));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalClipPlaneContractTest,
	"SlayTheSpireDemo.Interior.Portals.ExitClipPlaneContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalClipPlaneContractTest::RunTest(const FString& Parameters)
{
	for (const FTransform& ExitFrame : {
		FTransform(FRotator::ZeroRotator, FVector(500.0f, 20.0f, 100.0f)),
		FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(900.0f, -40.0f, 210.0f)),
		FTransform(FRotator(35.0f, 20.0f, 10.0f), FVector(-300.0f, 700.0f, 80.0f))})
	{
		FPlane ClipPlane;
		TestTrue(TEXT("Logical exit frame produces a clip plane"),
			InteriorPortalMath::BuildPortalClipPlane(ExitFrame, 0.5, ClipPlane));
		const FVector Normal = ExitFrame.GetUnitAxis(EAxis::X).GetSafeNormal();
		const FVector PlanePoint = ExitFrame.GetLocation() + Normal * 0.5f;
		TestTrue(TEXT("Exit clip plane contains its biased logical point"),
			FMath::Abs(ClipPlane.PlaneDot(PlanePoint)) < 0.01f);
		TestTrue(TEXT("Destination-side point is retained by the plane contract"),
			ClipPlane.PlaneDot(PlanePoint + Normal * 10.0f) > 0.0f);
		TestTrue(TEXT("Camera-side point is rejected by the plane contract"),
			ClipPlane.PlaneDot(PlanePoint - Normal * 10.0f) < 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalSpikeActivationTest,
	"SlayTheSpireDemo.Interior.Portals.SpikeActivationGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalSpikeActivationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Valid linked visible one-layer request is allowed"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(true, true, true, true, 1));
	TestFalse(TEXT("Unlinked pair cannot submit a request"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(false, true, true, true, 1));
	TestFalse(TEXT("Behind-camera portal cannot submit a request"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(true, false, true, true, 1));
	TestFalse(TEXT("Invalid projected rect cannot submit a request"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(true, true, false, true, 1));
	TestFalse(TEXT("Invalid virtual view cannot submit a request"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(true, true, true, false, 1));
	TestFalse(TEXT("Recursion depth above the spike contract cannot submit a request"),
		InteriorPortalRenderer::CanSubmitMainViewStencilRequest(true, true, true, true, 2));
	TestTrue(TEXT("Endpoint and recursion identities do not collide"),
		FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 0, 3)
		!= FInteriorPortalRenderRequest::MakeHistoryIdentity(1, 0, 3)
		&& FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 0, 3)
		!= FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 1, 3)
		&& FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 0, 3)
		!= FInteriorPortalRenderRequest::MakeHistoryIdentity(0, 0, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPortalViewExtensionLifecycleTest,
	"SlayTheSpireDemo.Interior.Portals.ViewExtensionLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteriorPortalViewExtensionLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> Extension =
		FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
	TestFalse(TEXT("ViewExtension starts disabled"), Extension->IsEnabled());
	Extension->SetEnabled(true);
	TestTrue(TEXT("ViewExtension can be enabled"), Extension->IsEnabled());
	FInteriorPortalRenderRequest Request;
	Request.PortalId = 1;
	Request.EndpointIndex = 0;
	Request.HistoryIdentity = 77;
	Request.bEnabled = true;
	Extension->PublishRequest(Request);
	TestTrue(TEXT("ViewExtension receives an immutable request copy"), Extension->HasPublishedRequest());
	TestEqual(TEXT("Published request keeps its history identity"),
		Extension->GetPublishedRequest().HistoryIdentity, uint64(77));
	Extension->ClearRequest();
	TestFalse(TEXT("ViewExtension clear removes stale request state"), Extension->HasPublishedRequest());
	Extension->SetEnabled(false);
	TestFalse(TEXT("Disabling the extension clears request state"), Extension->HasPublishedRequest());
	Extension.Reset();
	World->DestroyWorld(false);
	return true;
}
#endif
