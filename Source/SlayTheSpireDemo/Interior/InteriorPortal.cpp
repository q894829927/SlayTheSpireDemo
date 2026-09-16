#include "InteriorPortal.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "SceneManagement.h"

AInteriorPortal::AInteriorPortal()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PortalFrame"));
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalSurface"));
	Surface->SetupAttachment(RootComponent);
	// Engine plane lies in XY, normal +Z. Rotate to portal +X.
	Surface->SetRelativeRotation(FRotator(0, 90, -90));
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->SetCanEverAffectNavigation(false);
	Surface->bVisibleInRayTracing = false;
	Surface->bAffectDistanceFieldLighting = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) { Surface->SetStaticMesh(Plane.Object); }
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalCapture"));
	Capture->SetupAttachment(RootComponent);
	CaptureViews.Add(Capture);
	ConfigureCaptureDefaults(Capture);
	PortalCapturePreExposureOwnership = TEXT("Unavailable / Unverified");
}

void AInteriorPortal::ConfigureCaptureDefaults(USceneCaptureComponent2D* InCapture)
{
	if (!InCapture) { return; }
	InCapture->bCaptureEveryFrame = false;
	InCapture->bCaptureOnMovement = false;
	// Persistent rendering state is required for temporal/Lumen history even though captures are issued manually.
	InCapture->bAlwaysPersistRenderingState = true;
	// SceneColorLinear is the explicit system default. The system applies the
	// selected A/B mode again before every capture so authored component state
	// cannot silently select a different path.
	InCapture->CaptureSource = SCS_SceneColorHDRNoAlpha;
	InCapture->bUseCustomProjectionMatrix = true;
	// Portal clipping is owned by the selected render path. Do not stack a second near-plane override on top.
	InCapture->bOverride_CustomNearClippingPlane = false;
	InCapture->ShowFlags.SetMotionBlur(false);
	InCapture->ShowFlags.SetTemporalAA(true);
	InCapture->ShowFlags.SetBloom(false);
	InCapture->ShowFlags.SetEyeAdaptation(false);
}

FTransform AInteriorPortal::GetLogicalFrame() const
{
	return GetActorTransform();
}

void AInteriorPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshAppearance();
}

void AInteriorPortal::BeginPlay()
{
	Super::BeginPlay();
	RefreshAppearance();
}

void AInteriorPortal::RefreshAppearance()
{
	// Keep the actor transform as the logical aperture plane. Only the render surface receives cosmetic depth bias.
	Surface->SetRelativeLocation(FVector(SurfaceVisualBias, 0, 0));
	Surface->SetRelativeScale3D(FVector(HalfWidth / 50.f, HalfHeight / 50.f, 1));
	Surface->SetVisibility(bPlaced);
	if (PortalMaterial && (!DynamicMaterial || DynamicMaterial->Parent != PortalMaterial))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(PortalMaterial, this);
		Surface->SetMaterial(0, DynamicMaterial);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("PortalColor"), PortalColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("PortalViewExposureCorrection"), PortalViewExposureCorrection);
		DynamicMaterial->SetScalarParameterValue(TEXT("PortalCapturePreExposure"), FMath::Max(PortalCapturePreExposure, .000001f));
		DynamicMaterial->SetScalarParameterValue(TEXT("PortalCaptureColorMode"), 0.0f);
	}
}

void AInteriorPortal::SetView(UTextureRenderTarget2D* Texture, bool bLinked, float InputCapturePreExposure,
	const FString& InputCapturePreExposureOwnership, float EffectiveExposureCorrection)
{
	PortalCapturePreExposure = (FMath::IsFinite(InputCapturePreExposure) && InputCapturePreExposure > .000001f)
		? InputCapturePreExposure : 1.0f;
	PortalCapturePreExposureOwnership = InputCapturePreExposureOwnership.IsEmpty()
		? TEXT("Unavailable / Unverified") : InputCapturePreExposureOwnership;
	if (!DynamicMaterial) { return; }
	if (Texture) { DynamicMaterial->SetTextureParameterValue(TEXT("PortalView"), Texture); }
	DynamicMaterial->SetScalarParameterValue(TEXT("Linked"), bLinked && Texture ? 1 : 0);
	DynamicMaterial->SetScalarParameterValue(TEXT("PortalCapturePreExposure"), PortalCapturePreExposure);
	DynamicMaterial->SetScalarParameterValue(TEXT("PortalViewExposureCorrection"),
		EffectiveExposureCorrection >= 0.0f ? EffectiveExposureCorrection : PortalViewExposureCorrection);
}

