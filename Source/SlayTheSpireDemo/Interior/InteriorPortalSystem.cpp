#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPlayerController.h"
#include "InteriorPortalMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneView.h"

namespace
{
	FVector EyeOf(const ACharacter* Pawn)
	{
		const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
		// CharacterMovement can defer child component transforms inside a scoped move.
		// The eye must follow the current capsule, not a stale cached camera transform.
		return Camera && Camera->GetAttachParent()==Pawn->GetCapsuleComponent()
			? Pawn->GetActorTransform().TransformPosition(Camera->GetRelativeLocation()) : Pawn->GetPawnViewLocation();
	}
	bool BodyFits(const UPrimitiveComponent* Body, const AInteriorPortal* Portal)
	{
		const FVector Local = Portal->GetLogicalFrame().InverseTransformPositionNoScale(Body->GetComponentLocation());
		// Bounding sphere is conservative for arbitrary rigid bodies.
		const float Radius = Body->Bounds.SphereRadius;
		return InteriorPortalMath::Inside(Local, Portal->HalfWidth, Portal->HalfHeight, Radius, Radius);
	}
}

AInteriorPortalSystem::AInteriorPortalSystem()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PortalSystemRoot"));
	GrabHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PortalCubeHandle"));
	// Only active play needs pre-movement collision preparation, never editor ticking.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void AInteriorPortalSystem::BeginPlay()
{
	Super::BeginPlay();
	if (!IsValid(BluePortal) || !IsValid(OrangePortal) || BluePortal == OrangePortal)
	{
		UE_LOG(LogTemp, Error, TEXT("InteriorPortalSystem requires two distinct explicit endpoints."));
		return;
	}
	BluePortal->PortalColor = FLinearColor(0.01f, 0.25f, 1.0f);
	OrangePortal->PortalColor = FLinearColor(1.0f, 0.15f, 0.008f);
	BluePortal->RefreshAppearance();
	OrangePortal->RefreshAppearance();
	PreviousBodyPositions.SetNum(PhysicsTravellers.Num());
	BodyExits.SetNum(PhysicsTravellers.Num());
	PassageConstraints.SetNum(PhysicsTravellers.Num());
	BodyProxies.SetNum(PhysicsTravellers.Num());
	BodyMaterials.SetNum(PhysicsTravellers.Num());
	ProxyMaterials.SetNum(PhysicsTravellers.Num());
	for (int32 I = 0; I < PhysicsTravellers.Num(); ++I)
	{
		if (IsValid(PhysicsTravellers[I])) { PreviousBodyPositions[I] = PhysicsTravellers[I]->GetComponentLocation(); }
		if (UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(PhysicsTravellers[I]))
		{
			UStaticMeshComponent* Proxy = NewObject<UStaticMeshComponent>(this);
			Proxy->SetStaticMesh(Body->GetStaticMesh());
			Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Proxy->SetCanEverAffectNavigation(false);
			Proxy->SetVisibility(false);
			Proxy->RegisterComponent();
			ProxyMaterials[I] = Proxy->CreateDynamicMaterialInstance(0, Body->GetMaterial(0));
			BodyMaterials[I] = Body->CreateDynamicMaterialInstance(0);
			BodyProxies[I] = Proxy;
		}
	}
	UpdateFidelityDiagnostics();
	SetActorTickEnabled(true);
}

bool AInteriorPortalSystem::IsLinked() const
{
	return IsValid(BluePortal) && IsValid(OrangePortal) && BluePortal != OrangePortal
		&& BluePortal->bPlaced && OrangePortal->bPlaced;
}

bool AInteriorPortalSystem::IsFlashlightTraceThroughPortal(const FHitResult& Hit,
	const FVector& TraceStart, const FVector& TraceEnd, const float TraceRadius) const
{
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (!HitComponent || TraceRadius < 0.0f) { return false; }

	for (const AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()})
	{
		if (!IsValid(Portal) || !Portal->bPlaced || !IsValid(Portal->Support)
			|| Portal->Support.Get() != HitComponent) { continue; }

		const FTransform Frame = Portal->GetLogicalFrame();
		const FVector Start = Frame.InverseTransformPositionNoScale(TraceStart);
		const FVector End = Frame.InverseTransformPositionNoScale(TraceEnd);
		const FVector Delta = End - Start;

		// The sweep must approach the portal plane from one of its two sides.
		// This rejects a wall hit that happens to use the same support component
		// while the flashlight is pointing away from the portal.
		const bool bApproachesPlane = (Start.X >= 0.0f && Delta.X < 0.0f)
			|| (Start.X <= 0.0f && Delta.X > 0.0f);
		if (!bApproachesPlane) { continue; }

		float PlaneTime = 0.0f;
		if (!FMath::IsNearlyZero(Delta.X))
		{
			PlaneTime = FMath::Clamp(-Start.X / Delta.X, 0.0f, 1.0f);
		}
		const FVector NearestToPlane = Start + Delta * PlaneTime;
		if (FMath::Abs(NearestToPlane.X) > TraceRadius + 1.0f) { continue; }

		// Expand the aperture by the sweep radius so the spherical clearance
		// query follows the visible opening instead of the solid support wall.
		const float Width = Portal->HalfWidth + TraceRadius;
		const float Height = Portal->HalfHeight + TraceRadius;
		if (Width > 0.0f && Height > 0.0f
			&& FMath::Square(NearestToPlane.Y / Width) + FMath::Square(NearestToPlane.Z / Height) <= 1.0f)
		{
			return true;
		}
	}
	return false;
}

