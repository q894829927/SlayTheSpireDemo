#include "InteriorChildCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"
#include "InteriorLightSwitch.h"
#include "InteriorPlayerController.h"
#include "InteriorPortalSystem.h"
#include "InteriorDayNightController.h"
#include "../MapToggle/MapToggle.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AInteriorChildCharacter::AInteriorChildCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(23.0f, 60.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	BaseEyeHeight = 48.0f;
	GetCharacterMovement()->MaxWalkSpeed = 260.0f;
	GetCharacterMovement()->JumpZVelocity = 320.0f;
	GetCharacterMovement()->AirControl = 0.25f;
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	FlashlightRig = CreateDefaultSubobject<USceneComponent>(TEXT("FlashlightRig"));
	FlashlightRig->SetupAttachment(FirstPersonCamera);
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FlashlightRig);
	// Light leaves the front lens; body, lens and beam share the same moving rig.
	Flashlight->SetRelativeLocation(FVector(34.1f, 12.0f, -12.0f));
	Flashlight->SetRelativeRotation(FRotator::ZeroRotator);
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensityUnits(ELightUnits::Lumens);
	Flashlight->SetIntensity(700.0f);
	Flashlight->SetAttenuationRadius(1400.0f);
	Flashlight->SetInnerConeAngle(12.0f);
	Flashlight->SetOuterConeAngle(28.0f);
	Flashlight->SetSourceRadius(2.0f);
	Flashlight->SetSoftSourceRadius(1.0f);
	Flashlight->SetLightColor(FLinearColor(1.0f, 0.96f, 0.86f));
	Flashlight->SetCastShadows(true);
	Flashlight->SetVisibility(false);
	Flashlight->SetVolumetricScatteringIntensity(0.2f);
	Flashlight->SetIndirectLightingIntensity(0.25f);

	FlashlightBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightBody"));
	FlashlightRim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightRim"));
	FlashlightLens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightLens"));
	FlashlightGrip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightGrip"));
	for (UStaticMeshComponent* Part : {FlashlightBody, FlashlightRim, FlashlightLens, FlashlightGrip})
	{
		Part->SetupAttachment(FlashlightRig);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetOnlyOwnerSee(true);
	}
	// Engine cylinder axis is Z; rotate the barrel along the viewing direction.
	FlashlightBody->SetRelativeLocation(FVector(24.0f, 12.0f, -12.0f));
	FlashlightBody->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	FlashlightBody->SetRelativeScale3D(FVector(0.047f, 0.047f, 0.16f));
	FlashlightRim->SetRelativeLocation(FVector(32.0f, 12.0f, -12.0f));
	FlashlightRim->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	FlashlightRim->SetRelativeScale3D(FVector(0.074f, 0.074f, 0.035f));
	FlashlightLens->SetRelativeLocation(FVector(33.8f, 12.0f, -12.0f));
	FlashlightLens->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	FlashlightLens->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.003f));
	FlashlightGrip->SetRelativeLocation(FVector(22.0f, 12.0f, -14.0f));
	FlashlightGrip->SetRelativeScale3D(FVector(0.065f, 0.075f, 0.065f));

	ChildTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildTorso"));
	ChildTorso->SetupAttachment(GetCapsuleComponent());
	ChildTorso->SetRelativeLocation(FVector(-4.0f, 0.0f, -5.0f));
	ChildTorso->SetRelativeScale3D(FVector(0.22f, 0.28f, 0.45f));

	ChildHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildHead"));
	ChildHead->SetupAttachment(GetCapsuleComponent());
	ChildHead->SetRelativeLocation(FVector(0.0f, 0.0f, 42.0f));
	ChildHead->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.25f));
	ChildHead->SetOwnerNoSee(true);
	ChildHead->SetCastHiddenShadow(true);

	ChildLeftHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildLeftHand"));
	ChildLeftHand->SetupAttachment(GetCapsuleComponent());
	ChildLeftHand->SetRelativeLocation(FVector(5.0f, -19.0f, -4.0f));
	ChildLeftHand->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.31f));

	ChildRightHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildRightHand"));
	ChildRightHand->SetupAttachment(GetCapsuleComponent());
	ChildRightHand->SetRelativeLocation(FVector(5.0f, 19.0f, -4.0f));
	ChildRightHand->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.31f));
	ChildRightHand->SetOwnerNoSee(true);
	ChildRightHand->SetCastHiddenShadow(true);

	ChildLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildLeftLeg"));
	ChildLeftLeg->SetupAttachment(GetCapsuleComponent());
	ChildLeftLeg->SetRelativeLocation(FVector(-4.0f, -8.0f, -42.0f));
	ChildLeftLeg->SetRelativeScale3D(FVector(0.13f, 0.11f, 0.34f));
	ChildRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChildRightLeg"));
	ChildRightLeg->SetupAttachment(GetCapsuleComponent());
	ChildRightLeg->SetRelativeLocation(FVector(-4.0f, 8.0f, -42.0f));
	ChildRightLeg->SetRelativeScale3D(FVector(0.13f, 0.11f, 0.34f));

	for (UStaticMeshComponent* BodyPart : {ChildTorso, ChildHead, ChildLeftHand, ChildRightHand, ChildLeftLeg, ChildRightLeg})
	{
		BodyPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BodyPart->SetCanEverAffectNavigation(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		for (UStaticMeshComponent* Part : {FlashlightBody, FlashlightRim, FlashlightLens})
		{
			Part->SetStaticMesh(CylinderMesh.Object);
		}
	}
	if (CubeMesh.Succeeded())
	{
		ChildTorso->SetStaticMesh(CubeMesh.Object);
		ChildLeftHand->SetStaticMesh(CubeMesh.Object);
		ChildRightHand->SetStaticMesh(CubeMesh.Object);
		ChildLeftLeg->SetStaticMesh(CubeMesh.Object);
		ChildRightLeg->SetStaticMesh(CubeMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		ChildHead->SetStaticMesh(SphereMesh.Object);
		FlashlightGrip->SetStaticMesh(SphereMesh.Object);
	}

	// Reuse the sample's existing palette for the deliberately simple child avatar.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Skin(TEXT("/Game/House/InteriorMaterials/M_Interior_Plaster.M_Interior_Plaster"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shirt(TEXT("/Game/House/InteriorMaterials/M_Interior_Blue.M_Interior_Blue"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trousers(TEXT("/Game/House/InteriorMaterials/M_Interior_Black.M_Interior_Black"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Steel(TEXT("/Game/House/InteriorMaterials/M_Interior_Steel.M_Interior_Steel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lens(TEXT("/Game/House/InteriorMaterials/M_Interior_Ivory.M_Interior_Ivory"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cookie(TEXT("/Game/House/Flashlight/M_LF_Flashlight.M_LF_Flashlight"));
	if (Cookie.Succeeded()) { Flashlight->SetLightFunctionMaterial(Cookie.Object); }
	if (Steel.Succeeded()) { FlashlightRim->SetMaterial(0, Steel.Object); }
	if (Lens.Succeeded()) { FlashlightLens->SetMaterial(0, Lens.Object); }
	if (Skin.Succeeded())
	{
		FlashlightGrip->SetMaterial(0, Skin.Object);
		for (UStaticMeshComponent* Part : {ChildHead, ChildLeftHand, ChildRightHand})
		{
			Part->SetMaterial(0, Skin.Object);
		}
	}
	if (Shirt.Succeeded()) { ChildTorso->SetMaterial(0, Shirt.Object); }
	if (Trousers.Succeeded())
	{
		FlashlightBody->SetMaterial(0, Trousers.Object);
		ChildLeftLeg->SetMaterial(0, Trousers.Object);
		ChildRightLeg->SetMaterial(0, Trousers.Object);
	}
}

void AInteriorChildCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Raw key/axis bindings are intentionally local to this pawn; no project Input.ini mappings are needed.
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &AInteriorChildCharacter::InputLookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &AInteriorChildCharacter::InputLookPitch);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AInteriorChildCharacter::InputJumpPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AInteriorChildCharacter::InputJumpReleased);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AInteriorChildCharacter::InputInteract);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AInteriorChildCharacter::InputPrimaryAction);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AInteriorChildCharacter::ToggleDayNight);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AInteriorChildCharacter::InputFlashlight);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AInteriorChildCharacter::InputMapToggle);
	PlayerInputComponent->BindKey(EKeys::M, IE_Released, this, &AInteriorChildCharacter::InputMapToggleReleased);
}

void AInteriorChildCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetFlashlightEnabled(bFlashlightStartsOn);
	ResolveEntranceDoor();
	// One level-owned controller, resolved once. Never select by actor discovery order.
	for (TActorIterator<AInteriorDayNightController> It(GetWorld()); It; ++It)
	{
		if (DayNightController)
		{
			DayNightController = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Interior map has multiple day/night controllers; Q disabled."));
			return;
		}
		DayNightController = *It;
	}
}

void AInteriorChildCharacter::ToggleDayNight()
{
	if (IsValid(DayNightController)) { DayNightController->ToggleDayNight(); }
}

bool AInteriorChildCharacter::IsNight() const
{
	return IsValid(DayNightController) && DayNightController->IsNight();
}

void AInteriorChildCharacter::ToggleFlashlight()
{
	SetFlashlightEnabled(!bFlashlightEnabled);
}

void AInteriorChildCharacter::SetFlashlightEnabled(const bool bEnabled)
{
	bFlashlightEnabled = bEnabled;
	if (IsValid(Flashlight))
	{
		Flashlight->SetVisibility(bFlashlightEnabled);
	}
}

bool AInteriorChildCharacter::IsFlashlightEnabled() const
{
	return bFlashlightEnabled;
}

FVector AInteriorChildCharacter::GetInteriorViewOrigin() const
{
	return IsValid(FirstPersonCamera) ? FirstPersonCamera->GetComponentLocation() : GetActorLocation();
}

