#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalRenderer.h"
#include "InteriorPortalQuery.h"
#include "InteriorPlayerController.h"
#include "InteriorPortalMovementComponent.h"
#include "InteriorPortalPresentation.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	struct FPortalBodySupport
	{
		float Normal = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
	};

	FPortalBodySupport BodySupport(const UPrimitiveComponent* Body, const FTransform& PortalFrame)
	{
		FPortalBodySupport Result;
		if (!Body) { return Result; }

		const FCollisionShape Shape = Body->GetCollisionShape();
		const FVector PortalX = PortalFrame.GetUnitAxis(EAxis::X);
		const FVector PortalY = PortalFrame.GetUnitAxis(EAxis::Y);
		const FVector PortalZ = PortalFrame.GetUnitAxis(EAxis::Z);
		const FQuat BodyRotation = Body->GetComponentQuat();

		if (Shape.IsSphere())
		{
			Result.Normal = Result.Width = Result.Height = Shape.GetSphereRadius();
			return Result;
		}

		if (Shape.IsCapsule())
		{
			// UE capsules use local +Z as their axial direction. The radial
			// support and axial half-length are projected independently into the
			// portal frame, so a rotated capsule cannot scrape through the rim
			// merely because its center fits.
			const FVector Axis = BodyRotation.GetAxisZ();
			const float Radius = Shape.GetCapsuleRadius();
			const float HalfLength = Shape.GetCapsuleAxisHalfLength();
			Result.Normal = Radius + FMath::Abs(FVector::DotProduct(Axis, PortalX)) * HalfLength;
			Result.Width = Radius + FMath::Abs(FVector::DotProduct(Axis, PortalY)) * HalfLength;
			Result.Height = Radius + FMath::Abs(FVector::DotProduct(Axis, PortalZ)) * HalfLength;
			return Result;
		}

		if (Shape.IsBox())
		{
			const FVector Extent = Shape.GetBox();
			// Shape components expose local extents. Generic primitive components
			// (including static meshes) expose a world-aligned conservative bounds
			// box from the base implementation, so use world axes for those.
			const bool bLocalAxes = Body->IsA<UBoxComponent>();
			const FVector AxisX = bLocalAxes ? BodyRotation.GetAxisX() : FVector::ForwardVector;
			const FVector AxisY = bLocalAxes ? BodyRotation.GetAxisY() : FVector::RightVector;
			const FVector AxisZ = bLocalAxes ? BodyRotation.GetAxisZ() : FVector::UpVector;
			const auto ProjectBox = [&Extent, &AxisX, &AxisY, &AxisZ](const FVector& Axis)
			{
				return FMath::Abs(FVector::DotProduct(Axis, AxisX)) * Extent.X
					+ FMath::Abs(FVector::DotProduct(Axis, AxisY)) * Extent.Y
					+ FMath::Abs(FVector::DotProduct(Axis, AxisZ)) * Extent.Z;
			};
			Result.Normal = ProjectBox(PortalX);
			Result.Width = ProjectBox(PortalY);
			Result.Height = ProjectBox(PortalZ);
			return Result;
		}

		Result.Normal = Result.Width = Result.Height = Body->Bounds.SphereRadius;
		return Result;
	}

	bool BodyFits(const UPrimitiveComponent* Body, const AInteriorPortal* Portal)
	{
		if (!Body || !Portal) { return false; }
		const FVector Local = Portal->GetLogicalFrame().InverseTransformPositionNoScale(Body->GetComponentLocation());
		const FPortalBodySupport Support = BodySupport(Body, Portal->GetLogicalFrame());
		return InteriorPortalMath::Inside(Local, Portal->HalfWidth, Portal->HalfHeight,
			Support.Width, Support.Height);
	}
	bool BodyFitsAt(const UPrimitiveComponent* Body, const AInteriorPortal* Portal, const FVector& Location)
	{
		if (!Body || !Portal) { return false; }
		const FVector Local = Portal->GetLogicalFrame().InverseTransformPositionNoScale(Location);
		const FPortalBodySupport Support = BodySupport(Body, Portal->GetLogicalFrame());
		return InteriorPortalMath::Inside(Local, Portal->HalfWidth, Portal->HalfHeight,
			Support.Width, Support.Height);
	}
}