bool AInteriorPortalSystem::FitsCharacter(const ACharacter* Pawn, const FVector& Center, const AInteriorPortal* Portal) const
{
	const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
	const float R = Capsule->GetScaledCapsuleRadius() + 1;
	const float Segment = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
	const FTransform Frame = Portal->GetLogicalFrame();
	const FVector Local = Frame.InverseTransformPositionNoScale(Center);
	const FVector Spine = Frame.InverseTransformVectorNoScale(FVector::UpVector * Segment);
	double Enter, Leave; FVector Normal;
	return InteriorPortalMath::CapsuleApertureInterval(Local,FVector::ZeroVector,Spine,R,
		Portal->HalfWidth*.94,Portal->HalfHeight*.94,Enter,Leave,Normal);
}

double AInteriorPortalSystem::CharacterNormalExtent(const ACharacter* Pawn, const FTransform& Frame) const
{
	const UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();
	return Capsule->GetScaledCapsuleRadius() + FMath::Abs(Frame.GetUnitAxis(EAxis::X).Z)
		*(Capsule->GetScaledCapsuleHalfHeight()-Capsule->GetScaledCapsuleRadius());
}

bool AInteriorPortalSystem::IsPlayerClearingPortal() const
{
	return PlayerCrossingState==EInteriorPortalCrossingState::Transferred
		|| PlayerCrossingState==EInteriorPortalCrossingState::ClearingExit;
}

void AInteriorPortalSystem::RecoverCharacterPassage()
{
	if (Character.IsValid() && (PlayerGate.IsValid() || !IgnoredSupports.IsEmpty()))
	{
		ACharacter* Pawn=Character.Get();
		const FVector Normal=PlayerGateFrame.GetUnitAxis(EAxis::X);
		const double Distance=FVector::DotProduct(Pawn->GetActorLocation()-PlayerGateFrame.GetLocation(),Normal);
		const FVector Candidate=Pawn->GetActorLocation()+Normal*FMath::Max(0.0,CharacterNormalExtent(Pawn,PlayerGateFrame)+2-Distance);
		const UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalRestore),false,Pawn);
		const bool bBlocked=GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
		if (!bBlocked || bHasSafePlayerCenter)
		{
			Pawn->SetActorLocation(bBlocked?LastSafePlayerCenter:Candidate,false,nullptr,ETeleportType::TeleportPhysics);
			Pawn->GetCharacterMovement()->Velocity=FVector::VectorPlaneProject(Pawn->GetVelocity(),Normal);
		}
	}
	RestoreIgnores(); PlayerGate.Reset(); LastPlayerExit.Reset();
	PlayerCrossingState=EInteriorPortalCrossingState::Outside;
	bHasPreviousEye=false;
}

double AInteriorPortalSystem::ConstrainCharacterMove(ACharacter* Pawn,const FVector& Delta,FHitResult& OutGateHit)
{
	if (!Pawn || Character.Get()!=Pawn) { return 1; }
	if (!IsLinked()) { RecoverCharacterPassage(); return 1; }
	if (!PlayerGate.IsValid() && !IgnoredSupports.IsEmpty()) { RecoverCharacterPassage(); }
	if (PlayerGate.IsValid() && (!PlayerGate->GetLogicalFrame().Equals(PlayerGateFrame,.001)
		|| !IsValid(PlayerGate->Support))) { RecoverCharacterPassage(); }
	const FVector Center=Pawn->GetActorLocation();
	const UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();
	const double Radius=Capsule->GetScaledCapsuleRadius()+1;
	const double Segment=Capsule->GetScaledCapsuleHalfHeight()-Capsule->GetScaledCapsuleRadius();
	AInteriorPortal* Gate=PlayerGate.Get();
	if (!Gate)
	{
		for (AInteriorPortal* Candidate : {BluePortal.Get(),OrangePortal.Get()})
		{
			if (!IsValid(Candidate->Support)) { continue; }
			const FTransform Frame=Candidate->GetLogicalFrame();
			const FVector Start=Frame.InverseTransformPositionNoScale(Center);
			const FVector Move=Frame.InverseTransformVectorNoScale(Delta);
			const double Reach=CharacterNormalExtent(Pawn,Frame)+2;
			if (Start.X < 0 || (Start.X>Reach && (Move.X>=0 || Start.X+Move.X>Reach))) { continue; }
			double Enter,Leave; FVector N;
			if (!InteriorPortalMath::CapsuleApertureInterval(Start,Move,Frame.InverseTransformVectorNoScale(FVector::UpVector*Segment),
				Radius,Candidate->HalfWidth*.94,Candidate->HalfHeight*.94,Enter,Leave,N)) { continue; }
			const double Contact=Start.X<=Reach?0:(Reach-Start.X)/Move.X;
			if (Enter>Contact || Leave<Contact) { continue; }
			Gate=Candidate; PlayerGate=Gate; PlayerGateFrame=Frame;
			PlayerCrossingState=EInteriorPortalCrossingState::ApproachingEntry;
			break;
		}
	}
	if (!Gate) { return 1; }
	const FTransform Frame=Gate->GetLogicalFrame();
	const FVector Local=Frame.InverseTransformPositionNoScale(Center);
	const FVector Move=Frame.InverseTransformVectorNoScale(Delta);
	double Enter,Leave; FVector N;
	const bool bFits=InteriorPortalMath::CapsuleApertureInterval(Local,Move,Frame.InverseTransformVectorNoScale(FVector::UpVector*Segment),
		Radius,Gate->HalfWidth*.94,Gate->HalfHeight*.94,Enter,Leave,N);
	double Fraction=bFits?FMath::Clamp(Leave,0.0,1.0):0;
	FVector HitNormal=Frame.TransformVectorNoScale(N);
	const double Clearance=CharacterNormalExtent(Pawn,Frame)+2;
	// Once the entire capsule is in front of the wall, lateral movement is ordinary room movement.
	if (bFits && Move.X>0 && (Clearance-Local.X)/Move.X<=Fraction) { Fraction=1; }
	if (!bFits) { HitNormal=-Delta.GetSafeNormal(); }
	if ((IsPlayerClearingPortal() || LastPlayerTransferFrame==GFrameCounter) && Move.X<0)
	{
		const double EyeX=Frame.InverseTransformPositionNoScale(EyeOf(Pawn)).X;
		const double Stop=FMath::Clamp((.05-EyeX)/Move.X,0.0,1.0);
		if (Stop<Fraction) { Fraction=Stop; HitNormal=Frame.GetUnitAxis(EAxis::X); }
	}
	if (Fraction<1)
	{
		// Leave a geometric skin, independent of requested speed or frame duration.
		Fraction=FMath::Max(0.0,Fraction-.01/FMath::Max(Delta.Size(),.01));
		OutGateHit=FHitResult(Gate->Support->GetOwner(),Gate->Support,Center+Delta*Fraction,HitNormal);
		OutGateHit.bBlockingHit=true; OutGateHit.Time=Fraction;
		OutGateHit.Location=Center+Delta*Fraction;
		OutGateHit.TraceStart=Center; OutGateHit.TraceEnd=Center+Delta;
	}
	Pawn->GetCapsuleComponent()->IgnoreComponentWhenMoving(Gate->Support,true);
	IgnoredSupports.AddUnique(Gate->Support);
	return Fraction;
}