FVector AInteriorChildCharacter::GetInteriorViewDirection() const
{
	return IsValid(FirstPersonCamera) ? FirstPersonCamera->GetForwardVector() : GetActorForwardVector();
}

AInteriorLightSwitch* AInteriorChildCharacter::GetInteractionFocus() const
{
	if (!GetWorld() || !IsValid(FirstPersonCamera))
	{
		return nullptr;
	}

	const FVector Start = GetInteriorViewOrigin();
	const FVector End = Start + GetInteriorViewDirection() * AInteriorLightSwitch::InteractionDistance;
	FCollisionQueryParams Params(TEXT("InteriorCharacterInteraction"), true, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return nullptr;
	}

	AInteriorLightSwitch* Switch = Cast<AInteriorLightSwitch>(Hit.GetActor());
	return IsValid(Switch) ? Switch : nullptr;
}

bool AInteriorChildCharacter::IsEntranceDoorActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->GetName().StartsWith(TEXT("Entrance_Door")))
	{
		return true;
	}

#if WITH_EDITOR
	return Actor->GetActorLabel().StartsWith(TEXT("Entrance_Door"));
#else
	return false;
#endif
}

bool AInteriorChildCharacter::HasEntranceDoorPanels(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	bool bHasLeftPanel = false;
	bool bHasRightPanel = false;
	TArray<USceneComponent*> SceneComponents;
	Actor->GetComponents<USceneComponent>(SceneComponents);
	for (const USceneComponent* Component : SceneComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FString ComponentName = Component->GetName();
		bHasLeftPanel |= ComponentName.Contains(TEXT("DoorLeft"));
		bHasRightPanel |= ComponentName.Contains(TEXT("DoorRight"));
	}
	return bHasLeftPanel || bHasRightPanel;
}