ESceneCaptureSource AInteriorPortalSystem::GetCaptureSourceForColorMode(const EInteriorPortalCaptureColorMode Mode)
{
	return Mode == EInteriorPortalCaptureColorMode::FinalColorHDR
		? SCS_FinalColorHDR : SCS_SceneColorHDRNoAlpha;
}

bool AInteriorPortalSystem::UsesCaptureEyeAdaptation(const EInteriorPortalCaptureColorMode Mode)
{
	return Mode == EInteriorPortalCaptureColorMode::FinalColorHDR;
}

bool AInteriorPortalSystem::UsesSceneCapture(const EInteriorPortalRendererBackend Backend)
{
	return Backend == EInteriorPortalRendererBackend::SceneCapture;
}

bool AInteriorPortalSystem::UsesMainViewStencil(const EInteriorPortalRendererBackend Backend)
{
	return Backend == EInteriorPortalRendererBackend::MainViewStencilSpike;
}

bool AInteriorPortalSystem::UsesCustomRenderPass(const EInteriorPortalRendererBackend Backend)
{
	return Backend == EInteriorPortalRendererBackend::CustomRenderPassSpike;
}

bool AInteriorPortalSystem::RequiresRendererHistoryReset(
	const EInteriorPortalRendererBackend PreviousBackend,
	const EInteriorPortalRendererBackend NewBackend)
{
	return PreviousBackend != NewBackend;
}

AInteriorPortalSystem::AInteriorPortalSystem()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PortalSystemRoot"));
	GrabHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PortalCubeHandle"));
	PlayerPresentation = CreateDefaultSubobject<UInteriorPortalPresentation>(TEXT("PortalPlayerPresentation"));
	// Only active play needs pre-movement collision preparation, never editor ticking.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	PreviousVirtualViews.Init(FTransform::Identity, 8);
	bPreviousVirtualViewsValid.Init(false, 8);
	CaptureHistoryGenerations.Init(0, 8);
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
	MainViewStencilExtension = FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(GetWorld());
	MainViewStencilExtension->SetEnabled(false);
	for (FSceneViewStateReference& ViewState : CustomRenderPassViewStates)
	{
		ViewState.Allocate(GetWorld()->GetFeatureLevel());
	}
	const TArray<TObjectPtr<UPrimitiveComponent>> AuthoredTravellers = PhysicsTravellers;
	PhysicsTravellers.Reset();
	for (UPrimitiveComponent* Traveller : AuthoredTravellers) { RegisterPhysicsTraveller(Traveller); }
	DiscoverTaggedTravellers();
	InvalidateRendererHistories(TEXT("begin play"));
	UpdateFidelityDiagnostics();
	SetActorTickEnabled(true);
}

bool AInteriorPortalSystem::RegisterPhysicsTraveller(UPrimitiveComponent* Traveller)
{
	if (!IsValid(Traveller) || Traveller->GetOwner() == this || !Traveller->IsSimulatingPhysics()
		|| PhysicsTravellers.Contains(Traveller))
	{
		return false;
	}
	const int32 Index = PhysicsTravellers.Add(Traveller);
	PreviousBodyPositions.Add(Traveller->GetComponentLocation());
	LastSafeBodyPositions.Add(Traveller->GetComponentLocation());
	BodyExits.Add(nullptr);
	PassageConstraints.Add(nullptr);
	BodyProxies.Add(nullptr);
	BodyMaterials.Add(nullptr);
	ProxyMaterials.Add(nullptr);
	if (UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Traveller))
	{
		UStaticMeshComponent* Proxy = NewObject<UStaticMeshComponent>(this);
		Proxy->SetStaticMesh(Body->GetStaticMesh());
		Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Proxy->SetCanEverAffectNavigation(false);
		Proxy->SetVisibility(false);
		Proxy->RegisterComponent();
		ProxyMaterials[Index] = Proxy->CreateDynamicMaterialInstance(0, Body->GetMaterial(0));
		BodyMaterials[Index] = Body->CreateDynamicMaterialInstance(0);
		BodyProxies[Index] = Proxy;
	}
	return true;
}

