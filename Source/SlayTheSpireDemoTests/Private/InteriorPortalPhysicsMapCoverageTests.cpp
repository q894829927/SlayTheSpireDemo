#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interior/InteriorPortal.h"
#include "Interior/InteriorPortalSystem.h"
#include "Interior/InteriorPortalPhysicsBinding.h"
#include "Interior/InteriorPortalPhysicsBindingBridge.h"
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
				FTravellerRegistry Registry;
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
						FPortalPhysicsBindingBridge Observer;
						Observer.Update_GameThread(&Request);
						bNativeBound = Observer.HasLiveBinding();
						Observer.Shutdown_GameThread();
						if (bNativeBound) { ++ReadyBindings; }
					}
				}
				Test->AddInfo(FString::Printf(TEXT("traveller=%s class=%s registered=%d geometry=%s binding=%s nativeBound=%d"),
					IsValid(Body) ? *Body->GetPathName() : TEXT("<none>"),
					IsValid(Body) ? *Body->GetClass()->GetName() : TEXT("<none>"),
					int32(bRegistered),Reason(RegistrationResult),BindingResultName(BindingResult),int32(bNativeBound)));
			}
			Test->TestTrue(TEXT("Authored map has at least one prepared and native-bound traveller"),ReadyBindings > 0);
			return true;
		}
	private:
		FAutomationTestBase* Test;
		int32 ReadyTicks = 0;
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

#endif
