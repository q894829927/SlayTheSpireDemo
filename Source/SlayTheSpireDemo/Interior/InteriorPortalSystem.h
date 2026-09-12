#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorPortalSystem.generated.h"

class AInteriorPortal;
class ACharacter;
class APlayerController;
class UPrimitiveComponent;
class UPhysicsConstraintComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPhysicsHandleComponent;

/** One explicit, local-player portal pair. Does not participate in card-battle state. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorPortalSystem : public AActor
{
	GENERATED_BODY()
public:
	AInteriorPortalSystem();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals")
	TObjectPtr<AInteriorPortal> BluePortal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals")
	TObjectPtr<AInteriorPortal> OrangePortal;
	/** Allow-list is authored in the map. Furniture, glazing and doors cannot acquire portals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals")
	TArray<TObjectPtr<UPrimitiveComponent>> PortalSurfaces;
	/** Physics bodies must be independently simulated root components, in explicit processing order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals")
	TArray<TObjectPtr<UPrimitiveComponent>> PhysicsTravellers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals", meta=(ClampMin="1", ClampMax="4"))
	int32 RecursionDepth = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals", meta=(ClampMin="0.25", ClampMax="1.0"))
	float ResolutionScale = 1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	FString PlacementMessage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	int32 PlayerCrossings = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	int32 PhysicsCrossings = 0;
	UFUNCTION(BlueprintCallable, Category="Portals")
	bool FirePortal(APlayerController* Player, bool bOrange);
	UFUNCTION(BlueprintCallable, Category="Portals")
	void ResetPortals();
	UFUNCTION(BlueprintPure, Category="Portals")
	bool IsLinked() const;
	bool TryGrab(APlayerController* Player);
	/** Called by the local controller immediately before and after its camera update. */
	void UpdateTraversal(APlayerController* Player);
	void RenderViews(APlayerController* Player);
	bool ValidatePlacement(const FHitResult& Hit, const FVector& ViewRight, const AInteriorPortal* Endpoint,
		FTransform& OutFrame, FString& OutReason) const;
private:
	bool FitsCharacter(const ACharacter* Character, const FVector& Center, const AInteriorPortal* Portal) const;
	void RestoreIgnores();
	void UpdatePhysicsGates();
	void UpdateBodyVisuals();
	bool IsBusy() const;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<AInteriorPortal> LastPlayerExit;
	TWeakObjectPtr<AInteriorPortal> HeldThroughEntry;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoredSupports;
	FVector PreviousEye = FVector::ZeroVector;
	bool bHasPreviousEye = false;
	TArray<FVector> PreviousBodyPositions;
	TArray<TWeakObjectPtr<AInteriorPortal>> BodyExits;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPhysicsConstraintComponent>> PassageConstraints;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPhysicsHandleComponent> GrabHandle;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyProxies;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ProxyMaterials;
};
