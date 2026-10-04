#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorPortalProjectiveAperture.h"
#include "Interior/InteriorPortalProjectedBounds.h"
#include "Interior/InteriorPortalRenderer.h"
#include "Interior/InteriorPortalViewFamilyPolicy.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "Math/PerspectiveMatrix.h"
#include "Engine/World.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalAnalyticApertureGeometryTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.AnalyticApertureGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalAnalyticApertureGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FTransform PortalFrame(FRotator(0.0, 89.99, 0.0), FVector(1200.0, 35.0, 160.0));
	InteriorPortalProjectiveAperture::FScreenToPortalMapping Geometry;
	TestTrue(TEXT("Analytic geometry remains valid independently of projected determinant"),
		InteriorPortalProjectiveAperture::BuildAnalyticRayPlaneGeometry(
			PortalFrame, 65.0, 115.0, 0.6, Geometry));
	TestTrue(TEXT("Analytic transport flag is explicit"), Geometry.bAnalyticRayPlane);
	TestTrue(TEXT("Analytic geometry does not encode an inverse-homography quality"),
		FMath::IsNearlyEqual(Geometry.DeterminantQuality, 1.0f));
	TestTrue(TEXT("Cosmetic surface bias is transported independently"),
		FMath::IsNearlyEqual(Geometry.Row2.W, 0.6f));
	TestTrue(TEXT("Portal basis remains normalized"),
		FMath::IsNearlyEqual(FVector(Geometry.Row1.X, Geometry.Row1.Y, Geometry.Row1.Z).Size(), 1.0, 1.0e-4)
		&& FMath::IsNearlyEqual(FVector(Geometry.Row2.X, Geometry.Row2.Y, Geometry.Row2.Z).Size(), 1.0, 1.0e-4)
		&& FMath::IsNearlyEqual(FVector(Geometry.ClipZRow.X, Geometry.ClipZRow.Y, Geometry.ClipZRow.Z).Size(), 1.0, 1.0e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalRecursiveRenderRequestTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.RecursiveRenderRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalRecursiveRenderRequestTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FIntRect ViewRect(0, 0, 1920, 1080);
	const FMatrix Projection = FReversedZPerspectiveMatrix(PI / 4.0, 1920, 1080, 10.0);
	const FTransform Entry(FRotator::ZeroRotator, FVector(500.0, 0.0, 100.0));
	const FTransform Exit(FRotator(0.0, 180.0, 0.0), FVector(1500.0, 0.0, 100.0));
	const FTransform ParentView(FRotator::ZeroRotator, FVector::ZeroVector);
	const FMatrix ViewPlanes(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FMatrix ViewProjection =
		FTranslationMatrix(-ParentView.GetLocation())
		* FInverseRotationMatrix(ParentView.Rotator())
		* ViewPlanes
		* Projection;

	for (int32 Level = 0; Level < 4; ++Level)
	{
		FInteriorPortalRenderRequest Request;
		const bool bBuilt = FInteriorPortalRenderRequest::Build(
			0, 0, Level,
			ParentView, Entry, Exit,
			65.0, 115.0,
			ViewProjection, ViewRect, Projection,
			true, 10.0, 1.5,
			uint64(Level + 1), Request);
		TestTrue(*FString::Printf(TEXT("Recursion level %d can build a request"), Level), bBuilt);
		if (bBuilt)
		{
			TestEqual(TEXT("Request preserves recursion level"), Request.RecursionLevel, Level);
			TestTrue(TEXT("Production analytic aperture is present"),
				Request.ForegroundDepthReference.bValid
				&& Request.ForegroundDepthReference.bAnalyticRayPlane);
			TestEqual(TEXT("Only level 0 owns player exposure authority"),
				Request.bPlayerExposureAuthority, Level == 0);
		}
	}

	FInteriorPortalRenderRequest Rejected;
	TestFalse(TEXT("Recursion level beyond production maximum is rejected"),
		FInteriorPortalRenderRequest::Build(
			0, 0, 4,
			ParentView, Entry, Exit,
			65.0, 115.0,
			ViewProjection, ViewRect, Projection,
			true, 10.0, 1.5, 1, Rejected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalTravellerVisualPartitionTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.TravellerVisualPartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalTravellerVisualPartitionTest::RunTest(const FString& Parameters)
{
	const FTransform Entry(FRotator(31.0, 79.0, 17.0), FVector(1500.0, 400.0, 110.0));
	const FTransform Exit(FRotator(-11.0, -93.0, 29.0), FVector(-900.0, 600.0, 180.0));
	for (int32 Level = 0; Level < 4; ++Level)
	{
		FInteriorPortalRenderRequest Request;
		Request.EntryFrame = Entry;
		Request.ExitFrame = Exit;
		Request.ViewLocation = Exit.TransformPosition(FVector(-100.0, 20.0, 80.0));
		Request.RecursionLevel = Level;
		InteriorPortalProjectiveAperture::BuildAnalyticRayPlaneGeometry(
			Entry, 65.0, 115.0, 0.6, Request.ForegroundDepthReference);
		InteriorPortalMath::BuildPortalClipPlane(Exit, 0.5, Request.ExitClipPlane);
		Request.UseLogicalPlaneComposition(FPrimitiveComponentId(), FLinearColor::Blue, 5.0f);
		TestEqual(TEXT("Foreground uses the logical plane despite cosmetic bias"),
			Request.ForegroundDepthReference.Row2.W, 0.0f);
		const double CoplanarDistance = Request.ExitClipPlane.PlaneDot(Exit.GetLocation());
		TestTrue(TEXT("Coplanar exit support is excluded by a numerical tie bound, not a centimeter band"),
			CoplanarDistance < 0.0 && CoplanarDistance >= -0.000101);
		for (double Distance : {-1.0, -0.1, -0.01, 0.01, 0.1, 1.0})
		{
			// Samples next to the seam must belong to exactly one representation,
			// including the former 0.5 cm remote clip exclusion band.
			const FVector Source = Entry.TransformPosition(FVector(Distance, 12.0, 9.0));
			const FVector Mapped = InteriorPortalMath::Position(Source, Entry, Exit);
			const double RemoteSlice = FVector::DotProduct(
				Mapped - Exit.GetLocation(), Exit.GetUnitAxis(EAxis::X));
			const bool bSourceVisible = Distance >= 0.0;
			const bool bRemoteSliceVisible = RemoteSlice >= 0.0;
			const bool bRemoteClipVisible = Request.ExitClipPlane.PlaneDot(Mapped) >= 0.0;
			TestTrue(TEXT("Complementary slices leave neither a hole nor duplicate ownership"),
				bSourceVisible != bRemoteSliceVisible);
			TestEqual(TEXT("Hardware clip retains every point kept by remote slice"),
				bRemoteClipVisible, bRemoteSliceVisible);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCompositedSurfaceOwnershipTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.CompositedSurfaceOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalCompositedSurfaceOwnershipTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> Extension =
		FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
	Extension->SetEnabled(true);
	FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(
		nullptr, nullptr, FEngineShowFlags(ESFIM_Game)).SetTime(FGameTime()));
	Family.bIsMainViewFamily = false;
	FSceneViewInitOptions Options;
	Options.ViewFamily = &Family;
	Options.SetViewRectangle(FIntRect(17, 23, 657, 503));
	FPrimitiveComponentId Surface;
	Surface.PrimIDValue = 123;
	FPrimitiveComponentId Unrelated;
	Unrelated.PrimIDValue = 456;
	FInteriorPortalRenderRequest Request;
	Request.bEnabled = true;
	Request.PortalId = Request.EndpointIndex = 0;
	Request.HistoryIdentity = 1;
	Request.ProjectedBounds.bHasVisiblePortion = true;
	Request.ViewRect = Request.ScissorRect = Options.GetViewRect();
	Request.EntryFrame = FTransform(FVector(-10, 0, 0));
	Request.UseLogicalPlaneComposition(Surface, FLinearColor::Blue, 5.0f);
	Extension->PublishRequest(Request);
	{
		FSceneView View(Options);
		View.HiddenPrimitives.Add(Unrelated);
		Family.Views.Add(&View);
		Extension->SetupView(Family, View);
		TestFalse(TEXT("Early SetupView has not yet received the viewport's main-family identity"),
			View.HiddenPrimitives.Contains(Surface));
		Family.bIsMainViewFamily = true;
		Extension->BeginRenderViewFamily(Family);
		TestTrue(TEXT("The composed main view excludes its opaque fallback surface"),
			View.HiddenPrimitives.Contains(Surface));
		TestTrue(TEXT("Existing view exclusions are preserved"), View.HiddenPrimitives.Contains(Unrelated));
		Family.Views.Reset();
	}
	// Two independent publishers must not suppress one another. Clearing one
	// restores only its own fallback, while its peer continues to compose.
	TSharedPtr<FInteriorPortalViewExtension, ESPMode::ThreadSafe> Peer =
		FSceneViewExtensions::NewExtension<FInteriorPortalViewExtension>(World);
	Peer->SetEnabled(true);
	FPrimitiveComponentId PeerSurface;
	PeerSurface.PrimIDValue = 789;
	FInteriorPortalRenderRequest PeerRequest = Request;
	PeerRequest.PortalId = PeerRequest.EndpointIndex = 1;
	PeerRequest.UseLogicalPlaneComposition(PeerSurface, FLinearColor::Red, 5.0f);
	Peer->PublishRequest(PeerRequest);
	for (bool bClearFirst : {false, true})
	{
		if (bClearFirst) { Extension->ClearRequest(); }
		FSceneView View(Options);
		Family.Views.Add(&View);
		Extension->BeginRenderViewFamily(Family);
		TestFalse(TEXT("One endpoint publisher preserves its peer's fallback"),
			View.HiddenPrimitives.Contains(PeerSurface));
		Peer->BeginRenderViewFamily(Family);
		TestTrue(TEXT("The independently published peer opening is compositor-owned"),
			View.HiddenPrimitives.Contains(PeerSurface));
		TestEqual(TEXT("Clearing one endpoint changes only its own ownership"),
			View.HiddenPrimitives.Contains(Surface), !bClearFirst);
		TestFalse(TEXT("Unrelated geometry is never excluded by either publisher"),
			View.HiddenPrimitives.Contains(Unrelated));
		Family.Views.Reset();
	}
	Extension->PublishRequest(Request);
	Peer.Reset();
	Family.bAdditionalViewFamily = true;
	{
		FSceneView View(Options);
		Family.Views.Add(&View);
		Extension->BeginRenderViewFamily(Family);
		TestFalse(TEXT("Main publisher cannot change additional view visibility"),
			View.HiddenPrimitives.Contains(Surface));
		Family.Views.Reset();
	}
	Family.bAdditionalViewFamily = false;
	for (bool bDisable : {false, true})
	{
		if (bDisable) { Extension->SetEnabled(false); }
		else { Extension->ClearRequest(); }
		FSceneView View(Options);
		Family.Views.Add(&View);
		Extension->BeginRenderViewFamily(Family);
		TestFalse(TEXT("Cleared/stopped composition restores normal fallback visibility"),
			View.HiddenPrimitives.Contains(Surface));
		Family.Views.Reset();
	}
	// A completed front-side publication may outlive a camera side change. It
	// cannot hide the fallback or grant ownership to the support's back face.
	Extension->SetEnabled(true);
	Extension->PublishRequest(Request);
	Options.ViewOrigin = FVector(-20, 0, 0);
	{
		FSceneView View(Options);
		Family.Views.Add(&View);
		Extension->BeginRenderViewFamily(Family);
		TestFalse(TEXT("A stale front-side publication cannot hide a back-side fallback"),
			View.HiddenPrimitives.Contains(Surface));
		Family.Views.Reset();
	}
	Extension.Reset();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalReceivingPlanePrecisionTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.ReceivingPlanePrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalReceivingPlanePrecisionTest::RunTest(const FString& Parameters)
{
	// A 1 mm source slice must survive GPU encoding even in a translated world.
	// Encoding world coordinates as float first erases both it and the plane.
	const FVector WorldOffset(100000000.0, -100000000.0, 100000000.0);
	const FTransform Plane(FRotator(0, 90, 0), WorldOffset + FVector(0.125, 0.375, 0.625));
	for (int32 Level = 0; Level < 4; ++Level)
	{
		FInteriorPortalRenderRequest Request;
		Request.EntryFrame = Plane;
		Request.RecursionLevel = Level;
		for (double CameraDistance : {100.0, 1000.0})
		{
			const FVector Camera = Plane.TransformPosition(FVector(CameraDistance, 40, 80));
			const FVector3f Encoded = Request.GetTranslatedPlaneCenter(-Camera);
			TestTrue(TEXT("Receiving camera is subtracted before float encoding"),
				FVector(Encoded).Equals(Plane.GetLocation() - Camera, 0.0001));
			const FVector3f Hit(Plane.TransformPosition(FVector(0.1, 12, 9)) - Camera);
			const float Distance = FVector3f::DotProduct(Hit - Encoded,
				FVector3f(Plane.GetUnitAxis(EAxis::X)));
			TestTrue(TEXT("Sub-centimeter source slice remains on the positive side"),
				FMath::IsNearlyEqual(Distance, 0.1f, 0.0001f));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalReceivingRasterPlaneTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.ReceivingRasterPlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalReceivingRasterPlaneTest::RunTest(const FString& Parameters)
{
	const FIntRect Rect(317, 83, 1091, 679);
	const FMatrix ViewPlanes(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FVector Offset(1.e8, -1.e8, 1.e8);
	for (int32 Level = 0; Level < 4; ++Level)
	{
		const FRotator Rotation(-7, 19 + 13 * Level, 0);
		const FVector Camera = Offset + FVector(300, 700, 120);
		FInteriorPortalRenderRequest Request;
		Request.EntryFrame = FTransform(Rotation.Quaternion() * FRotator(0, 60, 0).Quaternion(),
			Camera + Rotation.Vector() * 500);
		const FMatrix Projection = FReversedZPerspectiveMatrix(PI / 4, 774, 596, 10);
		FMatrix Cropped;
		const FIntRect Parent(0, 0, 1920, 1080);
		TestTrue(TEXT("Offset recursive crop builds"),
			InteriorPortalProjectedBounds::BuildCroppedProjection(Projection, Parent, Rect, Cropped));
		const FMatrix TranslatedToClip = FInverseRotationMatrix(Rotation) * ViewPlanes * Cropped;
		const FVector4f Equation = Request.GetReceivingPlaneDepthEquation(
			TranslatedToClip.Inverse(), -Camera, Rect);
		TestEqual(TEXT("Finite receiving-plane equation"), Equation.W, 1.f);
		for (double Y : {-40., 0., 40.})
		{
			for (double Z : {-100., 0., 100.})
			{
				for (double Slice : {0., -0.1})
				{
					const FVector P = Request.EntryFrame.TransformPosition(FVector(Slice, Y, Z)) - Camera;
					const FVector4 Clip = TranslatedToClip.TransformFVector4(FVector4(P, 1));
					const double Px = Rect.Min.X + (Clip.X / Clip.W + 1) * Rect.Width() * .5;
					const double Py = Rect.Min.Y + (1 - Clip.Y / Clip.W) * Rect.Height() * .5;
					const double PlaneZ = Equation.X * Px + Equation.Y * Py + Equation.Z;
					const double RasterZ = Clip.Z / Clip.W;
					if (Slice == 0)
					{
						TestTrue(TEXT("Projected coplanar raster agrees across offset/crop/oblique views"),
							FMath::Abs(RasterZ - PlaneZ) < 1.e-8);
					}
					else
					{
						TestTrue(TEXT("A real 1 mm foreground slice remains resolvable in reversed raster depth"),
							RasterZ - PlaneZ > 1.e-6);
					}
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalDirectedReceivingViewTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.DirectedReceivingView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalDirectedReceivingViewTest::RunTest(const FString& Parameters)
{
	const FTransform Entry(FRotator(23, 71, -19), FVector(1.e8, -1.e8, 1.e8));
	const FTransform Exit(FRotator(-17, 149, 8), FVector(800, 1100, 105));
	for (int32 Level = 0; Level < 4; ++Level)
	{
		FInteriorPortalRenderRequest Request;
		Request.EntryFrame = Entry;
		Request.ExitFrame = Exit;
		Request.RecursionLevel = Level;
		Request.UseLogicalPlaneComposition(FPrimitiveComponentId(), FLinearColor::Blue, 0);
		for (double Distance : {-100., -0.01, 0.01, 100.})
		{
			const FVector Camera = Entry.TransformPosition(FVector(Distance, 40, 70));
			TestEqual(TEXT("All receiving levels use the same directed opening"),
				Request.CanComposeFrom(Camera), Distance > 0);
			const FTransform Child = InteriorPortalMath::BuildVirtualViewTransform(
				FTransform(FRotator::ZeroRotator, Camera), Entry, Exit);
			TestEqual(TEXT("Transport maps a legal receiving camera behind the exit; that is not a back-side receiver"),
				FInteriorPortalRenderRequest::IsFrontFacing(Exit, Child.GetLocation()), Distance < 0);
		}
		TestFalse(TEXT("A camera on the plane has no directed surface ownership"),
			Request.CanComposeFrom(Entry.GetLocation()));
		TestFalse(TEXT("An invalid camera cannot acquire surface ownership"),
			Request.CanComposeFrom(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalAdditionalViewShowFlagsTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.AdditionalViewShowFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPortalAdditionalViewShowFlagsTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Lighting = IConsoleManager::Get().FindConsoleVariable(TEXT("ShowFlag.Lighting"));
	IConsoleVariable* Shadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadowQuality"));
	IConsoleVariable* Exposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality"));
	if (!TestNotNull(TEXT("Stock lighting override exists"), Lighting)
		|| !TestNotNull(TEXT("Stock shadow quality exists"), Shadows)
		|| !TestNotNull(TEXT("Stock exposure quality exists"), Exposure)) return false;
	const int32 PreviousLighting = Lighting->GetInt();
	const int32 PreviousShadows = Shadows->GetInt();
	const int32 PreviousExposure = Exposure->GetInt();
	const EConsoleVariableFlags LightingPriority = EConsoleVariableFlags(Lighting->GetFlags() & ECVF_SetByMask);
	const EConsoleVariableFlags ShadowPriority = EConsoleVariableFlags(Shadows->GetFlags() & ECVF_SetByMask);
	const EConsoleVariableFlags ExposurePriority = EConsoleVariableFlags(Exposure->GetFlags() & ECVF_SetByMask);
	ON_SCOPE_EXIT
	{
		Lighting->Set(PreviousLighting, LightingPriority);
		Shadows->Set(PreviousShadows, ShadowPriority);
		Exposure->Set(PreviousExposure, ExposurePriority);
	};
	FEngineShowFlags RawViewportFlags(ESFIM_Game);
	RawViewportFlags.SetLighting(true);
	RawViewportFlags.SetDynamicShadows(true);
	RawViewportFlags.SetEyeAdaptation(true);
	RawViewportFlags.SetMotionBlur(true);
	RawViewportFlags.SetDepthOfField(true);
	Lighting->Set(0, LightingPriority);
	Shadows->Set(0, ShadowPriority);
	Exposure->Set(0, ExposurePriority);
	const FEngineShowFlags Disabled = InteriorPortalRendering::BuildAdditionalViewShowFlags(RawViewportFlags);
	TestFalse(TEXT("Unlit control affects secondary lighting, not just the main viewport"), Disabled.Lighting);
	TestFalse(TEXT("Shadow quality applies to additional families"), Disabled.DynamicShadows);
	TestFalse(TEXT("Exposure quality applies to additional families"), Disabled.EyeAdaptation);
	TestTrue(TEXT("Producer retains its native TSR/screen-percentage contract"), Disabled.TemporalAA && Disabled.ScreenPercentage);
	TestFalse(TEXT("Receiver-owned motion blur and DOF are not duplicated"), Disabled.MotionBlur || Disabled.DepthOfField);
	TestTrue(TEXT("The producer does not mutate viewport-owned configuration"),
		RawViewportFlags.Lighting && RawViewportFlags.DynamicShadows && RawViewportFlags.EyeAdaptation);
	Lighting->Set(1, LightingPriority);
	Shadows->Set(5, ShadowPriority);
	Exposure->Set(2, ExposurePriority);
	const FEngineShowFlags Enabled = InteriorPortalRendering::BuildAdditionalViewShowFlags(RawViewportFlags);
	TestTrue(TEXT("Lit mode regains lighting, shadows and exposure without stale disabled flags"),
		Enabled.Lighting && Enabled.DynamicShadows && Enabled.EyeAdaptation);
	return true;
}

#endif
