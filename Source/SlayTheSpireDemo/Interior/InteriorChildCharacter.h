#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InteriorChildCharacter.generated.h"

class AInteriorLightSwitch;
class AInteriorDayNightController;
class AActor;
class UCameraComponent;
class USceneComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/** Compact first-person child avatar used only by the interior exploration map. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorChildCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AInteriorChildCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Interior Interaction")
	void ToggleDayNight();
	UFUNCTION(BlueprintPure, Category = "Interior Interaction")
	bool IsNight() const;
	UFUNCTION(BlueprintCallable, Category = "Interior Flashlight")
	void ToggleFlashlight();
	UFUNCTION(BlueprintCallable, Category = "Interior Flashlight")
	void SetFlashlightEnabled(bool bEnabled);
	UFUNCTION(BlueprintPure, Category = "Interior Flashlight")
	bool IsFlashlightEnabled() const;

	/** Returns the switch hit by the current camera view, or nullptr when obstructed/out of range. */
	UFUNCTION(BlueprintPure, Category = "Interior Interaction")
	AInteriorLightSwitch* GetInteractionFocus() const;

	/** Toggles the currently focused switch. */
	UFUNCTION(BlueprintCallable, Category = "Interior Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "Interior Interaction")
	FVector GetInteriorViewOrigin() const;

	UFUNCTION(BlueprintPure, Category = "Interior Interaction")
	FVector GetInteriorViewDirection() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior Movement")
	float LookSensitivity = 1.0f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UCameraComponent* FirstPersonCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildTorso = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildHead = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildLeftHand = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildRightHand = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildLeftLeg = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior")
	UStaticMeshComponent* ChildRightLeg = nullptr;

	/** A camera-aligned spot light used as the child's handheld flashlight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	USpotLightComponent* Flashlight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	UStaticMeshComponent* FlashlightBody = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	UStaticMeshComponent* FlashlightRim = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	UStaticMeshComponent* FlashlightLens = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	UStaticMeshComponent* FlashlightGrip = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Flashlight")
	bool bFlashlightStartsOn = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Flashlight", meta = (ClampMin = "0", ClampMax = "2"))
	float FlashlightSwayAmount = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Flashlight", meta = (ClampMin = "1", ClampMax = "30"))
	float FlashlightFollowSpeed = 14.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior|Flashlight")
	USceneComponent* FlashlightRig = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<AInteriorDayNightController> DayNightController;
	UPROPERTY(Transient)
	TObjectPtr<AActor> EntranceDoor = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> DoorLeft = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> DoorRight = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> DoorPanel = nullptr;

	bool ResolveEntranceDoor();
	bool IsEntranceDoorActor(const AActor* Actor) const;
	bool HasEntranceDoorPanels(const AActor* Actor) const;
	bool TryInteractWithEntranceDoor();
	void ApplyEntranceDoorPose(float OpenAlpha);
	FRotator DoorLeftClosedRotation = FRotator::ZeroRotator;
	FRotator DoorRightClosedRotation = FRotator::ZeroRotator;
	FRotator DoorPanelClosedRotation = FRotator::ZeroRotator;
	float EntranceDoorOpenAlpha = 0.0f;
	float EntranceDoorTargetAlpha = 0.0f;
	bool bEntranceDoorPoseInitialized = false;

	void InputLookYaw(float Value);
	void InputLookPitch(float Value);
	void InputJumpPressed();
	void InputJumpReleased();
	void InputInteract();
	void InputPrimaryAction();
	void InputFlashlight();
	void InputMapToggle();
	void InputMapToggleReleased();
	bool bFlashlightEnabled = false;
	void UpdateFlashlightPose(float DeltaSeconds);
	FQuat SmoothedFlashlightView = FQuat::Identity;
	float FlashlightMotionTime = 0.0f;
	bool bFlashlightPoseInitialized = false;
};
