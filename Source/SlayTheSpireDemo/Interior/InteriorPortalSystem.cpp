#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
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
		return Camera ? Camera->GetComponentLocation() : Pawn->GetPawnViewLocation();
	}
	bool BodyFits(const UPrimitiveComponent* Body, const AInteriorPortal* Portal)
	{
		const FVector Local = Portal->GetActorTransform().InverseTransformPositionNoScale(Body->GetComponentLocation());
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
	SetActorTickEnabled(true);
}

bool AInteriorPortalSystem::IsLinked() const
{
	return IsValid(BluePortal) && IsValid(OrangePortal) && BluePortal != OrangePortal
		&& BluePortal->bPlaced && OrangePortal->bPlaced;
}

bool AInteriorPortalSystem::FitsCharacter(const ACharacter* Pawn, const FVector& Center, const AInteriorPortal* Portal) const
{
	const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
	const float R = Capsule->GetScaledCapsuleRadius() + 1;
	const float Segment = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
	const FTransform Frame = Portal->GetActorTransform();
	const FVector Local = Frame.InverseTransformPositionNoScale(Center);
	const FVector Spine = Frame.InverseTransformVectorNoScale(FVector::UpVector * Segment);
	for (int32 I = 0; I < 16; ++I)
	{
		const float Angle = I * 2 * PI / 16;
		const FVector Rim(0, R * FMath::Cos(Angle), R * FMath::Sin(Angle));
		if (!InteriorPortalMath::Inside(Local + Spine + Rim, Portal->HalfWidth * .94, Portal->HalfHeight * .94)
			|| !InteriorPortalMath::Inside(Local - Spine + Rim, Portal->HalfWidth * .94, Portal->HalfHeight * .94)) { return false; }
	}
	return true;
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
	ACharacter* Pawn = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Character.Get() != Pawn)
	{
		RestoreIgnores();
		Character = Pawn;
		bHasPreviousEye = false;
		LastPlayerExit.Reset();
		if (Pawn) { Pawn->GetCharacterMovement()->AddTickPrerequisiteActor(this); }
	}
	RestoreIgnores();
	if (Pawn && IsLinked())
	{
		const FVector Center = Pawn->GetActorLocation();
		if (LastPlayerExit.IsValid())
		{
			const FVector Local = LastPlayerExit->GetActorTransform().InverseTransformPositionNoScale(Center);
			if (FMath::Abs(Local.X) > Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+12) { LastPlayerExit.Reset(); }
		}
		for (AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()})
		{
			const FTransform Frame = Portal->GetActorTransform();
			const FVector Local = Frame.InverseTransformPositionNoScale(Center);
			const float Reach = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 12;
			const float Approach = Reach + Pawn->GetVelocity().Size() * FMath::Min(DeltaSeconds, .1f);
			const bool bExiting = LastPlayerExit.Get() == Portal && FMath::Abs(Local.X) < Reach
				&& InteriorPortalMath::Inside(Local, Portal->HalfWidth, Portal->HalfHeight);
			if (IsValid(Portal->Support) && FMath::Abs(Local.X) < Approach
				&& (FitsCharacter(Pawn, Center, Portal) || bExiting))
			{
				Pawn->GetCapsuleComponent()->IgnoreComponentWhenMoving(Portal->Support, true);
				IgnoredSupports.AddUnique(Portal->Support);
			}
		}
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
					Eye = InteriorPortalMath::Position(Eye,Entry->GetActorTransform(),Exit->GetActorTransform());
					Desired = InteriorPortalMath::Position(Desired,Entry->GetActorTransform(),Exit->GetActorTransform());
					TargetRotation = (InteriorPortalMath::Rotation(Entry->GetActorTransform(),Exit->GetActorTransform())*TargetRotation.Quaternion()).Rotator();
					if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
				}
				else
				{
					for (AInteriorPortal* Entry : {BluePortal.Get(),OrangePortal.Get()})
					{
						FVector Intersection;
						if (Entry->Support && InteriorPortalMath::Crossed(Eye,Desired,Entry->GetActorTransform(),Entry->HalfWidth-30,Entry->HalfHeight-30,Intersection))
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
				const double Distance = FVector::DotProduct(Body->GetComponentLocation() - Portal->GetActorLocation(), Portal->GetActorForwardVector());
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
				const double D = FVector::DotProduct(Body->GetComponentLocation()-Exit->GetActorLocation(), Exit->GetActorForwardVector());
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

void AInteriorPortalSystem::UpdateTraversal(APlayerController* Player)
{
	if (!IsLinked() || !Player) { bHasPreviousEye = false; return; }
	ACharacter* Pawn = Cast<ACharacter>(Player->GetPawn());
	if (Pawn && Character.Get() == Pawn)
	{
		FVector Eye = EyeOf(Pawn);
		if (bHasPreviousEye)
		{
			for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
			{
				FVector Intersection;
				if (!InteriorPortalMath::Crossed(PreviousEye, Eye, Entry->GetActorTransform(), Entry->HalfWidth*.94, Entry->HalfHeight*.94, Intersection)) { continue; }
				const FVector CenterAtPlane = Pawn->GetActorLocation() + Intersection - Eye;
				if (!FitsCharacter(Pawn, CenterAtPlane, Entry)) { continue; }
				AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
				const FQuat Rotation = InteriorPortalMath::Rotation(Entry->GetActorTransform(), Exit->GetActorTransform());
				const FVector Velocity = Rotation.RotateVector(Pawn->GetCharacterMovement()->Velocity);
				const FRotator View = (Rotation * Player->GetControlRotation().Quaternion()).Rotator();
				const FVector NewEye = InteriorPortalMath::Position(Eye, Entry->GetActorTransform(), Exit->GetActorTransform());
				const FVector EyeOffset = Eye - Pawn->GetActorLocation();
				const FVector Location = NewEye - EyeOffset;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalExit), false, Pawn);
				if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
				const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
				if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params))
				{
					Pawn->SetActorLocation(Pawn->GetActorLocation() + Intersection - Eye + Entry->GetActorForwardVector() * 2, false);
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
					Held->SetWorldLocationAndRotation(InteriorPortalMath::Position(Held->GetComponentLocation(), Entry->GetActorTransform(), Exit->GetActorTransform()), Rotation*Held->GetComponentQuat(), false, nullptr, ETeleportType::TeleportPhysics);
					Held->SetPhysicsLinearVelocity(HeldVelocity);
					const int32 HeldIndex = PhysicsTravellers.IndexOfByKey(Held);
					if (PreviousBodyPositions.IsValidIndex(HeldIndex)) { PreviousBodyPositions[HeldIndex]=Held->GetComponentLocation(); BodyExits[HeldIndex]=Exit; }
					GrabHandle->GrabComponentAtLocationWithRotation(Held,NAME_None,Held->GetComponentLocation(),Held->GetComponentRotation());
					}
				}
				Player->SetControlRotation(View);
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				Pawn->GetCharacterMovement()->Velocity = Velocity;
				LastPlayerExit = Exit;
				++PlayerCrossings;
				break;
			}
		}
		PreviousEye = EyeOf(Pawn);
		bHasPreviousEye = true;
	}
	for (int32 I = 0; I < PreviousBodyPositions.Num(); ++I)
	{
		UPrimitiveComponent* Body = PhysicsTravellers[I];
		if (!IsValid(Body) || !Body->IsSimulatingPhysics()) { continue; }
		for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
		{
			FVector Intersection;
			if (!InteriorPortalMath::Crossed(PreviousBodyPositions[I], Body->GetComponentLocation(), Entry->GetActorTransform(),
				Entry->HalfWidth, Entry->HalfHeight, Intersection) || !BodyFits(Body, Entry)) { continue; }
			AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
			const FQuat Rotation = InteriorPortalMath::Rotation(Entry->GetActorTransform(), Exit->GetActorTransform());
			const FVector Velocity = Rotation.RotateVector(Body->GetPhysicsLinearVelocity());
			const FVector Spin = Rotation.RotateVector(Body->GetPhysicsAngularVelocityInRadians());
			const FVector Destination = InteriorPortalMath::Position(Body->GetComponentLocation(), Entry->GetActorTransform(), Exit->GetActorTransform());
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PortalPhysicsExit), false, Body->GetOwner());
			if (Exit->Support) { Params.AddIgnoredComponent(Exit->Support.Get()); }
			if (GetWorld()->OverlapBlockingTestByChannel(Destination, FQuat::Identity, Body->GetCollisionObjectType(), FCollisionShape::MakeSphere(Body->Bounds.SphereRadius*.6f),Params))
			{
				Body->SetWorldLocation(Intersection+Entry->GetActorForwardVector()*(Body->Bounds.SphereRadius+1),false,nullptr,ETeleportType::TeleportPhysics);
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
				const FVector L=P->GetActorTransform().InverseTransformPositionNoScale(Body->GetComponentLocation());
				if (FMath::Abs(L.X)<Body->Bounds.SphereRadius+2 && BodyFits(Body,P)) { Entry=P; break; }
			}
		}
		Proxy->SetVisibility(Entry!=nullptr);
		BodyMaterials[I]->SetScalarParameterValue(TEXT("SliceEnabled"),Entry?1:0);
		if (!Entry) { continue; }
		AInteriorPortal* Exit=Entry==BluePortal?OrangePortal:BluePortal;
		const FQuat Q=InteriorPortalMath::Rotation(Entry->GetActorTransform(),Exit->GetActorTransform());
		Proxy->SetWorldTransform(FTransform(Q*Body->GetComponentQuat(),InteriorPortalMath::Position(Body->GetComponentLocation(),Entry->GetActorTransform(),Exit->GetActorTransform()),Body->GetComponentScale()));
		BodyMaterials[I]->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(Entry->GetActorLocation()));
		BodyMaterials[I]->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(Entry->GetActorForwardVector()));
		ProxyMaterials[I]->SetScalarParameterValue(TEXT("SliceEnabled"),1);
		ProxyMaterials[I]->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(Exit->GetActorLocation()));
		ProxyMaterials[I]->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(Exit->GetActorForwardVector()));
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
	OutFrame = FTransform(Rotation, Hit.ImpactPoint + Normal * .6f);
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
		const FVector Delta = Other->GetActorTransform().InverseTransformPositionNoScale(OutFrame.GetLocation());
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
		if (FVector::DotProduct(View.GetLocation()-Portal->GetActorLocation(), Portal->GetActorForwardVector()) < -.5) { return false; }
		const FVector P = View.InverseTransformPositionNoScale(Portal->GetActorLocation());
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
		TArray<FTransform, TInlineAllocator<4>> Views;
		FTransform View(POV.Rotation, POV.Location);
		const FQuat Rotation = InteriorPortalMath::Rotation(Entry->GetActorTransform(), Exit->GetActorTransform());
		for (int32 I=0; I<Depth; ++I)
		{
			View = FTransform(Rotation*View.GetRotation(), InteriorPortalMath::Position(View.GetLocation(), Entry->GetActorTransform(), Exit->GetActorTransform()));
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
		for (int32 I=VisibleDepth-1; I>=0; --I)
		{
			Entry->SetView(I+1<VisibleDepth ? Entry->RenderTargets[I+1] : nullptr, I+1<VisibleDepth);
			Capture->SetWorldLocationAndRotation(Views[I].GetLocation(), Views[I].GetRotation());
			const FVector N = Views[I].InverseTransformVectorNoScale(Exit->GetActorForwardVector());
			const FVector P = Views[I].InverseTransformPositionNoScale(Exit->GetActorLocation()+Exit->GetActorForwardVector()*.1f);
			// Camera local X/Y/Z -> projection view Z/X/Y.
			const FVector4 Plane(N.Y, N.Z, N.X, -FVector::DotProduct(N,P));
			Capture->CustomProjectionMatrix = InteriorPortalMath::ObliqueProjection(ProjectionData.ProjectionMatrix, Plane);
			Capture->TextureTarget = Entry->RenderTargets[I];
			Capture->CaptureScene();
		}
		Entry->SetView(Entry->RenderTargets[0], true);
	}
}

void AInteriorPortalSystem::EndPlay(const EEndPlayReason::Type Reason)
{
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
