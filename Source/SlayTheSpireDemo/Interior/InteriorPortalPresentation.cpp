#include "InteriorPortalPresentation.h"
#include "InteriorPortal.h"
#include "InteriorPortalMath.h"
#include "InteriorPortalSystem.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

UInteriorPortalPresentation::UInteriorPortalPresentation()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteriorPortalPresentation::Bind(ACharacter* Pawn)
{
	Reset();
	Character = Pawn;
	if (!Pawn) { return; }
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Pawn);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh->ComponentHasTag(TEXT("PortalTravellerVisual"))) { continue; }
		bool bSupported = Mesh->GetNumMaterials() > 0;
		for (int32 Slot=0; Slot<Mesh->GetNumMaterials(); ++Slot)
		{ bSupported &= SliceMaterials.Contains(Mesh->GetMaterial(Slot)); }
		if (!bSupported) { continue; }
		FInteriorPortalVisual& Visual = Visuals.AddDefaulted_GetRef();
		Visual.Source = Mesh;
		Visual.Remote = NewObject<UStaticMeshComponent>(GetOwner());
		Visual.Remote->SetStaticMesh(Mesh->GetStaticMesh());
		Visual.Remote->SetMobility(EComponentMobility::Movable);
		Visual.Remote->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual.Remote->SetCanEverAffectNavigation(false);
		Visual.Remote->SetCastShadow(Mesh->CastShadow);
		Visual.Remote->SetOnlyOwnerSee(false);
		Visual.Remote->SetOwnerNoSee(false);
		Visual.Remote->SetVisibility(false);
		Visual.Remote->RegisterComponent();
		for (int32 Slot=0; Slot<Mesh->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Original = Mesh->GetMaterial(Slot);
			Visual.Originals.Add(Original);
			UMaterialInterface* Slice = SliceMaterials.FindChecked(Original);
			UMaterialInstanceDynamic* SourceMID = UMaterialInstanceDynamic::Create(Slice,this);
			UMaterialInstanceDynamic* RemoteMID = UMaterialInstanceDynamic::Create(Slice,this);
			Mesh->SetMaterial(Slot,SourceMID);
			Visual.Remote->SetMaterial(Slot,RemoteMID);
			Visual.SourceMaterials.Add(SourceMID);
			Visual.RemoteMaterials.Add(RemoteMID);
		}
	}
	SourceLight = Pawn->FindComponentByClass<USpotLightComponent>();
	if (SourceLight) { SourceIntensity = SourceLight->Intensity; }
}

void UInteriorPortalPresentation::Update(AInteriorPortalSystem* System, ACharacter* Pawn, AInteriorPortal* ActiveGate)
{
	if (Character.Get()!=Pawn) { Bind(Pawn); }
	ActiveRemoteVisuals = 0;
	AInteriorPortal* Entry = System->IsLinked() ? ActiveGate : nullptr;
	if (!Entry && Pawn && System->IsLinked())
	{
		// The flashlight can reach the plane before the character capsule starts passage.
		for (AInteriorPortal* Candidate : {System->BluePortal.Get(),System->OrangePortal.Get()})
		{
			const FVector Local = Candidate->GetLogicalFrame().InverseTransformPositionNoScale(Pawn->GetActorLocation());
			if (Local.X>=0 && Local.X<100 && InteriorPortalMath::Inside(Local,Candidate->HalfWidth,Candidate->HalfHeight))
			{ Entry=Candidate; break; }
		}
	}
	for (FInteriorPortalVisual& Visual : Visuals)
	{
		UStaticMeshComponent* Source = Visual.Source;
		bool bSlice = false;
		if (Entry && IsValid(Source) && Source->IsVisible())
		{
			const FVector Local = Entry->GetLogicalFrame().InverseTransformPositionNoScale(Source->Bounds.Origin);
			const double Radius = Source->Bounds.SphereRadius;
			bSlice = Local.X<Radius+1 && InteriorPortalMath::Inside(Local,Entry->HalfWidth+Radius,Entry->HalfHeight+Radius);
		}
		Visual.Remote->SetVisibility(bSlice);
		for (UMaterialInstanceDynamic* MID : Visual.SourceMaterials) { MID->SetScalarParameterValue(TEXT("SliceEnabled"),bSlice?1:0); }
		if (!bSlice) { continue; }
		++ActiveRemoteVisuals;
		AInteriorPortal* Exit = Entry==System->BluePortal ? System->OrangePortal : System->BluePortal;
		const FTransform From = Entry->GetLogicalFrame();
		const FTransform To = Exit->GetLogicalFrame();
		const FQuat Mapping = InteriorPortalMath::Rotation(From,To);
		Visual.Remote->SetWorldTransform(FTransform(Mapping*Source->GetComponentQuat(),
			InteriorPortalMath::Position(Source->GetComponentLocation(),From,To),Source->GetComponentScale()));
		for (UMaterialInstanceDynamic* MID : Visual.SourceMaterials)
		{
			MID->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(From.GetLocation()));
			MID->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(From.GetUnitAxis(EAxis::X)));
		}
		for (UMaterialInstanceDynamic* MID : Visual.RemoteMaterials)
		{
			MID->SetScalarParameterValue(TEXT("SliceEnabled"),1);
			MID->SetVectorParameterValue(TEXT("SliceOrigin"),FLinearColor(To.GetLocation()));
			MID->SetVectorParameterValue(TEXT("SliceNormal"),FLinearColor(To.GetUnitAxis(EAxis::X)));
		}
	}
	UpdateFlashlight(System);
}

