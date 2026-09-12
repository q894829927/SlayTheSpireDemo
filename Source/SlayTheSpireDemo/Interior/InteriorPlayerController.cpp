#include "InteriorPlayerController.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortalCameraManager.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

bool AInteriorPlayerController::IsPortalGunEquipped() const { return bPortalGunEquipped && IsValid(PortalSystem); }

AInteriorPlayerController::AInteriorPlayerController()
{
	PlayerCameraManagerClass = AInteriorPortalCameraManager::StaticClass();
}

void AInteriorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
	for (TActorIterator<AInteriorPortalSystem> It(GetWorld()); It; ++It)
	{
		if (PortalSystem)
		{
			PortalSystem = nullptr;
			UE_LOG(LogTemp, Error, TEXT("Multiple InteriorPortalSystems: portal input disabled."));
			break;
		}
		PortalSystem = *It;
	}
}

void AInteriorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AInteriorPlayerController::FireOrangePortal);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AInteriorPlayerController::TogglePortalGun);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AInteriorPlayerController::ClearPortals);
}

void AInteriorPlayerController::FireBluePortal()
{
	if (IsPortalGunEquipped()) { PortalSystem->FirePortal(this, false); }
}
void AInteriorPlayerController::FireOrangePortal()
{
	if (IsPortalGunEquipped()) { PortalSystem->FirePortal(this, true); }
}
void AInteriorPlayerController::TogglePortalGun() { bPortalGunEquipped = !bPortalGunEquipped; }
void AInteriorPlayerController::ClearPortals()
{
	if (IsPortalGunEquipped()) { PortalSystem->ResetPortals(); }
}
void AInteriorPlayerController::UpdateCameraManager(float DeltaSeconds)
{
	if (IsValid(PortalSystem)) { PortalSystem->UpdateTraversal(this); }
	if (PortalCamera.bActive) { SetControlRotation(PortalCamera.Orientation.Rotator()); }
	Super::UpdateCameraManager(DeltaSeconds);
	if (IsValid(PortalSystem)) { PortalSystem->RenderViews(this); }
}

void AInteriorPlayerController::ApplyPortalView(const FQuat& Mapping)
{
	PortalCamera.Transfer(Mapping,GetControlRotation().Quaternion());
	SetControlRotation(PortalCamera.Orientation.Rotator());
}

void AInteriorPlayerController::UpdateRotation(float DeltaSeconds)
{
	if (!PortalCamera.bActive) { Super::UpdateRotation(DeltaSeconds); return; }
	PortalCamera.ApplyInput(RotationInput);
	// Recovery starts only after the whole capsule clears; transfer-frame roll is preserved.
	if (!IsValid(PortalSystem) || !PortalSystem->IsPlayerClearingPortal()) { PortalCamera.RecoverHorizon(DeltaSeconds); }
	SetControlRotation(PortalCamera.Orientation.Rotator());
	if (APawn* ControlledPawn=GetPawn()) { ControlledPawn->FaceRotation(FRotator(0,GetControlRotation().Yaw,0),DeltaSeconds); }
}
