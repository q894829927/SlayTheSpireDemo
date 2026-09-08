#include "InteriorLightSwitch.h"

#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace InteriorLightSwitchPrivate
{
	static const FName TraceTag(TEXT("InteriorLightSwitchTrace"));
}

AInteriorLightSwitch::AInteriorLightSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	PanelMesh->SetupAttachment(SceneRoot);
	PanelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PanelMesh->SetRelativeScale3D(FVector(0.02f, 0.18f, 0.24f));

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Button"));
	ButtonMesh->SetupAttachment(SceneRoot);
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ButtonMesh->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
	ButtonMesh->SetRelativeScale3D(FVector(0.02f, 0.10f, 0.13f));

	ButtonCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("ButtonCollision"));
	ButtonCollision->SetupAttachment(SceneRoot);
	ButtonCollision->SetBoxExtent(FVector(3.0f, 10.0f, 13.0f));
	ButtonCollision->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
	ButtonCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ButtonCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ButtonCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	StateIndicator = CreateDefaultSubobject<UPointLightComponent>(TEXT("StateIndicator"));
	StateIndicator->SetupAttachment(SceneRoot);
	StateIndicator->SetRelativeLocation(FVector(4.0f, 0.0f, 8.0f));
	StateIndicator->SetIntensity(0.5f);
	StateIndicator->SetCastShadows(false);
	StateIndicator->SetAttenuationRadius(30.0f);
	StateIndicator->SetLightColor(FLinearColor(0.2f, 1.0f, 0.2f));
	StateIndicator->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PanelMesh->SetStaticMesh(CubeMesh.Object);
		ButtonMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AInteriorLightSwitch::BeginPlay()
{
	Super::BeginPlay();
	SetLightsEnabled(bStartOn);
}

void AInteriorLightSwitch::SetLightsEnabled(const bool bEnabled)
{
	bLightsOn = bEnabled;

	for (ALight* LightActor : ControlledLights)
	{
		if (!IsValid(LightActor))
		{
			continue;
		}

		if (ULightComponent* LightComponent = LightActor->GetLightComponent())
		{
			LightComponent->SetVisibility(bLightsOn, true);
		}
	}

	UpdateVisualState();
}

void AInteriorLightSwitch::Toggle()
{
	SetLightsEnabled(!bLightsOn);
}

bool AInteriorLightSwitch::CanInteractFrom(
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	AActor* IgnoredActor
) const
{
	if (IgnoredActor == this)
	{
		IgnoredActor = nullptr;
	}
	FCollisionQueryParams Params(InteriorLightSwitchPrivate::TraceTag, true);
	if (IsValid(IgnoredActor))
	{
		Params.AddIgnoredActor(IgnoredActor);
	}

	if (!GetWorld() || ViewDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector Direction = ViewDirection.GetSafeNormal();
	const FVector End = ViewOrigin + Direction * InteractionDistance;
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewOrigin, End, ECC_Visibility, Params))
	{
		return false;
	}
	return Hit.GetActor() == this
		&& Hit.GetComponent() == ButtonCollision
		&& FVector::DistSquared(ViewOrigin, Hit.ImpactPoint) <= FMath::Square(InteractionDistance);
}

bool AInteriorLightSwitch::TryInteract(AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return false;
	}

	FVector ViewOrigin;
	FRotator ViewRotation;
	Interactor->GetActorEyesViewPoint(ViewOrigin, ViewRotation);
	if (!CanInteractFrom(ViewOrigin, ViewRotation.Vector(), Interactor))
	{
		return false;
	}

	Toggle();
	return true;
}

void AInteriorLightSwitch::UpdateVisualState()
{
	if (IsValid(StateIndicator))
	{
		StateIndicator->SetVisibility(bLightsOn);
	}

	if (IsValid(ButtonMesh))
	{
		ButtonMesh->SetRelativeRotation(FRotator(bLightsOn ? -12.0f : 12.0f, 0.0f, 0.0f));
	}
}