bool UInteriorPortalPresentation::PrepareLightChannel(AInteriorPortalSystem* System)
{
	if (LightChannel==INDEX_NONE)
	{
		// Reserve an unused channel. Existing authored lighting channel assignments are preserved.
		bool Used[3] = {true,false,false};
		for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
		{
			TInlineComponentArray<ULightComponent*> Lights(*Actor);
			for (ULightComponent* Light : Lights)
			{ Used[1] |= Light->LightingChannels.bChannel1; Used[2] |= Light->LightingChannels.bChannel2; }
			TInlineComponentArray<UPrimitiveComponent*> Primitives(*Actor);
			for (UPrimitiveComponent* Primitive : Primitives)
			{ Used[1] |= Primitive->LightingChannels.bChannel1; Used[2] |= Primitive->LightingChannels.bChannel2; }
		}
		LightChannel = !Used[2] ? 2 : (!Used[1] ? 1 : INDEX_NONE);
		if (LightChannel==INDEX_NONE) { return false; }
		for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(*Actor);
			for (UPrimitiveComponent* Primitive : Primitives)
			{ ChannelRestores.Add({Primitive,Primitive->LightingChannels}); }
		}
	}
	if (PreviousBlueSupport!=System->BluePortal->Support || PreviousOrangeSupport!=System->OrangePortal->Support || RemoteLights.IsEmpty())
	{
		for (const FChannelRestore& Restore : ChannelRestores)
		{
			if (UPrimitiveComponent* Primitive = Restore.Primitive.Get())
			{
				// The aperture function supplies the hole; its supporting walls must not
				// shadow this light from the virtual source behind the destination plane.
				const bool bReceive = Primitive!=System->BluePortal->Support && Primitive!=System->OrangePortal->Support;
				Primitive->SetLightingChannels(Restore.Channels.bChannel0,
					Restore.Channels.bChannel1 || (LightChannel==1 && bReceive),
					Restore.Channels.bChannel2 || (LightChannel==2 && bReceive));
			}
		}
		PreviousBlueSupport=System->BluePortal->Support; PreviousOrangeSupport=System->OrangePortal->Support;
	}
	return true;
}

