#include "InteriorPortal.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = SCS_SceneColorHDRNoAlpha;
	Capture->bUseCustomProjectionMatrix = true;
	// Portal clipping is owned by the selected render path. Do not stack a second near-plane override on top.
	Capture->bOverride_CustomNearClippingPlane = false;
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetBloom(false);
	// Store scene-linear radiance: the player's view must apply exposure exactly once.
	Capture->ShowFlags.SetEyeAdaptation(false);
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
	if (DynamicMaterial) { DynamicMaterial->SetVectorParameterValue(TEXT("PortalColor"), PortalColor); }
}

void AInteriorPortal::SetView(UTextureRenderTarget2D* Texture, bool bLinked)
{
	if (!DynamicMaterial) { return; }
	if (Texture) { DynamicMaterial->SetTextureParameterValue(TEXT("PortalView"), Texture); }
	DynamicMaterial->SetScalarParameterValue(TEXT("Linked"), bLinked && Texture ? 1 : 0);
}

void AInteriorPortal::EnsureTargets(int32 Width, int32 Height, int32 Depth)
{
	while (RenderTargets.Num() < Depth)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->ClearColor = FLinearColor::Black;
		Target->RenderTargetFormat = RTF_RGBA16f;
		Target->bAutoGenerateMips = false;
		Target->InitAutoFormat(Width, Height);
		RenderTargets.Add(Target);
	}
	for (UTextureRenderTarget2D* Target : RenderTargets)
	{
		if (Target->SizeX != Width || Target->SizeY != Height) { Target->ResizeTarget(Width, Height); }
	}
}