void AInteriorPortalSystem::FinishCharacterMove(ACharacter* Pawn)
{
	if (!Pawn || Character.Get()!=Pawn) { return; }
	if (AInteriorPortal* Gate=PlayerGate.Get())
	{
		const double Distance=PlayerGateFrame.InverseTransformPositionNoScale(Pawn->GetActorLocation()).X;
		if (Distance>CharacterNormalExtent(Pawn,PlayerGateFrame)+2)
		{
			RestoreIgnores(); PlayerGate.Reset(); LastPlayerExit.Reset();
			PlayerCrossingState=EInteriorPortalCrossingState::Outside;
		}
		else if (IsPlayerClearingPortal()) { PlayerCrossingState=EInteriorPortalCrossingState::ClearingExit; }
		else { PlayerCrossingState=EInteriorPortalCrossingState::IntersectingAperture; }
	}
	if (!PlayerGate.IsValid()) { LastSafePlayerCenter=Pawn->GetActorLocation(); bHasSafePlayerCenter=true; }
}

void AInteriorPortalSystem::RestoreIgnores()
{
	if (Character.IsValid())
	{
		for (const auto& Support : IgnoredSupports)
		{
			if (Support.IsValid()) { Character->GetCapsuleComponent()->IgnoreComponentWhenMoving(Support.Get(), false); }
		}
	}
	IgnoredSupports.Reset();
}

void AInteriorPortalSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFidelityDiagnostics();
	ACharacter* Pawn = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Character.Get() != Pawn)
	{
		RestoreIgnores();
		Character = Pawn;
		bHasPreviousEye = false;
		LastPlayerExit.Reset();
		PlayerGate.Reset(); bHasSafePlayerCenter=false;
		PlayerCrossingState=EInteriorPortalCrossingState::Outside;
		if (Pawn) { Pawn->GetCharacterMovement()->AddTickPrerequisiteActor(this); }
	}
	if (!IsLinked() || (!PlayerGate.IsValid() && !IgnoredSupports.IsEmpty())
		|| (PlayerGate.IsValid() && (!IsValid(PlayerGate->Support) || !PlayerGate->GetLogicalFrame().Equals(PlayerGateFrame,.001))))
	{ RecoverCharacterPassage(); }
	if (Pawn && IsLinked())
	{
		FinishCharacterMove(Pawn);
		if (!bHasPreviousEye) { PreviousEye = EyeOf(Pawn); bHasPreviousEye = true; }
	}
	UpdatePhysicsGates();
	if (Pawn && GrabHandle->GetGrabbedComponent())
	{
		const APlayerController* Player = Cast<APlayerController>(Pawn->GetController());
		if (Player)
		{
			FVector Eye = EyeOf(Pawn);
			FVector Desired = Eye + Player->GetControlRotation().Vector()*125;
			FRotator TargetRotation = Player->GetControlRotation();
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalGrab), false, Pawn);
			Params.AddIgnoredComponent(GrabHandle->GetGrabbedComponent());
			if (IsLinked())
			{
				if (HeldThroughEntry.IsValid())
				{
					AInteriorPortal* Entry = HeldThroughEntry.Get();
					AInteriorPortal* Exit = Entry==BluePortal ? OrangePortal : BluePortal;
					const FTransform EntryFrame = Entry->GetLogicalFrame();
					const FTransform ExitFrame = Exit->GetLogicalFrame();
					Eye = InteriorPortalMath::Position(Eye, EntryFrame, ExitFrame);
					Desired = InteriorPortalMath::Position(Desired, EntryFrame, ExitFrame);
					TargetRotation = (InteriorPortalMath::Rotation(EntryFrame, ExitFrame)*TargetRotation.Quaternion()).Rotator();
					if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
				}
				else
				{
					for (AInteriorPortal* Entry : {BluePortal.Get(),OrangePortal.Get()})
					{
						FVector Intersection;
						if (Entry->Support && InteriorPortalMath::Crossed(Eye,Desired,Entry->GetLogicalFrame(),Entry->HalfWidth-30,Entry->HalfHeight-30,Intersection))
						{ Params.AddIgnoredComponent(Entry->Support.Get()); }
					}
				}
			}
			FHitResult Hit;
			const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Eye, Desired, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(28), Params);
			GrabHandle->SetTargetLocationAndRotation(bBlocked ? Hit.Location : Desired, TargetRotation);
		}
	}
}

