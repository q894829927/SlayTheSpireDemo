#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorPortal.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class UPrimitiveComponent;

/** A level-authored endpoint. Pair ownership, placement and traversal belong to InteriorPortalSystem. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorPortal : public AActor
{
	GENERATED_BODY()
public:
	AInteriorPortal();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FLinearColor PortalColor = FLinearColor(0.015f, 0.32f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal", meta=(ClampMin="40"))
	float HalfWidth = 65;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal", meta=(ClampMin="80"))
	float HalfHeight = 115;
	/** Cosmetic offset of the visible portal plane from the logical aperture plane. Never use this for traversal/query math. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal|Rendering", meta=(ClampMin="0.0", ClampMax="5.0"))
	float SurfaceVisualBias = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	TObjectPtr<UMaterialInterface> PortalMaterial;
	/** Explicit supporting primitive: only this component can be ignored during a valid crossing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal")
	TObjectPtr<UPrimitiveComponent> Support;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal")
	bool bPlaced = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal")
	TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal")
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> RenderTargets;
	/** Actor transform is the single logical portal/aperture frame. Visual bias lives only on Surface. */
	FTransform GetLogicalFrame() const;
	void SetView(UTextureRenderTarget2D* Texture, bool bLinked);
	void RefreshAppearance();
	void EnsureTargets(int32 Width, int32 Height, int32 Depth);
private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};
