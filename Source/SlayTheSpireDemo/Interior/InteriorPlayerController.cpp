#include "InteriorPlayerController.h"

void AInteriorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
}
