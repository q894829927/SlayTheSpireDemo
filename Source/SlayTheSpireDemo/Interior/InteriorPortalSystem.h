#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "SceneTypes.h"
#include "InteriorPortalSystem.generated.h"

class AInteriorPortal;
class ACharacter;
class APlayerController;
class UPrimitiveComponent;
class UPhysicsConstraintComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPhysicsHandleComponent;
class UInteriorPortalPresentation;
class FInteriorPortalViewExtension;
struct FInteriorPortalRenderRequest;
enum class EInteriorPortalSpikeStatus : uint8;

UENUM(BlueprintType)
enum class EInteriorPortalCrossingState : uint8
{
	Outside, ApproachingEntry, IntersectingAperture, Transferred, ClearingExit
};

UENUM(BlueprintType)
enum class EInteriorPortalRenderClipMode : uint8
{
	NativeClipPlane UMETA(DisplayName="Native SceneCapture Clip Plane"),
	ObliqueFallback UMETA(DisplayName="Oblique Projection (Experimental / Known Broken)")
};

UENUM(BlueprintType)
enum class EInteriorPortalCaptureColorMode : uint8
{
	/** Capture post-process FinalColorHDR. This remains an explicit comparison path. */
	FinalColorHDR UMETA(DisplayName="FinalColorHDR (Capture Post-Process Domain)"),
	/** Capture scene color with capture eye adaptation disabled. */
	SceneColorLinear UMETA(DisplayName="SceneColor Linear (Eye Adaptation Off)")
};

/** Renderer implementation backend. CaptureColorMode remains SceneCapture-only. */
UENUM(BlueprintType)
enum class EInteriorPortalRendererBackend : uint8
{
	SceneCapture UMETA(DisplayName="SceneCapture Fallback"),
	MainViewStencilSpike UMETA(DisplayName="MainView Stencil Feasibility Spike"),
	CustomRenderPassSpike UMETA(DisplayName="Public CustomRenderPass Feasibility Spike"),
	CustomRenderPassCompositionSpike UMETA(DisplayName="Public CustomRenderPass Composition Feasibility Spike")
};

