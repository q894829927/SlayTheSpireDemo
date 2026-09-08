#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorLightSwitch.generated.h"

class ALight;
class AActor;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** A small, self-contained wall switch for the interior exploration map. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorLightSwitch : public AActor
{
	GENERATED_BODY()

public:
	AInteriorLightSwitch();

	virtual void BeginPlay() override;

	/** Lights explicitly assigned on the map. Sun and sky lights are never discovered or changed. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Interior Switch")
	TArray<ALight*> ControlledLights;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Interior Switch")
	bool bStartOn = true;

	UFUNCTION(BlueprintCallable, Category = "Interior Switch")
	void Toggle();

	UFUNCTION(BlueprintCallable, Category = "Interior Switch")
	void SetLightsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interior Switch")
	bool AreLightsOn() const { return bLightsOn; }

	/** Validates a view trace and toggles this switch when the supplied actor is close enough. */
	UFUNCTION(BlueprintCallable, Category = "Interior Switch")
	bool TryInteract(AActor* Interactor);

	/** Read-only focus query used by the HUD and by focused interaction tests. */
	UFUNCTION(BlueprintCallable, Category = "Interior Switch")
	bool CanInteractFrom(const FVector& ViewOrigin, const FVector& ViewDirection, AActor* IgnoredActor = nullptr) const;

	static constexpr float InteractionDistance = 180.0f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior Switch")
	UStaticMeshComponent* PanelMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior Switch")
	UStaticMeshComponent* ButtonMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior Switch")
	UBoxComponent* ButtonCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior Switch")
	UPointLightComponent* StateIndicator = nullptr;

private:
	bool bLightsOn = true;

	void UpdateVisualState();
};
