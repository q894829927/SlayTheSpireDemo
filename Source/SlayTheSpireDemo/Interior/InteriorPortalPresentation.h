#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteriorPortalPresentation.generated.h"

class ACharacter;
class AInteriorPortal;
class AInteriorPortalSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USpotLightComponent;
class UStaticMeshComponent;

USTRUCT()
struct FInteriorPortalVisual
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Source;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Remote;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Originals;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SourceMaterials;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> RemoteMaterials;
};

/** Local-player visual halves and the player's direct flashlight continuation. No gameplay authority. */
UCLASS()
class SLAYTHESPIREDEMO_API UInteriorPortalPresentation : public UActorComponent
{
	GENERATED_BODY()
public:
	UInteriorPortalPresentation();
	UPROPERTY(EditAnywhere, Category="Portals|Player Visuals")
	TMap<TObjectPtr<UMaterialInterface>,TObjectPtr<UMaterialInterface>> SliceMaterials;
	UPROPERTY(EditAnywhere, Category="Portals|Flashlight")
	TObjectPtr<UMaterialInterface> FlashlightApertureMaterial;
	UPROPERTY(VisibleAnywhere, Category="Portals|Player Visuals")
	int32 ActiveRemoteVisuals = 0;
	UPROPERTY(VisibleAnywhere, Category="Portals|Flashlight")
	int32 ActiveRemoteLights = 0;
	void Update(AInteriorPortalSystem* System, ACharacter* Pawn, AInteriorPortal* ActiveGate);
	void Reset();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Bind(ACharacter* Pawn);
	bool PrepareLightChannel(AInteriorPortalSystem* System);
	void UpdateFlashlight(AInteriorPortalSystem* System);
	void RestoreLightChannels();
	TWeakObjectPtr<ACharacter> Character;
	UPROPERTY(Transient) TArray<FInteriorPortalVisual> Visuals;
	UPROPERTY(Transient) TObjectPtr<USpotLightComponent> SourceLight;
	UPROPERTY(Transient) TArray<TObjectPtr<USpotLightComponent>> RemoteLights;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LightMaterials;
	struct FChannelRestore { TWeakObjectPtr<UPrimitiveComponent> Primitive; FLightingChannels Channels; };
	TArray<FChannelRestore> ChannelRestores;
	TWeakObjectPtr<UPrimitiveComponent> PreviousBlueSupport;
	TWeakObjectPtr<UPrimitiveComponent> PreviousOrangeSupport;
	int32 LightChannel = INDEX_NONE;
	float SourceIntensity = 0;
};