void AInteriorPortalSystem::UpdatePhysicsGates()
{
	for (int32 I = 0; I < PreviousBodyPositions.Num(); ++I)
	{
		UPrimitiveComponent* Body = PhysicsTravellers[I];
		UPrimitiveComponent* Support = nullptr;
		if (IsValid(Body) && Body->IsSimulatingPhysics() && IsLinked())
		{
			for (AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()})
			{
				const FTransform Frame = Portal->GetLogicalFrame();
				const FVector Normal = Frame.GetUnitAxis(EAxis::X);
				const double Distance = FVector::DotProduct(Body->GetComponentLocation() - Frame.GetLocation(), Normal);
				const double Approach = Body->Bounds.SphereRadius + 20 + Body->GetPhysicsLinearVelocity().Size() * FMath::Min(GetWorld()->GetDeltaSeconds(), .1f);
				if (FMath::Abs(Distance) < Approach && BodyFits(Body, Portal))
				{
					Support = Portal->Support;
					break;
				}
			}
			if (BodyExits[I].IsValid())
			{
				AInteriorPortal* Exit = BodyExits[I].Get();
				const FTransform ExitFrame = Exit->GetLogicalFrame();
				const double D = FVector::DotProduct(Body->GetComponentLocation()-ExitFrame.GetLocation(), ExitFrame.GetUnitAxis(EAxis::X));
				if (FMath::Abs(D) < Body->Bounds.SphereRadius+20 && BodyFits(Body, Exit)) { Support=Exit->Support; }
				else { BodyExits[I].Reset(); }
			}
		}
		UPhysicsConstraintComponent* Constraint = PassageConstraints[I];
		if (Constraint && (!Support || Constraint->OverrideComponent1.Get() != Support))
		{
			Constraint->DestroyComponent();
			PassageConstraints[I] = nullptr;
		}
		if (Support && !PassageConstraints[I])
		{
			// A completely free joint suppresses ONLY this body's contact with its portal wall.
			// Other bodies, the floor, and all furniture keep their normal Chaos collision.
			Constraint = NewObject<UPhysicsConstraintComponent>(this);
			Constraint->SetLinearXLimit(LCM_Free, 0);
			Constraint->SetLinearYLimit(LCM_Free, 0);
			Constraint->SetLinearZLimit(LCM_Free, 0);
			Constraint->SetAngularSwing1Limit(ACM_Free, 0);
			Constraint->SetAngularSwing2Limit(ACM_Free, 0);
			Constraint->SetAngularTwistLimit(ACM_Free, 0);
			Constraint->SetDisableCollision(true);
			Constraint->RegisterComponent();
			Constraint->SetConstrainedComponents(Support, NAME_None, Body, NAME_None);
			PassageConstraints[I] = Constraint;
		}
	}
}