bool AInteriorPortalSystem::UnregisterPhysicsTraveller(UPrimitiveComponent* Traveller)
{
	const int32 Index = PhysicsTravellers.IndexOfByKey(Traveller);
	if (Index == INDEX_NONE) { return false; }
	if (GrabHandle && GrabHandle->GetGrabbedComponent() == Traveller) { GrabHandle->ReleaseComponent(); }
	if (PassageConstraints.IsValidIndex(Index) && IsValid(PassageConstraints[Index]))
	{
		PassageConstraints[Index]->DestroyComponent();
	}
	if (BodyProxies.IsValidIndex(Index) && IsValid(BodyProxies[Index]))
	{
		BodyProxies[Index]->DestroyComponent();
	}
	PhysicsTravellers.RemoveAt(Index);
	PreviousBodyPositions.RemoveAt(Index);
	LastSafeBodyPositions.RemoveAt(Index);
	BodyExits.RemoveAt(Index);
	PassageConstraints.RemoveAt(Index);
	BodyProxies.RemoveAt(Index);
	BodyMaterials.RemoveAt(Index);
	ProxyMaterials.RemoveAt(Index);
	return true;
}

void AInteriorPortalSystem::RemoveInvalidTravellers()
{
	for (int32 Index = PhysicsTravellers.Num() - 1; Index >= 0; --Index)
	{
		UPrimitiveComponent* Traveller = PhysicsTravellers[Index];
		if (!IsValid(Traveller) || !Traveller->IsSimulatingPhysics())
		{
			UnregisterPhysicsTraveller(Traveller);
		}
	}
}

