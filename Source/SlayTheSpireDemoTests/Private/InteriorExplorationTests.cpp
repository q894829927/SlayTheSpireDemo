#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorChildCharacter.h"
#include "Interior/InteriorLightSwitch.h"
#include "Interior/InteriorDayNightController.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/InputComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"

namespace InteriorTests
{
	struct FWorldFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		~FWorldFixture() { if (World) { World->DestroyWorld(false); } }

		template<typename T>
		T* Spawn(const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World ? World->SpawnActor<T>(T::StaticClass(), Location, Rotation, Params) : nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorFlashlightTest,
	"SlayTheSpireDemo.Interior.Flashlight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorFlashlightTest::RunTest(const FString& Parameters)
{
	InteriorTests::FWorldFixture Fixture;
	AInteriorChildCharacter* Child = Fixture.Spawn<AInteriorChildCharacter>();
	if (!TestNotNull(TEXT("Child"), Child)) { return false; }
	USpotLightComponent* Light = Child->FindComponentByClass<USpotLightComponent>();
	if (!TestNotNull(TEXT("Flashlight"), Light)) { return false; }
	TestFalse(TEXT("Starts off"), Child->IsFlashlightEnabled());
	UInputComponent* Input = NewObject<UInputComponent>(Child);
	Child->SetupPlayerInputComponent(Input);
	int32 Bindings = 0;
	int32 MapToggleBindings = 0;
	int32 MapToggleReleaseBindings = 0;
	for (const FInputKeyBinding& Binding : Input->KeyBindings)
	{
		if (Binding.Chord.Key.GetFName() == FName(TEXT("M")) && Binding.KeyEvent == IE_Pressed)
		{
			++MapToggleBindings;
		}
		if (Binding.Chord.Key.GetFName() == FName(TEXT("M")) && Binding.KeyEvent == IE_Released)
		{
			++MapToggleReleaseBindings;
		}
		if (Binding.Chord.Key.GetFName() != FName(TEXT("F")) || Binding.KeyEvent != IE_Pressed) { continue; }
		++Bindings;
		Binding.KeyDelegate.Execute(Binding.Chord.Key);
		TestTrue(TEXT("F switches state and light on"), Child->IsFlashlightEnabled() && Light->IsVisible());
		Child->SetFlashlightEnabled(true);
		Child->ToggleDayNight();
		TestTrue(TEXT("Repeated enable and day/night are independent"), Light->IsVisible());
		Binding.KeyDelegate.Execute(Binding.Chord.Key);
		TestFalse(TEXT("Second F switches state off"), Child->IsFlashlightEnabled());
		TestFalse(TEXT("Second F hides light"), Light->IsVisible());
	}
	TestEqual(TEXT("Exactly one F press binding"), Bindings, 1);
	TestEqual(TEXT("Exactly one M map-toggle binding"), MapToggleBindings, 1);
	TestEqual(TEXT("Exactly one M map-toggle release binding"), MapToggleReleaseBindings, 1);
	TestNotNull(TEXT("Optical cookie assigned"), Light->LightFunctionMaterial.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorLightCircuitTest,
	"SlayTheSpireDemo.Interior.LightCircuit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorLightCircuitTest::RunTest(const FString& Parameters)
{
	InteriorTests::FWorldFixture Fixture;
	AInteriorLightSwitch* Switch = Fixture.Spawn<AInteriorLightSwitch>();
	APointLight* Lamp = Fixture.Spawn<APointLight>();
	APointLight* OtherLamp = Fixture.Spawn<APointLight>();
	ADirectionalLight* Sun = Fixture.Spawn<ADirectionalLight>();
	if (!TestNotNull(TEXT("Switch"), Switch) || !Lamp || !OtherLamp || !Sun) { return false; }
	Lamp->GetLightComponent()->SetIntensity(950.0f);
	OtherLamp->GetLightComponent()->SetVisibility(true);
	Sun->GetLightComponent()->SetVisibility(true);
	Switch->ControlledLights = { Lamp, nullptr };
	Switch->bStartOn = false;
	Switch->DispatchBeginPlay();
	TestFalse(TEXT("Initial off state applied"), Switch->AreLightsOn());
	TestFalse(TEXT("Assigned lamp starts off"), Lamp->GetLightComponent()->IsVisible());
	Switch->Toggle();
	TestTrue(TEXT("First toggle turns circuit on"), Switch->AreLightsOn());
	TestTrue(TEXT("Assigned lamp restored"), Lamp->GetLightComponent()->IsVisible());
	Switch->Toggle();
	TestFalse(TEXT("Second toggle returns to initial state"), Switch->AreLightsOn());
	TestFalse(TEXT("Assigned lamp off again"), Lamp->GetLightComponent()->IsVisible());
	TestEqual(TEXT("Brightness is preserved"), Lamp->GetLightComponent()->Intensity, 950.0f);
	TestTrue(TEXT("Sun is not in the circuit"), Sun->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Unassigned indoor lamp is untouched"), OtherLamp->GetLightComponent()->IsVisible());
	Lamp->Destroy();
	Switch->Toggle();
	TestTrue(TEXT("Destroyed/null references do not prevent state change"), Switch->AreLightsOn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorInteractionTest,
	"SlayTheSpireDemo.Interior.InteractionRangeAndOcclusion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorInteractionTest::RunTest(const FString& Parameters)
{
	InteriorTests::FWorldFixture Fixture;
	AInteriorLightSwitch* Switch = Fixture.Spawn<AInteriorLightSwitch>(FVector(0, 0, 48));
	AInteriorChildCharacter* Child = Fixture.Spawn<AInteriorChildCharacter>(FVector(120, 0, 0), FRotator(0, 180, 0));
	if (!TestNotNull(TEXT("Switch"), Switch) || !TestNotNull(TEXT("Child"), Child)) { return false; }
	TestTrue(TEXT("Unobstructed nearby camera can reach switch"), Switch->CanInteractFrom(FVector(120, 0, 48), FVector(-1, 0, 0)));
	TestFalse(TEXT("Looking away is rejected"), Switch->CanInteractFrom(FVector(120, 0, 48), FVector(1, 0, 0)));
	TestFalse(TEXT("Beyond 180 cm is rejected"), Switch->CanInteractFrom(FVector(220, 0, 48), FVector(-1, 0, 0)));
	TestFalse(TEXT("Zero direction is rejected"), Switch->CanInteractFrom(FVector(120, 0, 48), FVector::ZeroVector));
	TestTrue(TEXT("Child camera identifies switch"), Child->GetInteractionFocus() == Switch);
	TestTrue(TEXT("Child interaction changes state"), Child->TryInteract());
	TestFalse(TEXT("Interaction switched off"), Switch->AreLightsOn());
	TestTrue(TEXT("Second child interaction succeeds"), Child->TryInteract());
	TestTrue(TEXT("Interaction restored on"), Switch->AreLightsOn());

	AActor* Wall = Fixture.Spawn<AActor>();
	UBoxComponent* Blocker = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(5, 50, 70));
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Blocker->RegisterComponent();
	Wall->SetActorLocation(FVector(60, 0, 48));
	TestFalse(TEXT("Wall blocks direct query"), Switch->CanInteractFrom(FVector(120, 0, 48), FVector(-1, 0, 0)));
	TestNull(TEXT("Wall removes focus"), Child->GetInteractionFocus());
	TestFalse(TEXT("Cannot interact through wall"), Child->TryInteract());
	TestTrue(TEXT("Blocked attempt does not change circuit"), Switch->AreLightsOn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorDayNightTest,
	"SlayTheSpireDemo.Interior.DayNight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorDayNightTest::RunTest(const FString& Parameters)
{
	InteriorTests::FWorldFixture Fixture;
	AInteriorDayNightController* Cycle = Fixture.Spawn<AInteriorDayNightController>();
	ADirectionalLight* Sun = Fixture.Spawn<ADirectionalLight>();
	ASkyLight* Sky = Fixture.Spawn<ASkyLight>();
	APostProcessVolume* Exposure = Fixture.Spawn<APostProcessVolume>();
	APointLight* Lamp = Fixture.Spawn<APointLight>();
	AInteriorLightSwitch* Switch = Fixture.Spawn<AInteriorLightSwitch>();
	AInteriorChildCharacter* Child = Fixture.Spawn<AInteriorChildCharacter>();
	if (!Cycle || !Sun || !Sky || !Exposure || !Lamp || !Switch || !Child) { return false; }
	Sun->GetLightComponent()->SetIntensity(65000.0f);
	Sun->GetLightComponent()->SetLightColor(FLinearColor::White);
	Sky->GetLightComponent()->SetIntensity(1.0f);
	Exposure->Settings.AutoExposureMinBrightness = 9.0f;
	Exposure->Settings.AutoExposureMaxBrightness = 10.0f;
	Exposure->Settings.bOverride_AutoExposureMinBrightness = false;
	Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
	Cycle->Sun = Sun;
	Cycle->Sky = Sky;
	Cycle->ExposureVolume = Exposure;
	AActor* Moon = Fixture.Spawn<AActor>();
	AActor* Fog = Fixture.Spawn<AActor>();
	Cycle->MoonVisual = Moon;
	Cycle->NightAtmosphere = Fog;
	Sun->SetActorRotation(FRotator(-32, -65, 0));
	Sun->GetLightComponent()->SetUseTemperature(true);
	Switch->ControlledLights = {Lamp};
	Switch->SetLightsEnabled(false);
	Child->DispatchBeginPlay();
	Child->ToggleDayNight();
	TestTrue(TEXT("Character request reaches map controller"), Cycle->IsNight());
	TestTrue(TEXT("HUD query reflects night"), Child->IsNight());
	TestFalse(TEXT("Moon visible at night"), Moon->IsHidden());
	TestFalse(TEXT("Night atmosphere visible"), Fog->IsHidden());
	TestTrue(TEXT("No duplicate lunar disk"), Sun->GetComponent()->AtmosphereSunDiskColorScale.Equals(FLinearColor::Black));
	TestTrue(TEXT("Moon direction applied"), Sun->GetActorRotation().Equals(Cycle->MoonRotation));
	TestFalse(TEXT("Moon tint is not multiplied by daylight temperature"), bool(Sun->GetLightComponent()->bUseTemperature));
	TestEqual(TEXT("Moon lighting applied"), Sun->GetLightComponent()->Intensity, Cycle->MoonIntensityLux);
	TestEqual(TEXT("Night sky applied"), Sky->GetLightComponent()->Intensity, Cycle->NightSkyIntensity);
	TestEqual(TEXT("Night exposure applied"), Exposure->Settings.AutoExposureMinBrightness, Cycle->NightExposureEV100);
	TestFalse(TEXT("Night does not switch indoor lamp on"), Lamp->GetLightComponent()->IsVisible());
	Cycle->SetNight(true);
	Switch->Toggle();
	TestTrue(TEXT("Wall switch still works at night"), Lamp->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Wall switch does not change night"), Cycle->IsNight());
	Child->ToggleDayNight();
	TestFalse(TEXT("Second request restores daytime"), Child->IsNight());
	TestTrue(TEXT("Moon hidden during day"), Moon->IsHidden());
	TestTrue(TEXT("Night atmosphere hidden during day"), Fog->IsHidden());
	TestTrue(TEXT("Day disk restored"), Sun->GetComponent()->AtmosphereSunDiskColorScale.Equals(FLinearColor::White));
	TestTrue(TEXT("Day direction restored"), Sun->GetActorRotation().Equals(FRotator(-32, -65, 0)));
	TestTrue(TEXT("Day temperature restored"), bool(Sun->GetLightComponent()->bUseTemperature));
	TestEqual(TEXT("Original sun intensity restored without drift"), Sun->GetLightComponent()->Intensity, 65000.0f);
	TestTrue(TEXT("Original sun color restored"), Sun->GetLightComponent()->GetLightColor().Equals(FLinearColor::White));
	TestEqual(TEXT("Original sky restored"), Sky->GetLightComponent()->Intensity, 1.0f);
	TestEqual(TEXT("Original min exposure restored"), Exposure->Settings.AutoExposureMinBrightness, 9.0f);
	TestEqual(TEXT("Original max exposure restored"), Exposure->Settings.AutoExposureMaxBrightness, 10.0f);
	TestFalse(TEXT("Original override flag restored"), bool(Exposure->Settings.bOverride_AutoExposureMinBrightness));
	TestTrue(TEXT("Daytime preserves indoor lamp on state"), Lamp->GetLightComponent()->IsVisible());
	return true;
}

#endif