void AInteriorPortalSystem::UpdateCharacterTraversal(APlayerController* Player)
{
	if (!IsLinked() || !Player) { RecoverCharacterPassage(); bHasPreviousEye = false; return; }
	ACharacter* Pawn = Cast<ACharacter>(Player->GetPawn());
	if (Pawn && Character.Get() == Pawn)
	{
		FVector Eye = EyeOf(Pawn);
		if (bHasPreviousEye && !IsPlayerClearingPortal() && LastPlayerTransferFrame!=GFrameCounter)
		{
			for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
			{
				if (PlayerGate.Get()!=Entry) { continue; }
				const FTransform EntryFrame = Entry->GetLogicalFrame();
				FVector Intersection;
				if (!InteriorPortalMath::Crossed(PreviousEye, Eye, EntryFrame, Entry->HalfWidth*.94, Entry->HalfHeight*.94, Intersection)) { continue; }
				const FVector CenterAtPlane = Pawn->GetActorLocation() + Intersection - Eye;
				if (!FitsCharacter(Pawn, CenterAtPlane, Entry)) { continue; }
				AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
				const FTransform ExitFrame = Exit->GetLogicalFrame();
				const FQuat Rotation = InteriorPortalMath::Rotation(EntryFrame, ExitFrame);
				const FVector Velocity = Rotation.RotateVector(Pawn->GetCharacterMovement()->Velocity);
				AInteriorPlayerController* InteriorPlayer=Cast<AInteriorPlayerController>(Player);
				const FRotator View = (Rotation * (InteriorPlayer?InteriorPlayer->GetPortalView():Player->GetControlRotation().Quaternion())).Rotator();
				const FVector NewEye = InteriorPortalMath::Position(Eye, EntryFrame, ExitFrame);
				const FVector EyeOffset = Eye - Pawn->GetActorLocation();
				const FVector Location = NewEye - EyeOffset;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalExit), false, Pawn);
				if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
				const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
				const FVector ExitPlaneCenter=InteriorPortalMath::Position(Intersection,EntryFrame,ExitFrame)-EyeOffset;
				FHitResult ExitSweep;
				const bool bExitFits=FitsCharacter(Pawn,ExitPlaneCenter,Exit);
				const bool bExitSweepBlocked=GetWorld()->SweepSingleByChannel(ExitSweep,ExitPlaneCenter,Location,FQuat::Identity,ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
				const bool bExitOverlap=GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
				if (!bExitFits || bExitSweepBlocked || bExitOverlap)
				{
					if (PlacementMessage!=TEXT("Exit blocked"))
					{
						UE_LOG(LogTemp,Display,TEXT("Portal exit rejected: fit=%d sweep=%d overlap=%d component=%s from=%s to=%s"),
							bExitFits,bExitSweepBlocked,bExitOverlap,*GetNameSafe(ExitSweep.GetComponent()),*ExitPlaneCenter.ToString(),*Location.ToString());
					}
					Pawn->SetActorLocation(Pawn->GetActorLocation() + Intersection - Eye + EntryFrame.GetUnitAxis(EAxis::X) * 2, false);
					Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
					PlacementMessage = TEXT("Exit blocked");
					break;
				}
				RestoreIgnores();
				if (Exit->Support)
				{
					Pawn->GetCapsuleComponent()->IgnoreComponentWhenMoving(Exit->Support, true);
					IgnoredSupports.Add(Exit->Support);
				}
				Pawn->SetActorLocationAndRotation(Location, FRotator(0, View.Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
				if (UPrimitiveComponent* Held = GrabHandle->GetGrabbedComponent())
				{
					if (HeldThroughEntry.Get()==Entry) { HeldThroughEntry.Reset(); }
					else
					{
						GrabHandle->ReleaseComponent();
						const FVector HeldVelocity = Rotation.RotateVector(Held->GetPhysicsLinearVelocity());
						const FVector HeldSpin = Rotation.RotateVector(Held->GetPhysicsAngularVelocityInRadians());
						Held->SetWorldLocationAndRotation(InteriorPortalMath::Position(Held->GetComponentLocation(), EntryFrame, ExitFrame), Rotation*Held->GetComponentQuat(), false, nullptr, ETeleportType::TeleportPhysics);
						Held->SetPhysicsLinearVelocity(HeldVelocity);
						Held->SetPhysicsAngularVelocityInRadians(HeldSpin);
						const int32 HeldIndex = PhysicsTravellers.IndexOfByKey(Held);
						if (PreviousBodyPositions.IsValidIndex(HeldIndex)) { PreviousBodyPositions[HeldIndex]=Held->GetComponentLocation(); BodyExits[HeldIndex]=Exit; }
						GrabHandle->GrabComponentAtLocationWithRotation(Held,NAME_None,Held->GetComponentLocation(),Held->GetComponentRotation());
					}
				}
				if (InteriorPlayer) { InteriorPlayer->ApplyPortalView(Rotation); }
				else { Player->SetControlRotation(View); }
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				Pawn->GetCharacterMovement()->Velocity = Velocity;
				// Do not infer a velocity from the discontinuous world-space location delta.
				Pawn->GetCharacterMovement()->bJustTeleported = true;
				if (UInteriorPortalMovementComponent* Movement=Cast<UInteriorPortalMovementComponent>(Pawn->GetCharacterMovement()))
				{ Movement->MapPortalAcceleration(Rotation); }
				LastPlayerExit = Exit;
				PlayerGate=Exit; PlayerGateFrame=ExitFrame;
				PlayerCrossingState=EInteriorPortalCrossingState::Transferred;
				LastPlayerTransferFrame=GFrameCounter;
				++PlayerCrossings;
				break;
			}
		}
		PreviousEye = EyeOf(Pawn);
		bHasPreviousEye = true;
	}
}

void AInteriorPortalSystem::UpdateTraversal(APlayerController* Player)
{
	UpdateCharacterTraversal(Player);
	if (!IsLinked() || !Player) { return; }
	for (int32 I = 0; I < PreviousBodyPositions.Num(); ++I)
	{
		UPrimitiveComponent* Body = PhysicsTravellers[I];
		if (!IsValid(Body) || !Body->IsSimulatingPhysics()) { continue; }
		for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
		{
			const FTransform EntryFrame = Entry->GetLogicalFrame();
			FVector Intersection;
			if (!InteriorPortalMath::Crossed(PreviousBodyPositions[I], Body->GetComponentLocation(), EntryFrame,
				Entry->HalfWidth, Entry->HalfHeight, Intersection) || !BodyFits(Body, Entry)) { continue; }
			AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
			const FTransform ExitFrame = Exit->GetLogicalFrame();
			const FQuat Rotation = InteriorPortalMath::Rotation(EntryFrame, ExitFrame);
			const FVector Velocity = Rotation.RotateVector(Body->GetPhysicsLinearVelocity());
			const FVector Spin = Rotation.RotateVector(Body->GetPhysicsAngularVelocityInRadians());
			const FVector Destination = InteriorPortalMath::Position(Body->GetComponentLocation(), EntryFrame, ExitFrame);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalPhysicsExit), false, Body->GetOwner());
			if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
			if (GetWorld()->OverlapBlockingTestByChannel(Destination, FQuat::Identity, Body->GetCollisionObjectType(), FCollisionShape::MakeSphere(Body->Bounds.SphereRadius*.6f),Params))
			{
				Body->SetWorldLocation(Intersection+EntryFrame.GetUnitAxis(EAxis::X)*(Body->Bounds.SphereRadius+1),false,nullptr,ETeleportType::TeleportPhysics);
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				break;
			}
			if (PassageConstraints[I]) { PassageConstraints[I]->DestroyComponent(); PassageConstraints[I] = nullptr; }
			const bool bHeld = GrabHandle->GetGrabbedComponent()==Body;
			if (bHeld) { GrabHandle->ReleaseComponent(); }
			Body->SetWorldLocationAndRotation(Destination,
				Rotation * Body->GetComponentQuat(), false, nullptr, ETeleportType::TeleportPhysics);
			Body->SetPhysicsLinearVelocity(Velocity);
			Body->SetPhysicsAngularVelocityInRadians(Spin);
			if (bHeld)
			{
				if (HeldThroughEntry.IsValid()) { HeldThroughEntry.Reset(); }
				else { HeldThroughEntry=Entry; }
				GrabHandle->GrabComponentAtLocationWithRotation(Body,NAME_None,Body->GetComponentLocation(),Body->GetComponentRotation());
			}
			BodyExits[I] = Exit;
			++PhysicsCrossings;
			break;
		}
		PreviousBodyPositions[I] = Body->GetComponentLocation();
	}
	UpdatePhysicsGates();
	UpdateBodyVisuals();
}

