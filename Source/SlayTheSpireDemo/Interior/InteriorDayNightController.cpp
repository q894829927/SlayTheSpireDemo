#include "InteriorDayNightController.h"

#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"

AInteriorDayNightController::AInteriorDayNightController()
{
	PrimaryActorTick.bCanEverTick = false;
	MusicPlayer = CreateDefaultSubobject<UAudioComponent>(TEXT("NightMusicPlayer"));
	SetRootComponent(MusicPlayer);
	MusicPlayer->bAutoActivate = false;
	MusicPlayer->bAllowSpatialization = false;
	MusicPlayer->bIsUISound = true;
	MusicPlayer->SetVolumeMultiplier(0.35f);
}

void AInteriorDayNightController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MusicPlayer->Stop();
	Super::EndPlay(EndPlayReason);
}

void AInteriorDayNightController::BeginPlay()
{
	Super::BeginPlay();
	SetNight(bStartAtNight);
}

void AInteriorDayNightController::CaptureDay()
{
	if (bDayCaptured) { return; }
	if (IsValid(Sun))
	{
		DaySunIntensity = Sun->GetLightComponent()->Intensity;
		DaySunColor = Sun->GetLightComponent()->GetLightColor();
		DaySunDiskColor = Sun->GetComponent()->AtmosphereSunDiskColorScale;
		DaySunRotation = Sun->GetActorRotation();
		bDayUseTemperature = Sun->GetLightComponent()->bUseTemperature;
	}
	if (IsValid(Sky)) { DaySkyIntensity = Sky->GetLightComponent()->Intensity; }
	if (IsValid(ExposureVolume))
	{
		const FPostProcessSettings& Settings = ExposureVolume->Settings;
		DayMinExposure = Settings.AutoExposureMinBrightness;
		DayMaxExposure = Settings.AutoExposureMaxBrightness;
		bDayOverrideMin = Settings.bOverride_AutoExposureMinBrightness;
		bDayOverrideMax = Settings.bOverride_AutoExposureMaxBrightness;
	}
	bDayCaptured = true;
}

void AInteriorDayNightController::SetNight(const bool bEnabled)
{
	// Cache the authored daytime values once: repeated toggles never accumulate changes.
	CaptureDay();
	bNight = bEnabled;
	if (IsValid(Sun))
	{
		Sun->GetLightComponent()->SetIntensity(bNight ? MoonIntensityLux : DaySunIntensity);
		Sun->GetLightComponent()->SetLightColor(bNight ? FLinearColor(0.45f, 0.60f, 1.0f) : DaySunColor);
		Sun->GetLightComponent()->SetUseTemperature(bNight ? false : bDayUseTemperature);
		Sun->SetActorRotation(bNight ? MoonRotation : DaySunRotation);
		// The authored lunar mesh supplies the disk; avoid a second atmosphere disk behind it.
		Sun->GetComponent()->SetAtmosphereSunDiskColorScale(bNight && IsValid(MoonVisual) ? FLinearColor::Black : DaySunDiskColor);
	}
	if (IsValid(Sky))
	{
		Sky->GetLightComponent()->SetIntensity(bNight ? NightSkyIntensity : DaySkyIntensity);
		Sky->GetLightComponent()->RecaptureSky();
	}
	if (IsValid(ExposureVolume))
	{
		FPostProcessSettings& Settings = ExposureVolume->Settings;
		Settings.bOverride_AutoExposureMinBrightness = bNight || bDayOverrideMin;
		Settings.bOverride_AutoExposureMaxBrightness = bNight || bDayOverrideMax;
		Settings.AutoExposureMinBrightness = bNight ? NightExposureEV100 : DayMinExposure;
		Settings.AutoExposureMaxBrightness = bNight ? NightExposureEV100 : DayMaxExposure;
	}
	// Indoor lights belong exclusively to the wall switch; their current state is preserved.
	if (IsValid(MoonVisual)) { MoonVisual->SetActorHiddenInGame(!bNight); }
	if (IsValid(NightAtmosphere)) { NightAtmosphere->SetActorHiddenInGame(!bNight); }
	if (!bNight || !IsValid(NightMusic))
	{
		MusicPlayer->Stop();
	}
	else
	{
		if (MusicPlayer->Sound != NightMusic) { MusicPlayer->SetSound(NightMusic); }
		if (!MusicPlayer->IsPlaying()) { MusicPlayer->FadeIn(1.0f, 0.35f); }
	}
}

void AInteriorDayNightController::ToggleDayNight()
{
	SetNight(!bNight);
}
