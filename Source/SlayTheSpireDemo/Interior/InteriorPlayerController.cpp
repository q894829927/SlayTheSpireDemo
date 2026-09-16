#include "InteriorPlayerController.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortalCameraManager.h"
#include "InteriorPortalFullSceneViewSubsystem.h"
#include "InteriorPortalPresentation.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
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

	const bool bUseFullFidelity = IsValid(PortalSystem)
		&& PortalSystem->bUseFullFidelityRenderer
		&& AInteriorPortalSystem::UsesSceneCapture(PortalSystem->RendererBackend);
	SetFullFidelityRendererActive(bUseFullFidelity);
}

void AInteriorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetFullFidelityRendererActive(false);
	Super::EndPlay(EndPlayReason);
}

void AInteriorPlayerController::SetFullFidelityRendererActive(const bool bEnable)
{
	if (bFullFidelityRendererActive == bEnable)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		bFullFidelityRendererActive = false;
		return;
	}

	if (bEnable)
	{
		GEngine->Exec(World, TEXT("portal.StartFullFidelityRenderer"));
		bFullFidelityRendererActive = true;
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: lifecycle START from InteriorPlayerController; legacy RenderViews bypassed."));
	}
	else
	{
		GEngine->Exec(World, TEXT("portal.StopFullFidelityRenderer"));
		bFullFidelityRendererActive = false;
		UE_LOG(LogTemp, Display,
			TEXT("PortalFullFidelityRenderer: lifecycle STOP from InteriorPlayerController."));
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
	if (IsValid(PortalSystem))
	{
		const bool bUseFullFidelity = PortalSystem->bUseFullFidelityRenderer
			&& AInteriorPortalSystem::UsesSceneCapture(PortalSystem->RendererBackend);
		SetFullFidelityRendererActive(bUseFullFidelity);

		if (bUseFullFidelity)
		{
			// Full-fidelity production candidate owns remote rendering. Keep only
			// gameplay/presentation-side local visuals here; do not run legacy
			// SceneCapture RenderViews or the older full-scene-view spike in parallel.
			if (PortalSystem->PlayerPresentation)
			{
				PortalSystem->PlayerPresentation->Update(
					PortalSystem,
					Cast<ACharacter>(GetPawn()),
					PortalSystem->GetPlayerGate());
			}
			return;
		}

		PortalSystem->RenderViews(this);
		if (UWorld* World = GetWorld())
		{
			if (UInteriorPortalFullSceneViewSubsystem* FullSceneView = World->GetSubsystem<UInteriorPortalFullSceneViewSubsystem>())
			{
				FullSceneView->Render(this, PortalSystem);
			}
		}
	}
	else
	{
		// Runtime destruction/replacement of the system must not leave endpoint
		// ViewStates or transient render targets alive until controller teardown.
		SetFullFidelityRendererActive(false);
	}
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