bool AInteriorPortalSystem::TryGrab(APlayerController* Player)
{
	if (!Player) { return false; }
	if (GrabHandle->GetGrabbedComponent()) { GrabHandle->ReleaseComponent(); HeldThroughEntry.Reset(); return true; }
	FVector Eye; FRotator View; Player->GetPlayerViewPoint(Eye,View);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalPickup),false,Player->GetPawn());
	if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*220,ECC_Visibility,Params)
		&& PhysicsTravellers.Contains(Hit.GetComponent()) && Hit.GetComponent()->IsSimulatingPhysics())
	{
		GrabHandle->GrabComponentAtLocationWithRotation(Hit.GetComponent(),NAME_None,Hit.GetComponent()->GetComponentLocation(),Hit.GetComponent()->GetComponentRotation());
		return true;
	}
	return false;
}

void AInteriorPortalSystem::UpdateBodyVisuals()
{
	for (int32 I=0; I<BodyProxies.Num(); ++I)
	{
		UStaticMeshComponent* Proxy=BodyProxies[I];
		UPrimitiveComponent* Body=PhysicsTravellers[I];
		if (!IsValid(Proxy) || !IsValid(Body) || !BodyMaterials[I] || !ProxyMaterials[I]) { continue; }
		AInteriorPortal* Entry=nullptr;
		if (IsLinked())
		{
			for (AInteriorPortal* P : {BluePortal.Get(),OrangePortal.Get()})
			{
				const FVector L=P->GetLogicalFrame().InverseTransformPositionNoScale(Body->GetComponentLocation());
				if (FMath::Abs(L.X)<Body->Bounds.SphereRadius+2 && BodyFits(Body,P)) { Entry=P; break; }
			}
		}
		Proxy->SetVisibility(Entry!=nullptr);
		BodyMaterials[I]->SetScalarParameterValue(TEXT("SliceEnabled"),Entry?1:0);
		if (!Entry) { continue; }
		AInteriorPortal* Exit=Entry==BluePortal?OrangePortal:BluePortal;
		const FTransform EntryFrame = Entry->GetLogicalFrame();
		const FTransform ExitFrame = Exit->GetLogicalFrame();
		const FQuat Q=InteriorPortalMath::Rotation(EntryFrame,ExitFrame);
		Proxy->SetWorldTransform(FTransform(Q*Body->GetComponentQuat(),InteriorPortalMath::Position(Body->GetComponentLocation(),EntryFrame,ExitFrame),Body->GetComponentScale()));
		BodyMaterials[I]->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(EntryFrame.GetLocation()));
		BodyMaterials[I]->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(EntryFrame.GetUnitAxis(EAxis::X)));
		ProxyMaterials[I]->SetScalarParameterValue(TEXT("SliceEnabled"),1);
		ProxyMaterials[I]->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(ExitFrame.GetLocation()));
		ProxyMaterials[I]->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(ExitFrame.GetUnitAxis(EAxis::X)));
	}
}

bool AInteriorPortalSystem::IsBusy() const
{
	return !IgnoredSupports.IsEmpty() || PassageConstraints.ContainsByPredicate([](const TObjectPtr<UPhysicsConstraintComponent>& Constraint) { return IsValid(Constraint); });
}

bool AInteriorPortalSystem::ValidatePlacement(const FHitResult& Hit, const FVector& ViewRight,
	const AInteriorPortal* Endpoint, FTransform& OutFrame, FString& OutReason) const
{
	if (!Endpoint || !Hit.bBlockingHit || !IsValid(Hit.GetComponent()) || !PortalSurfaces.Contains(Hit.GetComponent()))
	{
		OutReason = TEXT("Aim at a portal surface"); return false;
	}
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
	if (Up.IsNearlyZero()) { Up = FVector::CrossProduct(Normal, ViewRight).GetSafeNormal(); }
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(Normal, Up).ToQuat();
	// The actor transform is the logical aperture plane. Cosmetic mesh depth bias belongs only to AInteriorPortal::Surface.
	OutFrame = FTransform(Rotation, Hit.ImpactPoint);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalPlacement), true);
	for (int32 I = 0; I < 32; ++I)
	{
		const float Angle = I * 2 * PI / 32;
		const FVector Point = OutFrame.TransformPosition(FVector(0, FMath::Cos(Angle)*(Endpoint->HalfWidth+3), FMath::Sin(Angle)*(Endpoint->HalfHeight+3)));
		FHitResult Edge;
		if (!GetWorld()->LineTraceSingleByChannel(Edge, Point+Normal*4, Point-Normal*8, ECC_Visibility, Params)
			|| Edge.GetComponent() != Hit.GetComponent() || FVector::DotProduct(Edge.ImpactNormal, Normal) < .995)
		{
			OutReason = TEXT("Not enough flat space for the whole portal"); return false;
		}
	}
	// Room for an upright child at the opening, ignoring only the supporting primitive.
	Params.bTraceComplex = false;
	Params.AddIgnoredComponent(Hit.GetComponent());
	if (Character.IsValid()) { Params.AddIgnoredActor(Character.Get()); }
	if (GetWorld()->OverlapBlockingTestByChannel(OutFrame.GetLocation()+Normal*30, Rotation, ECC_Pawn,
		FCollisionShape::MakeBox(FVector(25, Endpoint->HalfWidth*.7f, Endpoint->HalfHeight*.7f)), Params))
	{
		OutReason = TEXT("Portal opening obstructed"); return false;
	}
	const AInteriorPortal* Other = Endpoint == BluePortal ? OrangePortal : BluePortal;
	if (IsValid(Other) && Other->bPlaced)
	{
		const FVector Delta = Other->GetLogicalFrame().InverseTransformPositionNoScale(OutFrame.GetLocation());
		if (FMath::Abs(Delta.X) < 10 && FMath::Abs(Delta.Y) < Other->HalfWidth+Endpoint->HalfWidth+6
			&& FMath::Abs(Delta.Z) < Other->HalfHeight+Endpoint->HalfHeight+6)
		{
			OutReason = TEXT("Portals cannot overlap"); return false;
		}
	}
	return true;
}

