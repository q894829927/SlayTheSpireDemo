#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorDayNightController.generated.h"

class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;
class UAudioComponent;
class USoundBase;

/** Explicit outdoor-light circuit for this standalone interior sample. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorDayNightController : public AActor
{
	GENERATED_BODY()
public:
	AInteriorDayNightController();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Day Night")
	TObjectPtr<AActor> MoonVisual;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Day Night")
	TObjectPtr<AActor> NightAtmosphere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night")
	FRotator MoonRotation = FRotator(-15.0f, -90.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night|Music")
	TObjectPtr<USoundBase> NightMusic;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day Night|Music")
	TObjectPtr<UAudioComponent> MusicPlayer;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Day Night")
	TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Day Night")
	TObjectPtr<ASkyLight> Sky;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Day Night")
	TObjectPtr<APostProcessVolume> ExposureVolume;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night")
	bool bStartAtNight = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night", meta = (ClampMin = "0"))
	float MoonIntensityLux = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night", meta = (ClampMin = "0"))
	float NightSkyIntensity = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Night")
	float NightExposureEV100 = 4.5f;

	UFUNCTION(BlueprintCallable, Category = "Day Night")
	void ToggleDayNight();
	UFUNCTION(BlueprintCallable, Category = "Day Night")
	void SetNight(bool bEnabled);
	UFUNCTION(BlueprintPure, Category = "Day Night")
	bool IsNight() const { return bNight; }

private:
	bool bNight = false;
	bool bDayCaptured = false;
	float DaySunIntensity = 0.0f;
	FLinearColor DaySunColor = FLinearColor::White;
	FLinearColor DaySunDiskColor = FLinearColor::White;
	FRotator DaySunRotation;
	bool bDayUseTemperature = false;
	float DaySkyIntensity = 1.0f;
	float DayMinExposure = 9.0f;
	float DayMaxExposure = 9.0f;
	bool bDayOverrideMin = false;
	bool bDayOverrideMax = false;
	void CaptureDay();
};