void AInteriorPortal::SetCaptureColorMode(const bool bFinalColorHDR)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("PortalCaptureColorMode"), bFinalColorHDR ? 1.0f : 0.0f);
	}
}

void AInteriorPortal::EnsureTargets(int32 Width, int32 Height, int32 Depth)
{
	while (RenderTargets.Num() < Depth)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->ClearColor = FLinearColor::Black;
		// Both capture modes require a linear HDR target. CaptureColorMode defines
		// whether the sampled values are scene-referred or already in the capture
		// post-process domain; linear gamma prevents an implicit sRGB decode.
		Target->bForceLinearGamma = true;
		Target->bAutoGenerateMips = false;
		Target->InitAutoFormat(Width, Height);
		RenderTargets.Add(Target);
	}
	for (UTextureRenderTarget2D* Target : RenderTargets)
	{
		if (Target->SizeX != Width || Target->SizeY != Height) { Target->ResizeTarget(Width, Height); }
	}
}

void AInteriorPortal::EnsureTargetForLevel(const int32 RecursionLevel, const int32 Width, const int32 Height)
{
	if (RecursionLevel < 0 || Width <= 0 || Height <= 0)
	{
		return;
	}

	while (RenderTargets.Num() <= RecursionLevel)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->ClearColor = FLinearColor::Black;
		Target->bForceLinearGamma = true;
		Target->bAutoGenerateMips = false;
		Target->InitAutoFormat(Width, Height);
		RenderTargets.Add(Target);
	}

	UTextureRenderTarget2D* Target = RenderTargets[RecursionLevel];
	if (Target && (Target->SizeX != Width || Target->SizeY != Height))
	{
		Target->ResizeTarget(Width, Height);
	}
}

void AInteriorPortal::EnsureCaptureViews(const int32 Depth)
{
	const int32 SafeDepth = FMath::Clamp(Depth, 1, 4);
	while (CaptureViews.Num() < SafeDepth)
	{
		const int32 RecursionLevel = CaptureViews.Num();
		const FName Name(*FString::Printf(TEXT("PortalCaptureDepth%d"), RecursionLevel));
		USceneCaptureComponent2D* NewCapture = NewObject<USceneCaptureComponent2D>(this, Name, RF_Transient);
		NewCapture->SetupAttachment(RootComponent);
		ConfigureCaptureDefaults(NewCapture);
		NewCapture->RegisterComponent();
		CaptureViews.Add(NewCapture);
	}
}

USceneCaptureComponent2D* AInteriorPortal::GetCaptureForDepth(const int32 RecursionLevel) const
{
	return CaptureViews.IsValidIndex(RecursionLevel) ? CaptureViews[RecursionLevel] : nullptr;
}

void AInteriorPortal::ResetCaptureHistories()
{
	for (int32 RecursionLevel = 0; RecursionLevel < CaptureViews.Num(); ++RecursionLevel)
	{
		ResetCaptureHistory(RecursionLevel);
	}
}

void AInteriorPortal::ResetCaptureHistory(const int32 RecursionLevel)
{
	USceneCaptureComponent2D* CaptureView = GetCaptureForDepth(RecursionLevel);
	if (!IsValid(CaptureView)) { return; }
	if (FSceneViewStateInterface* ViewState = CaptureView->GetViewState(0))
	{
		ViewState->ResetViewState();
	}
	// The renderer consumes this flag for the next queued capture and resets it
	// after the scene-capture render command has been built.
	CaptureView->bCameraCutThisFrame = true;
}