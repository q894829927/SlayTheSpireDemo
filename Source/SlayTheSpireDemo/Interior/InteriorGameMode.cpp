#include "InteriorGameMode.h"

#include "InteriorChildCharacter.h"
#include "InteriorHUD.h"
#include "InteriorPlayerController.h"

AInteriorGameMode::AInteriorGameMode()
{
	DefaultPawnClass = AInteriorChildCharacter::StaticClass();
	HUDClass = AInteriorHUD::StaticClass();
	PlayerControllerClass = AInteriorPlayerController::StaticClass();
}
