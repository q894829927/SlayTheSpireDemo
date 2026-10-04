#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalSystem.h"
#include "Interior/InteriorPortalPhysicsBinding.h"
#include "Interior/InteriorPortalPhysicsBindingBridge.h"
#include "Interior/InteriorPortalChaosSpeedCap.h"
#include "Interior/InteriorPortalBodyGeometry.h"

namespace InteriorPortalPhysicsMapCoverage
{
	constexpr const TCHAR* TargetMap = TEXT("/Game/House/L_Interior_LivingKitchen");

	const TCHAR* BindingResultName(const InteriorPortalPhysics::EPhysicsBindingResult Result)
	{
		using namespace InteriorPortalPhysics;
		switch (Result)
		{
		case EPhysicsBindingResult::Ready: return TEXT("Ready");
		case EPhysicsBindingResult::InvalidRequest: return TEXT("InvalidRequest");
		case EPhysicsBindingResult::StaleTraveller: return TEXT("StaleTraveller");
		case EPhysicsBindingResult::UnsupportedSupport: return TEXT("UnsupportedSupport");
		case EPhysicsBindingResult::IncompatibleAperture: return TEXT("IncompatibleAperture");
		default: return TEXT("Unknown");
		}
	}

	class FInspectPIEMap final : public IAutomationLatentCommand
	{
	public:
		explicit FInspectPIEMap(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			FWorldContext* Context = GEditor ? GEditor->GetPIEWorldContext() : nullptr;
			UWorld* World = Context ? Context->World() : nullptr;
			if (!World) { return false; }
			if (bObserving)
			{
				Observer.Update_GameThread(&ActiveRequest);
				if (Observer.ObservedClearanceSteps() < 3 && ++ObservationTicks < 120) { return false; }
				const auto Result = Observer.LastClearanceReason();
				Test->AddInfo(FString::Printf(TEXT("nativeSubsteps=%llu clearanceSteps=%llu bindingMismatches=%llu lastClearance=%d bindingIssue=%d component=%d stepDt=%.9f maxDt=%.9f solverFrame=%d"),
					Observer.ObservedPhysicsSteps(),Observer.ObservedClearanceSteps(),Observer.BindingMismatchSteps(),static_cast<int32>(Result),
					static_cast<int32>(Observer.LastBindingIssue()),Observer.LastBindingComponent(),
					Observer.LastClearanceStep().DeltaSeconds,ActiveRequest.Command.MaxStepSeconds,
					Observer.LastClearanceStep().SolverFrame));
				Test->TestTrue(TEXT("Authored map reaches repeated real Chaos clearance observations"),
					Observer.ObservedClearanceSteps() >= 3);
				Test->TestTrue(TEXT("Authored scaled supports remain native-bound; a variable step or uncapped motion rejects conservatively"),
					Observer.BindingMismatchSteps() == 0
					&& (Result == InteriorPortalPhysics::EStaticClearanceReason::InvalidInterval
						|| Result == InteriorPortalPhysics::EStaticClearanceReason::UnsupportedMotion)
					&& Observer.LastBindingIssue() == InteriorPortalPhysics::ENativeBindingIssue::None);
				Observer.Shutdown_GameThread();
				return true;
			}
			if (++ReadyTicks < 3) { return false; }
			AInteriorPortalSystem* System = nullptr;
			int32 Systems = 0;
			for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
			{
				++Systems;
				if (!System) { System = *It; }
			}
			if (Systems != 1 || !IsValid(System))
			{
				Test->AddError(FString::Printf(TEXT("Expected one portal system in map PIE; found %d"),Systems));
				return true;
			}
			using namespace InteriorPortalPhysics;
			const AInteriorPortal* Portals[2] = {System->BluePortal,System->OrangePortal};
			for (int32 I=0; I<2; ++I)
			{
				const AInteriorPortal* Portal = Portals[I];
				InteriorPortalPhysics::FGeometry Geometry;
				const EGeometryResult GeometryResult = IsValid(Portal) && IsValid(Portal->Support)
					? ExtractStaticSupportGeometry(Portal->Support,Geometry) : EGeometryResult::UnsupportedComponent;
				Test->AddInfo(FString::Printf(TEXT("endpoint=%d placed=%d support=%s supportClass=%s aperture=(%.3f,%.3f) geometry=%s"),
					I,Portal ? int32(Portal->bPlaced) : 0,
					Portal && IsValid(Portal->Support) ? *Portal->Support->GetPathName() : TEXT("<none>"),
					Portal && IsValid(Portal->Support) ? *Portal->Support->GetClass()->GetName() : TEXT("<none>"),
					Portal ? Portal->HalfWidth : 0,Portal ? Portal->HalfHeight : 0,Reason(GeometryResult)));
				if (GeometryResult == EGeometryResult::Fits)
				{
					const FTransform ComponentPose = Portal->Support->GetComponentTransform();
					const FFitResult Span = EvaluatePose(Geometry,ComponentPose,Portal->GetLogicalFrame(),1.e100,1.e100);
					Test->AddInfo(FString::Printf(TEXT("endpoint=%d supportScale=%s bakedScale=%s poseResult=%s normalSpan=(%.3f,%.3f)"),
						I,*ComponentPose.GetScale3D().ToString(),*Geometry.BakedScale.ToString(),
						Reason(Span.Result),Span.MinNormal,Span.MaxNormal));
				}
			}
			for (int32 I=0; I<System->PortalSurfaces.Num(); ++I)
			{
				UPrimitiveComponent* Surface = System->PortalSurfaces[I];
				InteriorPortalPhysics::FGeometry Geometry;
				const auto Result = ExtractStaticSupportGeometry(Surface,Geometry);
				Test->AddInfo(FString::Printf(TEXT("surface=%d component=%s class=%s geometry=%s"),I,
					IsValid(Surface) ? *Surface->GetPathName() : TEXT("<none>"),
					IsValid(Surface) ? *Surface->GetClass()->GetName() : TEXT("<none>"),Reason(Result)));
			}
			Test->AddInfo(FString::Printf(TEXT("linked=%d pairGeneration=%llu registeredTravellers=%d surfaces=%d"),
				int32(System->IsLinked()),System->GetPhysicsPairGeneration(),System->PhysicsTravellers.Num(),System->PortalSurfaces.Num()));
			int32 ReadyBindings = 0;
			for (UPrimitiveComponent* Body : System->PhysicsTravellers)
			{
				EGeometryResult RegistrationResult = EGeometryResult::InvalidGeometry;
				const bool bRegistered = Registry.Register(Body,RegistrationResult);
				FTravellerSnapshot Snapshot;
				if (bRegistered) { Registry.Capture(Body,Snapshot); }
				EPhysicsBindingResult BindingResult = EPhysicsBindingResult::InvalidRequest;
				bool bNativeBound = false;
				if (bRegistered && IsValid(Portals[0]) && IsValid(Portals[1]) && System->IsLinked())
				{
					FPhysicsBindingRequest Request;
					Request.World = World; Request.Body = Body; Request.Registry = &Registry;
					Request.Supports[0] = Portals[0]->Support; Request.Supports[1] = Portals[1]->Support;
					Request.EndpointApertures[0] = FVector2D(Portals[0]->HalfWidth,Portals[0]->HalfHeight);
					Request.EndpointApertures[1] = FVector2D(Portals[1]->HalfWidth,Portals[1]->HalfHeight);
					Request.Command.Traveller = Snapshot;
					Request.Command.PairGeneration = System->GetPhysicsPairGeneration();
					Request.Command.Revision = 1;
					Request.Command.Entry = Portals[0]->GetLogicalFrame();
					Request.Command.Exit = Portals[1]->GetLogicalFrame();
					Request.Command.HalfWidth = Portals[0]->HalfWidth;
					Request.Command.HalfHeight = Portals[0]->HalfHeight;
					FPreparedPhysicsBinding Prepared;
					BindingResult = PreparePhysicsBinding_GameThread(Request,Prepared);
					if (BindingResult == EPhysicsBindingResult::Ready)
					{
						if (!bObserving)
						{
							ActiveRequest = Request;
							Observer.Update_GameThread(&ActiveRequest);
							bNativeBound = Observer.HasLiveBinding();
							bObserving = bNativeBound;
						}
						if (bNativeBound) { ++ReadyBindings; }
					}
				}
				Test->AddInfo(FString::Printf(TEXT("traveller=%s class=%s registered=%d geometry=%s binding=%s nativeBound=%d"),
					IsValid(Body) ? *Body->GetPathName() : TEXT("<none>"),
					IsValid(Body) ? *Body->GetClass()->GetName() : TEXT("<none>"),
					int32(bRegistered),Reason(RegistrationResult),BindingResultName(BindingResult),int32(bNativeBound)));
			}
			Test->TestTrue(TEXT("Authored map has at least one prepared and native-bound traveller"),ReadyBindings > 0);
			return !bObserving;
		}
	private:
		FAutomationTestBase* Test;
		InteriorPortalPhysics::FTravellerRegistry Registry;
		InteriorPortalPhysics::FPortalPhysicsBindingBridge Observer;
		InteriorPortalPhysics::FPhysicsBindingRequest ActiveRequest;
		int32 ReadyTicks = 0;
		int32 ObservationTicks = 0;
		bool bObserving = false;
	};