/** One explicit, local-player portal pair. Does not participate in card-battle state. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API AInteriorPortalSystem : public AActor
{
	GENERATED_BODY()
public:
	AInteriorPortalSystem();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals|Presentation")
	TObjectPtr<UInteriorPortalPresentation> PlayerPresentation;
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
	/** Native clip plane is the SceneCapture validation baseline. The oblique path is diagnostic-only after PIE A/B reproduced giant-triangle/wedge distortion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering")
	EInteriorPortalRenderClipMode RenderClipMode = EInteriorPortalRenderClipMode::NativeClipPlane;
	/** Explicit SceneCapture A/B path. SceneColorLinear is the default production candidate because the player view remains the final exposure owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering")
	EInteriorPortalCaptureColorMode CaptureColorMode = EInteriorPortalCaptureColorMode::SceneColorLinear;
	/** STEP 1B is explicit opt-in. SceneCapture remains the default and production fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering")
	EInteriorPortalRendererBackend RendererBackend = EInteriorPortalRendererBackend::SceneCapture;
	/** Small logical-plane offset used only for capture clipping, never for traversal or visual-surface placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering", meta=(ClampMin="0.0", ClampMax="5.0"))
	float ClipPlaneBias = 0.5f;
	/** P2-A: keep the portal capture on a persistent temporal path so Lumen/history behavior can match the player view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|P2 Diagnostics")
	bool bCaptureTemporalAA = true;
	/** SceneCapture-specific Lumen Surface Cache resolution. 0.5 is the production-oriented baseline; raise to 1.0 only for controlled fidelity diagnosis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|P2 Diagnostics", meta=(ClampMin="0.5", ClampMax="1.0"))
	float CaptureLumenSurfaceCacheResolution = 0.5f;
	/** P2-A diagnostic only. Disables global eye adaptation and fixes pre-exposure so direct-vs-portal lighting can be compared without metering changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|P2 Diagnostics")
	bool bExposureIsolationDiagnostic = false;
	/** Pre-exposure value used while the diagnostic is active. 1.0 removes pre-exposure scaling from the comparison. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|P2 Diagnostics", meta=(ClampMin="0.125", ClampMax="8.0"))
	float DiagnosticPreExposureOverride = 1.0f;
	/** P2-A diagnostic only. Allows the legacy PortalViewExposureCorrection parameter to be compared explicitly; production paths leave it disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|P2 Diagnostics")
	bool bCaptureExposureNormalizationDiagnostic = false;
	/** Opt-in STEP1A JSON diagnostics. Disabled by default so periodic file I/O does not contaminate renderer profiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portals|Rendering|Diagnostics")
	bool bEnableRendererDiagnostics = false;
	/** Runtime description of which P2-A diagnostic state is actually active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals|Rendering|P2 Diagnostics")
	FString FidelityDiagnosticStatus;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	FString PlacementMessage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	int32 PlayerCrossings = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals")
	int32 PhysicsCrossings = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portals|Traversal")
	EInteriorPortalCrossingState PlayerCrossingState = EInteriorPortalCrossingState::Outside;
	/** CharacterMovement submove boundary. The returned fraction is safe before any world sweep occurs. */
	double ConstrainCharacterMove(ACharacter* Pawn, const FVector& Delta, FHitResult& OutGateHit);
	void FinishCharacterMove(ACharacter* Pawn);
	bool IsPlayerClearingPortal() const;
	UFUNCTION(BlueprintCallable, Category="Portals")
	bool FirePortal(APlayerController* Player, bool bOrange);
	UFUNCTION(BlueprintCallable, Category="Portals")
	void ResetPortals();
	UFUNCTION(BlueprintPure, Category="Portals")
	bool IsLinked() const;
	/** Register a supported, independently simulated primitive for portal traversal. */
	UFUNCTION(BlueprintCallable, Category="Portals|Travellers")
	bool RegisterPhysicsTraveller(UPrimitiveComponent* Traveller);
	/** Remove a runtime traveller and all of its transient portal state. */
	UFUNCTION(BlueprintCallable, Category="Portals|Travellers")
	bool UnregisterPhysicsTraveller(UPrimitiveComponent* Traveller);
	bool TryGrab(APlayerController* Player);
	/** Called by the local controller immediately before and after its camera update. */
	void UpdateTraversal(APlayerController* Player);
	/** Resolve after each character submove, before CharacterMovement probes the next floor. */
	void UpdateCharacterTraversal(APlayerController* Player);
	void RenderViews(APlayerController* Player);
	/** Explicit mapping used by the renderer and focused Automation. */
	static ESceneCaptureSource GetCaptureSourceForColorMode(EInteriorPortalCaptureColorMode Mode);
	static bool UsesCaptureEyeAdaptation(EInteriorPortalCaptureColorMode Mode);
	static bool UsesSceneCapture(EInteriorPortalRendererBackend Backend);
	static bool UsesMainViewStencil(EInteriorPortalRendererBackend Backend);
	static bool UsesCustomRenderPass(EInteriorPortalRendererBackend Backend);
	static bool UsesCustomRenderPassComposition(EInteriorPortalRendererBackend Backend);
	static bool RequiresRendererHistoryReset(EInteriorPortalRendererBackend PreviousBackend,
		EInteriorPortalRendererBackend NewBackend);
	/**
	 * Returns true when a first-person flashlight clearance sweep hit a portal's
	 * supporting wall through the portal aperture. The caller can ignore that hit
	 * while retaining normal wall retraction everywhere else.
	 */
	bool IsFlashlightTraceThroughPortal(const FHitResult& Hit, const FVector& TraceStart,
		const FVector& TraceEnd, float TraceRadius) const;
	bool ValidatePlacement(const FHitResult& Hit, const FVector& ViewRight, const AInteriorPortal* Endpoint,
		FTransform& OutFrame, FString& OutReason) const;