bool AInteriorChildCharacter::ResolveEntranceDoor()
{
	if (!GetWorld())
	{
		return false;
	}

	if (IsValid(EntranceDoor) && bEntranceDoorPoseInitialized)
	{
		return true;
	}

	EntranceDoor = nullptr;
	DoorLeft = nullptr;
	DoorRight = nullptr;
	DoorPanel = nullptr;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IsEntranceDoorActor(*It))
		{
			EntranceDoor = *It;
			break;
		}
	}
	if (!IsValid(EntranceDoor))
	{
		// Actor labels are editor-only. The component names provide a runtime-safe
		// fallback for the same authored door in a cooked build.
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (HasEntranceDoorPanels(*It))
			{
				EntranceDoor = *It;
				break;
			}
		}
	}

	if (!IsValid(EntranceDoor))
	{
		return false;
	}

	TArray<USceneComponent*> SceneComponents;
	EntranceDoor->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FString ComponentName = Component->GetName();
		if (!IsValid(DoorLeft) && ComponentName.Contains(TEXT("DoorLeft")))
		{
			DoorLeft = Component;
		}
		else if (!IsValid(DoorRight) && ComponentName.Contains(TEXT("DoorRight")))
		{
			DoorRight = Component;
		}
	}
	if (!IsValid(DoorLeft) && !IsValid(DoorRight))
	{
		// The authored room entrance is a single StaticMeshActor named
		// Entrance_Door, rather than the two-panel corridor Blueprint.
		DoorPanel = Cast<UStaticMeshComponent>(EntranceDoor->GetRootComponent());
		if (!IsValid(DoorPanel))
		{
			for (USceneComponent* Component : SceneComponents)
			{
				if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component))
				{
					DoorPanel = StaticMesh;
					break;
				}
			}
		}
	}

	if (IsValid(DoorLeft))
	{
		DoorLeft->SetMobility(EComponentMobility::Movable);
		DoorLeftClosedRotation = DoorLeft->GetRelativeRotation();
	}
	if (IsValid(DoorRight))
	{
		DoorRight->SetMobility(EComponentMobility::Movable);
		DoorRightClosedRotation = DoorRight->GetRelativeRotation();
	}
	if (IsValid(DoorPanel))
	{
		DoorPanel->SetMobility(EComponentMobility::Movable);
		DoorPanelClosedRotation = DoorPanel->GetRelativeRotation();
	}

	bEntranceDoorPoseInitialized = IsValid(DoorLeft) || IsValid(DoorRight) || IsValid(DoorPanel);
	if (bEntranceDoorPoseInitialized)
	{
		ApplyEntranceDoorPose(EntranceDoorOpenAlpha);
	}
	return bEntranceDoorPoseInitialized;
}

void AInteriorChildCharacter::ApplyEntranceDoorPose(const float OpenAlpha)
{
	const float ClampedAlpha = FMath::Clamp(OpenAlpha, 0.0f, 1.0f);
	constexpr float DoorOpenAngle = 95.0f;

	if (IsValid(DoorLeft))
	{
		FRotator Rotation = DoorLeftClosedRotation;
		Rotation.Yaw += DoorOpenAngle * ClampedAlpha;
		DoorLeft->SetRelativeRotation(Rotation);
	}
	if (IsValid(DoorRight))
	{
		FRotator Rotation = DoorRightClosedRotation;
		Rotation.Yaw -= DoorOpenAngle * ClampedAlpha;
		DoorRight->SetRelativeRotation(Rotation);
	}
	if (IsValid(DoorPanel))
	{
		FRotator Rotation = DoorPanelClosedRotation;
		Rotation.Yaw += DoorOpenAngle * ClampedAlpha;
		DoorPanel->SetRelativeRotation(Rotation);
	}
}

bool AInteriorChildCharacter::TryInteractWithEntranceDoor()
{
	if (!GetWorld() || !IsValid(FirstPersonCamera))
	{
		return false;
	}

	const FVector Start = GetInteriorViewOrigin();
	const FVector End = Start + GetInteriorViewDirection() * 260.0f;
	FCollisionQueryParams Params(TEXT("InteriorDoorInteraction"), true, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
		|| !ResolveEntranceDoor()
		|| Hit.GetActor() != EntranceDoor)
	{
		return false;
	}

	EntranceDoorTargetAlpha = EntranceDoorTargetAlpha > 0.5f ? 0.0f : 1.0f;
	return true;
}

bool AInteriorChildCharacter::TryInteract()
{
	if (TryInteractWithEntranceDoor())
	{
		return true;
	}

	if (AInteriorLightSwitch* Switch = GetInteractionFocus())
	{
		Switch->Toggle();
		return true;
	}
	return false;
}

void AInteriorChildCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFlashlightPose(DeltaSeconds);
	if (bEntranceDoorPoseInitialized && !FMath::IsNearlyEqual(EntranceDoorOpenAlpha, EntranceDoorTargetAlpha))
	{
		EntranceDoorOpenAlpha = FMath::FInterpTo(
			EntranceDoorOpenAlpha,
			EntranceDoorTargetAlpha,
			DeltaSeconds,
			8.0f);
		ApplyEntranceDoorPose(EntranceDoorOpenAlpha);
	}
	// Poll only this local player's held keys. This avoids global axis mappings and
	// stale pressed-state flags when PIE loses focus. Opposing keys cancel naturally.
	const APlayerController* Player = Cast<APlayerController>(GetController());
	if (!Player || !Player->IsLocalController() || Player->IsMoveInputIgnored())
	{
		return;
	}
	const float Forward = float(Player->IsInputKeyDown(EKeys::W)) - float(Player->IsInputKeyDown(EKeys::S));
	const float Right = float(Player->IsInputKeyDown(EKeys::D)) - float(Player->IsInputKeyDown(EKeys::A));
	AddMovementInput(GetActorForwardVector(), Forward);
	AddMovementInput(GetActorRightVector(), Right);
}