bool AInteriorPortalSystem::FirePortal(APlayerController* Player, bool bOrange)
{
	if (!Player || !BluePortal || !OrangePortal) { return false; }
	if (IsBusy()) { PlacementMessage = TEXT("Finish crossing before moving a portal"); return false; }
	FVector Start; FRotator View;
	Player->GetPlayerViewPoint(Start, View);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalShot), true, Player->GetPawn());
	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(Hit, Start, Start+View.Vector()*20000, ECC_Visibility, Params);
	AInteriorPortal* Portal = bOrange ? OrangePortal : BluePortal;
	FTransform Frame;
	if (!ValidatePlacement(Hit, FRotationMatrix(View).GetUnitAxis(EAxis::Y), Portal, Frame, PlacementMessage)) { return false; }
	Portal->SetActorTransform(Frame);
	Portal->Support = Hit.GetComponent();
	Portal->bPlaced = true;
	Portal->RefreshAppearance();
	Portal->Capture->bCameraCutThisFrame = true;
	bHasPreviousEye = false;
	for (int32 I=0; I<PreviousBodyPositions.Num(); ++I)
	{
		if (IsValid(PhysicsTravellers[I])) { PreviousBodyPositions[I]=PhysicsTravellers[I]->GetComponentLocation(); }
	}
	PlacementMessage = bOrange ? TEXT("Orange portal placed") : TEXT("Blue portal placed");
	return true;
}

void AInteriorPortalSystem::ResetPortals()
{
	if (IsBusy()) { PlacementMessage = TEXT("Finish crossing before clearing portals"); return; }
	for (AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()})
	{
		if (IsValid(Portal)) { Portal->bPlaced = false; Portal->RefreshAppearance(); Portal->SetView(nullptr, false); }
	}
	bHasPreviousEye = false;
	PlacementMessage = TEXT("Portals cleared");
}

void AInteriorPortalSystem::RenderViews(APlayerController* Player)
{
	if (!IsLinked() || !Player || !Player->PlayerCameraManager) { return; }
	ULocalPlayer* Local = Player->GetLocalPlayer();
	FSceneViewProjectionData ProjectionData;
	if (!Local || !Local->ViewportClient || !Local->GetProjectionData(Local->ViewportClient->Viewport, ProjectionData)) { return; }
	const FIntRect Rect = ProjectionData.GetConstrainedViewRect();
	const int32 Width = FMath::Clamp(FMath::RoundToInt(Rect.Width()*ResolutionScale), 256, 2560);
	const int32 Height = FMath::Max(144, FMath::RoundToInt(Width * double(Rect.Height()) / FMath::Max(1, Rect.Width())));
	const int32 Depth = FMath::Clamp(RecursionDepth, 1, 4);
	const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
	const auto IsVisible = [&ProjectionData](const AInteriorPortal* Portal, const FTransform& View)
	{
		const FTransform PortalFrame = Portal->GetLogicalFrame();
		if (FVector::DotProduct(View.GetLocation()-PortalFrame.GetLocation(), PortalFrame.GetUnitAxis(EAxis::X)) < -.5) { return false; }
		const FVector P = View.InverseTransformPositionNoScale(PortalFrame.GetLocation());
		const double R = FMath::Sqrt(FMath::Square(Portal->HalfWidth)+FMath::Square(Portal->HalfHeight));
		const double XScale = ProjectionData.ProjectionMatrix.M[0][0];
		const double YScale = ProjectionData.ProjectionMatrix.M[1][1];
		// Conservative sphere/frustum test also keeps the aperture visible while the camera crosses it.
		return P.X+R>0 && FMath::Abs(P.Y)*XScale-P.X < R*FMath::Sqrt(1+XScale*XScale)
			&& FMath::Abs(P.Z)*YScale-P.X < R*FMath::Sqrt(1+YScale*YScale);
	};
	for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
	{
		if (!IsVisible(Entry, FTransform(POV.Rotation,POV.Location))) { continue; }
		AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
		const FTransform EntryFrame = Entry->GetLogicalFrame();
		const FTransform ExitFrame = Exit->GetLogicalFrame();
		TArray<FTransform, TInlineAllocator<4>> Views;
		FTransform View(POV.Rotation, POV.Location);
		const FQuat Rotation = InteriorPortalMath::Rotation(EntryFrame, ExitFrame);
		for (int32 I=0; I<Depth; ++I)
		{
			View = FTransform(Rotation*View.GetRotation(), InteriorPortalMath::Position(View.GetLocation(), EntryFrame, ExitFrame));
			Views.Add(View);
			if (!IsVisible(Entry,View)) { break; }
		}
		const int32 VisibleDepth = Views.Num();
		Entry->EnsureTargets(Width, Height, VisibleDepth);
		USceneCaptureComponent2D* Capture = Entry->Capture;
		Capture->HiddenActors.Reset();
		Capture->HiddenActors.Add(Exit);
		Capture->FOVAngle = POV.FOV;
		Capture->PostProcessSettings = POV.PostProcessSettings;
		Capture->PostProcessBlendWeight = 1;
		Capture->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		Capture->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		Capture->PostProcessSettings.bOverride_ReflectionMethod = true;
		Capture->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
		Capture->PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
		Capture->PostProcessSettings.LumenSurfaceCacheResolution = CaptureLumenSurfaceCacheResolution;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->ShowFlags.SetTemporalAA(bCaptureTemporalAA);

		const bool bNativeClip = RenderClipMode == EInteriorPortalRenderClipMode::NativeClipPlane;
		Capture->bEnableClipPlane = bNativeClip;
		if (bNativeClip)
		{
			const FVector ExitNormal = ExitFrame.GetUnitAxis(EAxis::X);
			Capture->ClipPlaneBase = ExitFrame.GetLocation() + ExitNormal * ClipPlaneBias;
			Capture->ClipPlaneNormal = ExitNormal;
			Capture->CustomProjectionMatrix = ProjectionData.ProjectionMatrix;
		}

		bool bCaptureValid = true;
		for (int32 I=VisibleDepth-1; I>=0; --I)
		{
			Entry->SetView(I+1<VisibleDepth ? Entry->RenderTargets[I+1] : nullptr, I+1<VisibleDepth);
			Capture->SetWorldLocationAndRotation(Views[I].GetLocation(), Views[I].GetRotation());
			if (!bNativeClip)
			{
				const FVector N = Views[I].InverseTransformVectorNoScale(ExitFrame.GetUnitAxis(EAxis::X));
				const FVector P = Views[I].InverseTransformPositionNoScale(ExitFrame.GetLocation()+ExitFrame.GetUnitAxis(EAxis::X)*ClipPlaneBias);
				// Camera local X/Y/Z -> projection view Z/X/Y.
				const FVector4 Plane(N.Y, N.Z, N.X, -FVector::DotProduct(N,P));
				FMatrix ObliqueProjection;
				if (!InteriorPortalMath::TryObliqueProjection(ProjectionData.ProjectionMatrix, Plane, ObliqueProjection))
				{
					bCaptureValid = false;
					break;
				}
				Capture->CustomProjectionMatrix = ObliqueProjection;
			}
			Capture->TextureTarget = Entry->RenderTargets[I];
			Capture->CaptureScene();
		}
		Entry->SetView(bCaptureValid ? Entry->RenderTargets[0] : nullptr, bCaptureValid);
	}
}