private:
	bool FitsCharacter(const ACharacter* Character, const FVector& Center, const AInteriorPortal* Portal) const;
	void RestoreIgnores();
	void RecoverCharacterPassage();
	double CharacterNormalExtent(const ACharacter* Pawn, const FTransform& Frame) const;
	void UpdatePhysicsGates();
	void UpdateBodyVisuals();
	void DiscoverTaggedTravellers();
	void RemoveInvalidTravellers();
	void UpdateFidelityDiagnostics();
	void RestoreFidelityDiagnostics();
	void InvalidateRendererHistories(const FString& Reason);
	void InvalidateRendererHistorySlot(int32 EndpointIndex, int32 RecursionLevel, const FString& Reason);
	void WriteMainViewStencilSpikeDiagnostics(const FInteriorPortalRenderRequest* Request,
		EInteriorPortalSpikeStatus Status, const FString& Blocker);
	void WriteCustomRenderPassSpikeDiagnostics(const FInteriorPortalRenderRequest* Request,
		EInteriorPortalSpikeStatus Status, bool bSubmitted, bool bCompositionRequested,
		const FString& Result);
	bool IsBusy() const;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<AInteriorPortal> LastPlayerExit;
	TWeakObjectPtr<AInteriorPortal> PlayerGate;
	FTransform PlayerGateFrame;
	FVector LastSafePlayerCenter = FVector::ZeroVector;
	bool bHasSafePlayerCenter = false;
	uint64 LastPlayerTransferFrame = MAX_uint64;
	TWeakObjectPtr<AInteriorPortal> HeldThroughEntry;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoredSupports;
	FVector PreviousEye = FVector::ZeroVector;
	bool bHasPreviousEye = false;
	TArray<FVector> PreviousBodyPositions;
	TArray<FVector> LastSafeBodyPositions;
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
	bool bExposureDiagnosticsApplied = false;
	bool bSavedEyeAdaptationQuality = false;
	bool bSavedPreExposureOverride = false;
	int32 SavedEyeAdaptationQuality = 0;
	float SavedPreExposureOverride = 0.0f;
	bool bRendererConfigurationInitialized = false;
	bool bRendererBackendInitialized = false;
	EInteriorPortalRendererBackend LastRendererBackend = EInteriorPortalRendererBackend::SceneCapture;
	EInteriorPortalRenderClipMode LastRenderClipMode = EInteriorPortalRenderClipMode::NativeClipPlane;
	EInteriorPortalCaptureColorMode LastCaptureColorMode = EInteriorPortalCaptureColorMode::SceneColorLinear;
	bool bLastCaptureTemporalAA = true;
	float LastCaptureLumenSurfaceCacheResolution = 0.5f;
	int32 LastRendererWidth = 0;
	int32 LastRendererHeight = 0;
	int32 LastRendererDepth = 0;
	FIntPoint LastCustomRenderTargetSize = FIntPoint::ZeroValue;
	bool bWasRendererLinked = false;
	bool bRendererDiagnosticsDirty = true;
	double LastRendererDiagnosticsWriteTime = -1.0;
	uint64 RendererHistoryGeneration = 0;
	FString LastHistoryResetReason = TEXT("Not initialized");
	TArray<FTransform> PreviousVirtualViews;
	TArray<uint8> bPreviousVirtualViewsValid;
	TArray<uint64> CaptureHistoryGenerations;
	bool bPreviousPlayerViewValid = false;
	FTransform PreviousPlayerView;
	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> MainViewStencilExtension;
	/** Persistent state identities are independent from PlayerViewState and SceneCapture states. */
	FSceneViewStateReference CustomRenderPassViewStates[2];
};