void AInteriorChildCharacter::UpdateFlashlightPose(const float DeltaSeconds)
{
	if (!IsLocallyControlled() || !FirstPersonCamera || !FlashlightRig || !GetWorld()) { return; }
	const FQuat View = FirstPersonCamera->GetComponentQuat();
	if (!bFlashlightPoseInitialized)
	{
		SmoothedFlashlightView = View;
		bFlashlightPoseInitialized = true;
	}
	const float Alpha = 1.0f - FMath::Exp(-FMath::Max(1.0f, FlashlightFollowSpeed) * DeltaSeconds);
	SmoothedFlashlightView = FQuat::Slerp(SmoothedFlashlightView, View, Alpha).GetNormalized();
	FRotator Lag = (View.Inverse() * SmoothedFlashlightView).Rotator();
	Lag.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Lag.Pitch), -4.0f, 4.0f);
	Lag.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Lag.Yaw), -4.0f, 4.0f);
	Lag.Roll = 0.0f;
	FlashlightMotionTime = FMath::Fmod(FlashlightMotionTime + DeltaSeconds, 1000.0f);
	const float Walking = GetCharacterMovement()->IsMovingOnGround()
		? FMath::Clamp(GetVelocity().Size2D() / 260.0f, 0.0f, 1.0f) : 0.0f;
	Lag.Pitch += FlashlightSwayAmount * (0.2f * FMath::Sin(FlashlightMotionTime * 1.8f) + Walking * FMath::Sin(FlashlightMotionTime * 9.0f));
	Lag.Yaw += FlashlightSwayAmount * (0.15f * FMath::Sin(FlashlightMotionTime * 1.2f) + 0.5f * Walking * FMath::Sin(FlashlightMotionTime * 4.5f));
	FlashlightRig->SetRelativeRotation(Lag);

	const FVector Eye = FirstPersonCamera->GetComponentLocation();
	const FVector LensOffset = (View * Lag.Quaternion()).RotateVector(FVector(34.1f, 12.0f, -12.0f));
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("FlashlightWallClearance"), false, this);
	float Clearance = 1.0f;
	if (GetWorld()->SweepSingleByChannel(Hit, Eye, Eye + LensOffset, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(4.0f), Params))
	{
		Clearance = FMath::Clamp(Hit.Time - 0.05f, 0.0f, 1.0f);
	}
	// Retract every component together; never leave the beam on the far side of a wall.
	FlashlightRig->SetRelativeLocation(View.UnrotateVector(LensOffset * (Clearance - 1.0f)));
}

void AInteriorChildCharacter::InputLookYaw(const float Value)
{
	AddControllerYawInput(Value * LookSensitivity);
}

void AInteriorChildCharacter::InputLookPitch(const float Value)
{
	AddControllerPitchInput(Value * LookSensitivity * -1.0f);
}

void AInteriorChildCharacter::InputJumpPressed()
{
	Jump();
}

void AInteriorChildCharacter::InputJumpReleased()
{
	StopJumping();
}

void AInteriorChildCharacter::InputInteract()
{
	if (AInteriorPlayerController* Player = Cast<AInteriorPlayerController>(GetController()))
	{
		if (AInteriorPortalSystem* Portals = Player->GetPortalSystem()) { if (Portals->TryGrab(Player)) { return; } }
	}
	TryInteract();
}

void AInteriorChildCharacter::InputFlashlight()
{
	ToggleFlashlight();
}

void AInteriorChildCharacter::InputPrimaryAction()
{
	AInteriorPlayerController* Player = Cast<AInteriorPlayerController>(GetController());
	if (Player && Player->IsPortalGunEquipped()) { Player->FireBluePortal(); }
	else { TryInteract(); }
}

void AInteriorChildCharacter::InputMapToggle()
{
	MapToggle::Toggle(GetWorld());
}

void AInteriorChildCharacter::InputMapToggleReleased()
{
	MapToggle::HandleReleased(GetWorld());
}