	/** A separately owned PIE-only body exercises the authored scene without
	 * entering the gameplay actor's legacy hold/recovery/transfer writer list. */
	class FProbeNativeScene final : public IAutomationLatentCommand
	{
	public:
		explicit FProbeNativeScene(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			FWorldContext* Context = GEditor ? GEditor->GetPIEWorldContext() : nullptr;
			UWorld* World = Context ? Context->World() : nullptr;
			if (!World) { return false; }
			using namespace InteriorPortalPhysics;
			if (!Body)
			{
				if (++ReadyTicks < 3) { return false; }
				AInteriorPortalSystem* System = nullptr;
				for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
				{
					if (System) { Test->AddError(TEXT("Expected one portal system for native scene probe")); return true; }
					System = *It;
				}
				if (!IsValid(System) || !System->IsLinked() || !System->GetPhysicsPairGeneration()
					|| !IsValid(System->BluePortal) || !IsValid(System->OrangePortal)
					|| !IsValid(System->BluePortal->Support) || !IsValid(System->OrangePortal->Support))
				{ Test->AddError(TEXT("Map has no linked, supported portal pair")); return true; }
				LegacySystem = System;
				const FTransform Entry = System->BluePortal->GetLogicalFrame();
				ProbeActor = World->SpawnActor<AActor>();
				if (!IsValid(ProbeActor)) { Test->AddError(TEXT("Could not spawn PIE-only native traveller")); return true; }
				Body = NewObject<UBoxComponent>(ProbeActor);
				ProbeActor->SetRootComponent(Body); ProbeActor->AddInstanceComponent(Body);
				Body->SetBoxExtent(FVector(5)); Body->SetMobility(EComponentMobility::Movable);
				Body->SetCollisionProfileName(TEXT("PhysicsActor"));
				Body->BodyInstance.bContactModification = true;
				ProbeActor->SetActorLocation(Entry.GetLocation()+Entry.GetUnitAxis(EAxis::X)*20.);
				Body->SetEnableGravity(false); Body->RegisterComponent(); Body->SetSimulatePhysics(true);
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Test->TestFalse(TEXT("Native scene probe is not registered with legacy gameplay movement writer"),
					System->PhysicsTravellers.Contains(Body));
				Request.World = World; Request.Body = Body; Request.Registry = &Registry;
				Request.Supports[0] = System->BluePortal->Support;
				Request.Supports[1] = System->OrangePortal->Support;
				Request.EndpointApertures[0] = FVector2D(System->BluePortal->HalfWidth,System->BluePortal->HalfHeight);
				Request.EndpointApertures[1] = FVector2D(System->OrangePortal->HalfWidth,System->OrangePortal->HalfHeight);
				Request.Command.PairGeneration = System->GetPhysicsPairGeneration();
				Request.Command.Revision = 1;
				Request.Command.Entry = Entry; Request.Command.Exit = System->OrangePortal->GetLogicalFrame();
				Request.Command.HalfWidth = System->BluePortal->HalfWidth;
				Request.Command.HalfHeight = System->BluePortal->HalfHeight;
				Request.Command.MaxStepSeconds = 1. / 30.;
				return false;
			}
			if (!bBound)
			{
				if (++WarmupTicks < 3) { return false; }
				Test->TestTrue(TEXT("Native scene probe stays outside legacy writer after discovery ticks"),
					LegacySystem.IsValid() && !LegacySystem->PhysicsTravellers.Contains(Body));
				EGeometryResult GeometryResult;
				if (!Registry.Register(Body,GeometryResult) || GeometryResult != EGeometryResult::Fits
					|| !Registry.Capture(Body,Request.Command.Traveller)
					|| !FChaosTravellerSpeedCap::Install_GameThread(Body->BodyInstance,250))
				{ Test->AddError(TEXT("PIE-only traveller registration or native speed cap failed")); return Finish(); }
				Observer.Update_GameThread(&Request);
				bBound = Observer.HasLiveBinding();
				if (!bBound) { Test->AddError(TEXT("PIE-only native scene binding failed")); return Finish(); }
				return false;
			}
			Observer.Update_GameThread(&Request);
			if (Observer.ObservedClearanceSteps() < 3 && ++ObservationTicks < 120) { return false; }
			const EStaticClearanceReason Result = Observer.LastClearanceReason();
			Test->AddInfo(FString::Printf(TEXT("nativeSceneProbe steps=%llu mismatches=%llu result=%d bindingIssue=%d sceneIssues=0x%08x dt=%.9f body=%s"),
				Observer.ObservedClearanceSteps(),Observer.BindingMismatchSteps(),static_cast<int32>(Result),
				static_cast<int32>(Observer.LastBindingIssue()),Observer.LastSceneIssueMask(),Observer.LastClearanceStep().DeltaSeconds,
				*Body->GetComponentLocation().ToString()));
			Test->TestTrue(TEXT("Independent actual-map body reaches repeated native scene checks"),
				Observer.ObservedClearanceSteps() >= 3 && Observer.BindingMismatchSteps() == 0);
			Test->TestTrue(TEXT("Native scene probe remains outside legacy writer while observed"),
				LegacySystem.IsValid() && !LegacySystem->PhysicsTravellers.Contains(Body));
			Test->TestTrue(TEXT("Capped native map probe reaches scene clearance or a scene-owned rejection"),
				Result == EStaticClearanceReason::Clear || Result == EStaticClearanceReason::UnsupportedScene
				|| Result == EStaticClearanceReason::SourceBlocked || Result == EStaticClearanceReason::DestinationBlocked
				|| Result == EStaticClearanceReason::UncertifiedDynamicInteraction);
			Test->TestTrue(TEXT("Unsupported native scene rejection carries a typed reason"),
				Result != EStaticClearanceReason::UnsupportedScene || Observer.LastSceneIssueMask() != 0);
			return Finish();
		}
	private:
		bool Finish()
		{
			Observer.Shutdown_GameThread();
			if (IsValid(ProbeActor)) { ProbeActor->Destroy(); }
			Body = nullptr; ProbeActor = nullptr;
			return true;
		}
		FAutomationTestBase* Test;
		InteriorPortalPhysics::FTravellerRegistry Registry;
		InteriorPortalPhysics::FPortalPhysicsBindingBridge Observer;
		InteriorPortalPhysics::FPhysicsBindingRequest Request;
		TWeakObjectPtr<AInteriorPortalSystem> LegacySystem;
		AActor* ProbeActor = nullptr;
		UBoxComponent* Body = nullptr;
		int32 ReadyTicks = 0, WarmupTicks = 0, ObservationTicks = 0;
		bool bBound = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalPhysicsMapBindingCoverageTest,
	"SlayTheSpireDemo.Interior.Portals.MapBindingCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalPhysicsMapBindingCoverageTest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->IsPlayingSessionInEditor())
	{
		AddError(TEXT("Map binding coverage requires an idle Unreal Editor."));
		return true;
	}
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	const FString CurrentMap = EditorWorld ? EditorWorld->GetOutermost()->GetName() : FString();
	if (CurrentMap != InteriorPortalPhysicsMapCoverage::TargetMap)
	{
		AddError(FString::Printf(TEXT("Open %s before this read-only PIE test; current map: %s"),
			InteriorPortalPhysicsMapCoverage::TargetMap,*CurrentMap));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(InteriorPortalPhysicsMapCoverage::FInspectPIEMap(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalPhysicsMapNativeSceneCoverageTest,
	"SlayTheSpireDemo.Interior.Portals.MapNativeSceneCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalPhysicsMapNativeSceneCoverageTest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->IsPlayingSessionInEditor())
	{
		AddError(TEXT("Map native scene coverage requires an idle Unreal Editor."));
		return true;
	}
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	const FString CurrentMap = EditorWorld ? EditorWorld->GetOutermost()->GetName() : FString();
	if (CurrentMap != InteriorPortalPhysicsMapCoverage::TargetMap)
	{
		AddError(FString::Printf(TEXT("Open %s before this PIE native scene test; current map: %s"),
			InteriorPortalPhysicsMapCoverage::TargetMap,*CurrentMap));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(InteriorPortalPhysicsMapCoverage::FProbeNativeScene(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