void AInteriorPortalSystem::DiscoverTaggedTravellers()
{
	if (!GetWorld()) { return; }
	RemoveInvalidTravellers();
	TArray<UPrimitiveComponent*> Candidates;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UPrimitiveComponent*> Components;
		It->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (IsValid(Component) && Component->ComponentHasTag(TEXT("PortalTraveller"))
				&& Component->IsSimulatingPhysics() && !PhysicsTravellers.Contains(Component))
			{
				Candidates.Add(Component);
			}
		}
	}
	Candidates.Sort([](const UPrimitiveComponent& A, const UPrimitiveComponent& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (UPrimitiveComponent* Candidate : Candidates) { RegisterPhysicsTraveller(Candidate); }
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
		const bool bApproachesPlane = (Start.X >= 0.0f && Delta.X < 0.0f)
			|| (Start.X <= 0.0f && Delta.X > 0.0f);
		if (!bApproachesPlane) { continue; }
		float PlaneTime = 0.0f;
		if (!FMath::IsNearlyZero(Delta.X)) { PlaneTime = FMath::Clamp(-Start.X / Delta.X, 0.0f, 1.0f); }
		const FVector NearestToPlane = Start + Delta * PlaneTime;
		if (FMath::Abs(NearestToPlane.X) > TraceRadius + 1.0f) { continue; }
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
	FinishCharacterMove(Pawn);
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
		Radius,Gate->HalfWidth*.94,Gate->HalfHeight*.94,Enter,Leave,N,true);
	if (!bFits) { RecoverCharacterPassage(); return 1; }
	double Fraction=bFits?FMath::Clamp(Leave,0.0,1.0):0;
	FVector HitNormal=Frame.TransformVectorNoScale(N);
	const double Clearance=CharacterNormalExtent(Pawn,Frame)+2;
	if (bFits && Move.X>0 && (Clearance-Local.X)/Move.X<=Fraction) { Fraction=1; }
	if (Move.X < -UE_SMALL_NUMBER && !FitsCharacter(Pawn,Center,Gate))
	{
		Fraction=0;
		HitNormal=Frame.GetUnitAxis(EAxis::X);
	}
	if ((IsPlayerClearingPortal() || LastPlayerTransferFrame==GFrameCounter) && Move.X<0)
	{
		const double EyeX=Frame.InverseTransformPositionNoScale(EyeOf(Pawn)).X;
		const double Stop=FMath::Clamp((.05-EyeX)/Move.X,0.0,1.0);
		if (Stop<Fraction) { Fraction=Stop; HitNormal=Frame.GetUnitAxis(EAxis::X); }
	}
	if (Fraction<1)
	{
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
	DiscoverTaggedTravellers();
	UpdateFidelityDiagnostics();
	if (!IsLinked() && bWasRendererLinked)
	{
		InvalidateRendererHistories(TEXT("endpoint destruction or pair became invalid"));
		bWasRendererLinked = false;
	}
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
	for (int32 I = 0; I < PhysicsTravellers.Num(); ++I)
	{
		UPrimitiveComponent* Body = PhysicsTravellers[I];
		UPrimitiveComponent* Support = nullptr;
		AInteriorPortal* InvalidPortal = nullptr;
		bool bInvalidInsideSupport = false;
		bool bNearSupport = false;
		if (IsValid(Body) && Body->IsSimulatingPhysics() && IsLinked())
		{
			const FVector Location = Body->GetComponentLocation();
			const FVector Velocity = Body->GetPhysicsLinearVelocity();
			const float Delta = FMath::Min(GetWorld()->GetDeltaSeconds(), .1f);
			const FVector PredictedLocation = Location + Velocity * Delta;
			for (AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()})
			{
				if (!IsValid(Portal) || !IsValid(Portal->Support)) { continue; }
				const FTransform Frame = Portal->GetLogicalFrame();
				const FVector Normal = Frame.GetUnitAxis(EAxis::X);
				const FPortalBodySupport BodyExtent = BodySupport(Body, Frame);
				const double Distance = FVector::DotProduct(Location - Frame.GetLocation(), Normal);
				const double PredictedDistance = FVector::DotProduct(PredictedLocation - Frame.GetLocation(), Normal);
				const double Approach = BodyExtent.Normal + 20 + Velocity.Size() * Delta;
				if (FMath::Abs(Distance) >= Approach) { continue; }
				bNearSupport = true;
				const bool bFits = BodyFits(Body, Portal);
				const bool bPredictedFits = BodyFitsAt(Body, Portal, PredictedLocation);
				const bool bWillClearSupport = FMath::Abs(PredictedDistance) > BodyExtent.Normal + 20;
				if (bFits && (bPredictedFits || bWillClearSupport))
				{
					Support = Portal->Support;
					break;
				}
				if (!bFits && FMath::Abs(Distance) <= BodyExtent.Normal + 2)
				{
					bInvalidInsideSupport = true;
					InvalidPortal = Portal;
				}
			}
			if (BodyExits[I].IsValid())
			{
				AInteriorPortal* Exit = BodyExits[I].Get();
				if (IsValid(Exit) && IsValid(Exit->Support))
				{
					const FTransform ExitFrame = Exit->GetLogicalFrame();
					const FPortalBodySupport ExitExtent = BodySupport(Body, ExitFrame);
					const double D = FVector::DotProduct(Location - ExitFrame.GetLocation(), ExitFrame.GetUnitAxis(EAxis::X));
					const double PredictedD = FVector::DotProduct(PredictedLocation - ExitFrame.GetLocation(), ExitFrame.GetUnitAxis(EAxis::X));
					if (FMath::Abs(D) < ExitExtent.Normal + 20)
					{
						bNearSupport = true;
						const bool bFits = BodyFits(Body, Exit);
						const bool bPredictedFits = BodyFitsAt(Body, Exit, PredictedLocation);
						const bool bWillClearSupport = FMath::Abs(PredictedD) > ExitExtent.Normal + 20;
						if (bFits && (bPredictedFits || bWillClearSupport)) { Support = Exit->Support; }
						else if (!bFits && FMath::Abs(D) <= ExitExtent.Normal + 2)
						{
							bInvalidInsideSupport = true;
							InvalidPortal = Exit;
						}
					}
					else { BodyExits[I].Reset(); }
				}
				else { BodyExits[I].Reset(); }
			}
		}
		if (bInvalidInsideSupport && IsValid(Body) && LastSafeBodyPositions.IsValidIndex(I))
		{
			const FVector SafeLocation = LastSafeBodyPositions[I];
			if (!Body->GetComponentLocation().Equals(SafeLocation, .01f))
			{
				Body->SetWorldLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (InvalidPortal)
			{
				const FTransform Frame = InvalidPortal->GetLogicalFrame();
				const FVector Local = Frame.InverseTransformPositionNoScale(SafeLocation);
				const FVector Outward = Frame.GetUnitAxis(EAxis::X) * (Local.X >= 0.0f ? 1.0f : -1.0f);
				FVector SafeVelocity = Body->GetPhysicsLinearVelocity();
				const float InwardSpeed = FVector::DotProduct(SafeVelocity, -Outward);
				if (InwardSpeed > 0.0f) { SafeVelocity += Outward * InwardSpeed; }
				Body->SetPhysicsLinearVelocity(SafeVelocity);
			}
			Support = nullptr;
		}
		else if (IsValid(Body) && (!bNearSupport || Support || !IsLinked())
			&& LastSafeBodyPositions.IsValidIndex(I))
		{
			LastSafeBodyPositions[I] = Body->GetComponentLocation();
		}
		UPhysicsConstraintComponent* Constraint = PassageConstraints[I];
		if (Constraint && (!Support || Constraint->OverrideComponent1.Get() != Support))
		{
			Constraint->DestroyComponent();
			PassageConstraints[I] = nullptr;
		}
		if (Support && !PassageConstraints[I])
		{
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
	if (InteriorPortalQuery::LineTrace(this,Eye,Eye+View.Vector()*220,ECC_Visibility,Params,Hit,3,1.0f)
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
	InvalidateRendererHistories(TEXT("portal placement or replacement"));
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
		if (IsValid(Portal))
		{
			Portal->bPlaced = false;
			Portal->RefreshAppearance();
			Portal->SetView(nullptr, false, 1.0f, TEXT("Not applicable: no captured image"), 0.0f);
		}
	}
	InvalidateRendererHistories(TEXT("portal clear"));
	bHasPreviousEye = false;
	PlacementMessage = TEXT("Portals cleared");
}

void AInteriorPortalSystem::RenderViews(APlayerController* Player)
{
	PlayerPresentation->Update(this,Player?Cast<ACharacter>(Player->GetPawn()):nullptr,PlayerGate.Get());
	const bool bUsingMainViewStencil = UsesMainViewStencil(RendererBackend);
	if (!bRendererBackendInitialized || LastRendererBackend != RendererBackend)
	{
		const FString Reason = !bRendererBackendInitialized ? TEXT("renderer backend initialized") : TEXT("renderer backend changed");
		InvalidateRendererHistories(Reason);
		bRendererBackendInitialized = true;
		LastRendererBackend = RendererBackend;
		if (MainViewStencilExtension)
		{
			MainViewStencilExtension->SetEnabled(bUsingMainViewStencil);
			MainViewStencilExtension->ClearRequest();
		}
	}
	if (!IsLinked() || !Player || !Player->PlayerCameraManager) { return; }
	ULocalPlayer* Local = Player->GetLocalPlayer();
	FSceneViewProjectionData ProjectionData;
	if (!Local || !Local->ViewportClient || !Local->GetProjectionData(Local->ViewportClient->Viewport, ProjectionData)) { return; }
	const FIntRect Rect = ProjectionData.GetConstrainedViewRect();
	const int32 Width = FMath::Clamp(FMath::RoundToInt(Rect.Width()*ResolutionScale), 256, 2560);
	const int32 Height = FMath::Max(144, FMath::RoundToInt(Width * double(Rect.Height()) / FMath::Max(1, Rect.Width())));
	const int32 Depth = FMath::Clamp(RecursionDepth, 1, 4);
	const FMinimalViewInfo& POV = Player->PlayerCameraManager->GetCameraCacheView();
	const bool bFinalColorHDR = CaptureColorMode == EInteriorPortalCaptureColorMode::FinalColorHDR;
	const bool bCaptureEyeAdaptation = UsesCaptureEyeAdaptation(CaptureColorMode);
	const FTransform PlayerView(POV.Rotation, POV.Location);
	const FMatrix PortalViewPlanes(FPlane(0,0,1,0),FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
	const auto ViewProjectionForTransform = [&ProjectionData,&PortalViewPlanes](const FTransform& View)
	{
		return FTranslationMatrix(-View.GetLocation()) * FInverseRotationMatrix(View.Rotator()) * PortalViewPlanes * ProjectionData.ProjectionMatrix;
	};
	const auto IsVisible = [&ProjectionData,&Rect,&ViewProjectionForTransform](const AInteriorPortal* Portal,const FTransform& View,InteriorPortalMath::FPortalScreenBounds& OutBounds)
	{
		return Portal && InteriorPortalMath::ProjectPortalApertureToScreenBounds(Portal->GetLogicalFrame(),Portal->HalfWidth,Portal->HalfHeight,ViewProjectionForTransform(View),Rect,OutBounds,ProjectionData.IsPerspectiveProjection(),ProjectionData.GetNearPlaneFromProjectionMatrix());
	};
	if (!UsesSceneCapture(RendererBackend)) { return; }
	FString SamplesJson;
	int32 SampleCount = 0;
	for (AInteriorPortal* Entry : {BluePortal.Get(), OrangePortal.Get()})
	{
		InteriorPortalMath::FPortalScreenBounds DirectBounds;
		if (!IsVisible(Entry, PlayerView, DirectBounds)) { continue; }
		AInteriorPortal* Exit = Entry == BluePortal ? OrangePortal : BluePortal;
		const FTransform EntryFrame = Entry->GetLogicalFrame();
		const FTransform ExitFrame = Exit->GetLogicalFrame();
		TArray<FTransform, TInlineAllocator<4>> Views;
		TArray<InteriorPortalMath::FPortalScreenBounds, TInlineAllocator<4>> ViewBounds;
		FTransform View = PlayerView;
		const FQuat Rotation = InteriorPortalMath::Rotation(EntryFrame, ExitFrame);
		// Direct visibility above already proves that the player can see Entry.
		// Depth 0 is the primary through-portal view, so it must be rendered even
		// when that mapped camera cannot see Entry again for recursion.
		View = FTransform(Rotation*View.GetRotation(), InteriorPortalMath::Position(View.GetLocation(), EntryFrame, ExitFrame));
		InteriorPortalMath::FPortalScreenBounds Depth0Bounds;
		IsVisible(Entry, View, Depth0Bounds);
		Views.Add(View);
		ViewBounds.Add(Depth0Bounds);
		// Only subsequent recursion levels depend on recursive visibility.
		for (int32 I=1; I<Depth; ++I)
		{
			InteriorPortalMath::FPortalScreenBounds RecursiveGateBounds;
			if (!IsVisible(Entry, View, RecursiveGateBounds)) { break; }
			View = FTransform(Rotation*View.GetRotation(), InteriorPortalMath::Position(View.GetLocation(), EntryFrame, ExitFrame));
			InteriorPortalMath::FPortalScreenBounds MappedBounds;
			IsVisible(Entry, View, MappedBounds);
			Views.Add(View);
			ViewBounds.Add(MappedBounds);
		}
		const int32 VisibleDepth = Views.Num();
		Entry->EnsureTargets(Width, Height, VisibleDepth);
		Entry->EnsureCaptureViews(VisibleDepth);
		Entry->SetCaptureColorMode(bFinalColorHDR);
		bool bCaptureValid = true;
		for (int32 I=VisibleDepth-1; I>=0; --I)
		{
			USceneCaptureComponent2D* Capture = Entry->GetCaptureForDepth(I);
			if (!Capture) { bCaptureValid = false; break; }
			Capture->CaptureSource = GetCaptureSourceForColorMode(CaptureColorMode);
			Capture->ShowFlags.SetEyeAdaptation(bCaptureEyeAdaptation);
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
			Entry->SetView(I+1<VisibleDepth ? Entry->RenderTargets[I+1] : nullptr, I+1<VisibleDepth, 1.0f, TEXT("Depth-zero capture kept live; exposure ownership unchanged"), 0.0f);
			Capture->SetWorldLocationAndRotation(Views[I].GetLocation(), Views[I].GetRotation());
			if (!bNativeClip)
			{
				const FVector N = Views[I].InverseTransformVectorNoScale(ExitFrame.GetUnitAxis(EAxis::X));
				const FVector P = Views[I].InverseTransformPositionNoScale(ExitFrame.GetLocation()+ExitFrame.GetUnitAxis(EAxis::X)*ClipPlaneBias);
				const FVector4 Plane(N.Y, N.Z, N.X, -FVector::DotProduct(N,P));
				FMatrix ObliqueProjection;
				if (!InteriorPortalMath::TryObliqueProjection(ProjectionData.ProjectionMatrix, Plane, ObliqueProjection)) { bCaptureValid=false; break; }
				Capture->CustomProjectionMatrix = ObliqueProjection;
			}
			Capture->TextureTarget = Entry->RenderTargets[I];
			Capture->CaptureScene();
		}
		Entry->SetView(bCaptureValid ? Entry->RenderTargets[0] : nullptr, bCaptureValid, 1.0f,
			bFinalColorHDR ? TEXT("Unavailable / Unverified: capture image exposure is not publicly image-bound") : TEXT("Not applicable / UE 5.8 contract: capture EyeAdaptation OFF makes PreExposure 1; no readback used"), 0.0f);
	}
	bWasRendererLinked = true;
}

void AInteriorPortalSystem::WriteMainViewStencilSpikeDiagnostics(const FInteriorPortalRenderRequest*, EInteriorPortalSpikeStatus, const FString&) {}
void AInteriorPortalSystem::WriteCustomRenderPassSpikeDiagnostics(const FInteriorPortalRenderRequest*, EInteriorPortalSpikeStatus, bool, const FString&) {}
void AInteriorPortalSystem::InvalidateRendererHistorySlot(const int32 EndpointIndex, const int32 RecursionLevel, const FString& Reason)
{
	if (EndpointIndex < 0 || EndpointIndex > 1 || RecursionLevel < 0 || RecursionLevel >= 4) { return; }
	const int32 Slot = EndpointIndex * 4 + RecursionLevel;
	if (CaptureHistoryGenerations.Num() < 8) { CaptureHistoryGenerations.Init(0, 8); }
	if (bPreviousVirtualViewsValid.Num() < 8) { bPreviousVirtualViewsValid.Init(false, 8); }
	if (PreviousVirtualViews.Num() < 8) { PreviousVirtualViews.Init(FTransform::Identity, 8); }
	AInteriorPortal* Portal = EndpointIndex == 0 ? BluePortal.Get() : OrangePortal.Get();
	if (IsValid(Portal)) { Portal->EnsureCaptureViews(RecursionLevel + 1); Portal->ResetCaptureHistory(RecursionLevel); }
	++RendererHistoryGeneration;
	CaptureHistoryGenerations[Slot] = RendererHistoryGeneration;
	bPreviousVirtualViewsValid[Slot] = false;
	LastHistoryResetReason = Reason;
	bRendererDiagnosticsDirty = true;
}

void AInteriorPortalSystem::InvalidateRendererHistories(const FString& Reason)
{
	if (PreviousVirtualViews.Num() < 8) { PreviousVirtualViews.Init(FTransform::Identity, 8); }
	if (bPreviousVirtualViewsValid.Num() < 8) { bPreviousVirtualViewsValid.Init(false, 8); }
	if (CaptureHistoryGenerations.Num() < 8) { CaptureHistoryGenerations.Init(0, 8); }
	++RendererHistoryGeneration;
	for (int32 Index=0; Index<8; ++Index) { bPreviousVirtualViewsValid[Index]=false; CaptureHistoryGenerations[Index]=RendererHistoryGeneration; }
	for (AInteriorPortal* Portal : {BluePortal.Get(), OrangePortal.Get()}) { if (IsValid(Portal)) { Portal->ResetCaptureHistories(); } }
	for (FSceneViewStateReference& ViewState : CustomRenderPassViewStates) { if (FSceneViewStateInterface* State=ViewState.GetReference()) { State->ResetViewState(); } }
	LastHistoryResetReason = Reason;
	bRendererDiagnosticsDirty = true;
}

void AInteriorPortalSystem::UpdateFidelityDiagnostics()
{
	FidelityDiagnosticStatus = FString::Printf(TEXT("STEP1A CaptureColorMode=%s CaptureSource=%s CaptureTAA=%s LumenCache=%.2f"),
		CaptureColorMode == EInteriorPortalCaptureColorMode::FinalColorHDR ? TEXT("FinalColorHDR") : TEXT("SceneColorLinear"),
		CaptureColorMode == EInteriorPortalCaptureColorMode::FinalColorHDR ? TEXT("SCS_FinalColorHDR") : TEXT("SCS_SceneColorHDRNoAlpha"),
		bCaptureTemporalAA ? TEXT("ON") : TEXT("OFF"), CaptureLumenSurfaceCacheResolution);
}

void AInteriorPortalSystem::RestoreFidelityDiagnostics() {}

void AInteriorPortalSystem::EndPlay(const EEndPlayReason::Type Reason)
{
	if (MainViewStencilExtension)
	{
		MainViewStencilExtension->SetEnabled(false);
		MainViewStencilExtension->ClearRequest();
		MainViewStencilExtension.Reset();
	}
	for (FSceneViewStateReference& ViewState : CustomRenderPassViewStates) { ViewState.Destroy(); }
	PlayerPresentation->Reset();
	GrabHandle->ReleaseComponent();
	RestoreIgnores();
	if (Character.IsValid()) { Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this); }
	for (UPhysicsConstraintComponent* Constraint : PassageConstraints) { if (IsValid(Constraint)) { Constraint->DestroyComponent(); } }
	PassageConstraints.Reset();
	Super::EndPlay(Reason);
}
