#include "MapToggle.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName InteriorMap(TEXT("/Game/House/L_Interior_LivingKitchen"));
	const FName ReturnMap(TEXT("/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel"));
	TWeakObjectPtr<UGameInstance> ToggleGameInstance;
	TWeakObjectPtr<UWorld> ToggleWorld;
	bool bToggleKeyDown = false;

	bool IsInteriorMap(const UWorld* World)
	{
		return World
			&& World->GetMapName().EndsWith(TEXT("L_Interior_LivingKitchen"), ESearchCase::IgnoreCase);
	}

	bool IsNewGameInstance(const UWorld* World)
	{
		return World && ToggleGameInstance.Get() != World->GetGameInstance();
	}
}

void MapToggle::Toggle(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (IsNewGameInstance(World) || ToggleWorld.Get() != World)
	{
		ToggleGameInstance = World->GetGameInstance();
		ToggleWorld = World;
		bToggleKeyDown = false;
	}

	if (bToggleKeyDown)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MapToggle] Ignoring repeated M while the key is held"));
		return;
	}

	bToggleKeyDown = true;

	const FName TargetMap = IsInteriorMap(World) ? ReturnMap : InteriorMap;
	UE_LOG(LogTemp, Log, TEXT("[MapToggle] Opening %s"), *TargetMap.ToString());
	UGameplayStatics::OpenLevel(World, TargetMap);
}

void MapToggle::HandleReleased(UWorld* World)
{
	if (World && ToggleGameInstance.Get() == World->GetGameInstance())
	{
		bToggleKeyDown = false;
	}
}