void UInteriorPortalPresentation::UpdateFlashlight(AInteriorPortalSystem* System)
{
	ActiveRemoteLights=0;
	for (USpotLightComponent* Light : RemoteLights) { Light->SetVisibility(false); }
	if (!SourceLight) { return; }
	SourceLight->SetIntensity(SourceIntensity);
	if (!System->IsLinked() || !SourceLight->IsVisible() || !FlashlightApertureMaterial)
	{
		RestoreLightChannels();
		return;
	}
	if (!PrepareLightChannel(System)) { return; }
	while (RemoteLights.Num()<2)
	{
		USpotLightComponent* Light=NewObject<USpotLightComponent>(GetOwner());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetVisibility(false);
		Light->SetLightingChannels(false,LightChannel==1,LightChannel==2);
		Light->SetCastShadows(true);
		Light->SetIndirectLightingIntensity(0);
		Light->SetVolumetricScatteringIntensity(0);
		Light->RegisterComponent();
		UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(FlashlightApertureMaterial,this);
		Light->SetLightFunctionMaterial(Material);
		RemoteLights.Add(Light); LightMaterials.Add(Material);
	}
	int32 Index=0;
	for (AInteriorPortal* Entry : {System->BluePortal.Get(),System->OrangePortal.Get()})
	{
		USpotLightComponent* Light=RemoteLights[Index];
		UMaterialInstanceDynamic* Material=LightMaterials[Index++];
		const FTransform From=Entry->GetLogicalFrame();
		const FVector Origin=SourceLight->GetComponentLocation();
		const FVector Local=From.InverseTransformPositionNoScale(Origin);
		const FVector Direction=SourceLight->GetForwardVector();
		const FVector ToPortal=From.GetLocation()-Origin;
		const double Distance=ToPortal.Size();
		const double Radius=FMath::Max(Entry->HalfWidth,Entry->HalfHeight);
		const double AngularRadius=FMath::Asin(FMath::Clamp(Radius/FMath::Max(Distance,Radius),0.0,1.0));
		const bool bConeTouches=FVector::DotProduct(Direction,ToPortal.GetSafeNormal())>
			FMath::Cos(FMath::Min(UE_PI,FMath::DegreesToRadians(SourceLight->OuterConeAngle)+AngularRadius));
		const bool bEmitterCrossing=Local.X<=0 && Local.X>-80 && Character.IsValid()
			&& From.InverseTransformPositionNoScale(Character->GetActorLocation()).X>=0
			&& InteriorPortalMath::Inside(Local,Entry->HalfWidth,Entry->HalfHeight);
		if (Distance>SourceLight->AttenuationRadius || (!bEmitterCrossing && (Local.X<=0 || !bConeTouches))) { continue; }
		AInteriorPortal* Exit=Entry==System->BluePortal ? System->OrangePortal : System->BluePortal;
		const FTransform To=Exit->GetLogicalFrame();
		const FQuat Mapping=InteriorPortalMath::Rotation(From,To);
		const FVector MappedOrigin=InteriorPortalMath::Position(Origin,From,To);
		Light->SetWorldLocationAndRotation(MappedOrigin,Mapping*SourceLight->GetComponentQuat());
		Light->SetIntensityUnits(SourceLight->IntensityUnits);
		Light->SetIntensity(SourceIntensity);
		Light->SetLightColor(SourceLight->GetLightColor());
		Light->SetAttenuationRadius(SourceLight->AttenuationRadius);
		Light->SetInnerConeAngle(SourceLight->InnerConeAngle);
		Light->SetOuterConeAngle(SourceLight->OuterConeAngle);
		Light->SetSourceRadius(SourceLight->SourceRadius);
		Material->SetVectorParameterValue(TEXT("PortalOrigin"),FLinearColor(To.GetLocation()));
		Material->SetVectorParameterValue(TEXT("PortalNormal"),FLinearColor(To.GetUnitAxis(EAxis::X)));
		Material->SetVectorParameterValue(TEXT("PortalRight"),FLinearColor(To.GetUnitAxis(EAxis::Y)));
		Material->SetVectorParameterValue(TEXT("PortalUp"),FLinearColor(To.GetUnitAxis(EAxis::Z)));
		Material->SetVectorParameterValue(TEXT("LightOrigin"),FLinearColor(MappedOrigin));
		Material->SetVectorParameterValue(TEXT("PortalSize"),FLinearColor(Entry->HalfWidth*.94f,Entry->HalfHeight*.94f,0,0));
		Light->SetVisibility(true);
		++ActiveRemoteLights;
		if (bEmitterCrossing) { SourceLight->SetIntensity(0); }
	}
}

void UInteriorPortalPresentation::RestoreLightChannels()
{
	for (const FChannelRestore& Restore : ChannelRestores)
	{
		if (UPrimitiveComponent* Primitive=Restore.Primitive.Get())
		{
			Primitive->SetLightingChannels(Restore.Channels.bChannel0,
				Restore.Channels.bChannel1,Restore.Channels.bChannel2);
		}
	}
	ChannelRestores.Reset();
	LightChannel=INDEX_NONE;
	PreviousBlueSupport.Reset();
	PreviousOrangeSupport.Reset();
}

void UInteriorPortalPresentation::Reset()
{
	for (FInteriorPortalVisual& Visual : Visuals)
	{
		if (IsValid(Visual.Source))
		{
			for (int32 Slot=0; Slot<Visual.Originals.Num(); ++Slot) { Visual.Source->SetMaterial(Slot,Visual.Originals[Slot]); }
		}
		if (IsValid(Visual.Remote)) { Visual.Remote->DestroyComponent(); }
	}
	Visuals.Reset();
	if (IsValid(SourceLight)) { SourceLight->SetIntensity(SourceIntensity); }
	SourceLight=nullptr;
	for (USpotLightComponent* Light : RemoteLights) { if (IsValid(Light)) { Light->DestroyComponent(); } }
	RemoteLights.Reset(); LightMaterials.Reset();
	RestoreLightChannels();
	Character.Reset();
	ActiveRemoteVisuals=0; ActiveRemoteLights=0;
}

void UInteriorPortalPresentation::EndPlay(const EEndPlayReason::Type Reason)
{
	Reset();
	Super::EndPlay(Reason);
}