void AInteriorPortalSystem::UpdateFidelityDiagnostics()
{
	IConsoleVariable* EyeAdaptation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality"));
	IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride"));

	if (bExposureIsolationDiagnostic)
	{
		if (!bExposureDiagnosticsApplied)
		{
			if (EyeAdaptation)
			{
				SavedEyeAdaptationQuality = EyeAdaptation->GetInt();
				bSavedEyeAdaptationQuality = true;
			}
			if (PreExposure)
			{
				SavedPreExposureOverride = PreExposure->GetFloat();
				bSavedPreExposureOverride = true;
			}
			bExposureDiagnosticsApplied = true;
			UE_LOG(LogTemp, Display, TEXT("Portal P2-A exposure isolation enabled. This globally disables eye adaptation for diagnosis only."));
		}
		if (EyeAdaptation) { EyeAdaptation->Set(0, ECVF_SetByCode); }
		if (PreExposure) { PreExposure->Set(DiagnosticPreExposureOverride, ECVF_SetByCode); }
		FidelityDiagnosticStatus = FString::Printf(TEXT("P2-A isolation: CaptureTAA=%s LumenCache=%.2f EyeAdaptation=%s PreExposure=%.3f"),
			bCaptureTemporalAA ? TEXT("ON") : TEXT("OFF"), CaptureLumenSurfaceCacheResolution,
			EyeAdaptation ? TEXT("OFF") : TEXT("CVAR MISSING"), DiagnosticPreExposureOverride);
		return;
	}

	if (bExposureDiagnosticsApplied) { RestoreFidelityDiagnostics(); }
	FidelityDiagnosticStatus = FString::Printf(TEXT("P2-A production path: CaptureTAA=%s LumenCache=%.2f; SceneCapture eye adaptation remains disabled"),
		bCaptureTemporalAA ? TEXT("ON") : TEXT("OFF"), CaptureLumenSurfaceCacheResolution);
}

void AInteriorPortalSystem::RestoreFidelityDiagnostics()
{
	if (!bExposureDiagnosticsApplied) { return; }
	if (IConsoleVariable* EyeAdaptation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality")))
	{
		if (bSavedEyeAdaptationQuality) { EyeAdaptation->Set(SavedEyeAdaptationQuality, ECVF_SetByCode); }
	}
	if (IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
	{
		if (bSavedPreExposureOverride) { PreExposure->Set(SavedPreExposureOverride, ECVF_SetByCode); }
	}
	bExposureDiagnosticsApplied = false;
	bSavedEyeAdaptationQuality = false;
	bSavedPreExposureOverride = false;
	UE_LOG(LogTemp, Display, TEXT("Portal P2-A exposure isolation disabled; previous eye-adaptation/pre-exposure values restored."));
}

void AInteriorPortalSystem::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreFidelityDiagnostics();
	GrabHandle->ReleaseComponent();
	for (UMaterialInstanceDynamic* Material : BodyMaterials) { if (Material) { Material->SetScalarParameterValue(TEXT("SliceEnabled"),0); } }
	RestoreIgnores();
	if (Character.IsValid()) { Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this); }
	for (UPhysicsConstraintComponent* Constraint : PassageConstraints)
	{
		if (IsValid(Constraint)) { Constraint->DestroyComponent(); }
	}
	PassageConstraints.Reset();
	Super::EndPlay(Reason);
}
