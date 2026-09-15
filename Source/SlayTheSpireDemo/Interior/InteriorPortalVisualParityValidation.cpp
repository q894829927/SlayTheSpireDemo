#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace InteriorPortalVisualParityValidationPrivate
{
	struct FVisualParitySavedState
	{
		bool bActive = false;
		int32 CompositionDebugMode = 0;
		int32 StencilGatedComposition = 0;
		int32 StencilBypass = 0;
		int32 BoundedMainPassScissor = 0;
		int32 MainDepthPropagation = 0;
		int32 DepthAwareComposition = 0;
		int32 SecondaryDepthRemap = 0;
	};

	FVisualParitySavedState GSavedState;

	IConsoleVariable* FindCVar(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	int32 ReadInt(const TCHAR* Name, const int32 Fallback = 0)
	{
		if (IConsoleVariable* Var = FindCVar(Name))
		{
			return Var->GetInt();
		}
		return Fallback;
	}

	void SetInt(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Var = FindCVar(Name))
		{
			Var->Set(Value, ECVF_SetByCode);
		}
	}

	void DumpState()
	{
		UE_LOG(LogTemp, Display,
			TEXT("PortalVisualParityReference Active=%d DebugMode=%d StencilGate=%d StencilBypass=%d BoundedMainPass=%d MainDepthPropagation=%d DepthAware=%d SecondaryDepthRemap=%d PreExposureRebase=%d SecondaryPreExposure=%.9g"),
			GSavedState.bActive ? 1 : 0,
			ReadInt(TEXT("portal.CompositionDebugMode")),
			ReadInt(TEXT("portal.StencilGatedComposition")),
			ReadInt(TEXT("portal.StencilCompositionBypassShaderAperture")),
			ReadInt(TEXT("portal.BoundedMainPassScissor")),
			ReadInt(TEXT("portal.MainDepthPropagation")),
			ReadInt(TEXT("portal.DepthAwareComposition")),
			ReadInt(TEXT("portal.SecondaryDepthRemap")),
			ReadInt(TEXT("portal.PreExposureRebase")),
			FindCVar(TEXT("portal.SecondaryPreExposure"))
				? FindCVar(TEXT("portal.SecondaryPreExposure"))->GetFloat()
				: 1.0f);
	}

	void StartReference()
	{
		if (GSavedState.bActive)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalVisualParityReference: already active."));
			DumpState();
			return;
		}

		GSavedState.CompositionDebugMode = ReadInt(TEXT("portal.CompositionDebugMode"));
		GSavedState.StencilGatedComposition = ReadInt(TEXT("portal.StencilGatedComposition"));
		GSavedState.StencilBypass = ReadInt(TEXT("portal.StencilCompositionBypassShaderAperture"));
		GSavedState.BoundedMainPassScissor = ReadInt(TEXT("portal.BoundedMainPassScissor"));
		GSavedState.MainDepthPropagation = ReadInt(TEXT("portal.MainDepthPropagation"));
		GSavedState.DepthAwareComposition = ReadInt(TEXT("portal.DepthAwareComposition"));
		GSavedState.SecondaryDepthRemap = ReadInt(TEXT("portal.SecondaryDepthRemap"));
		GSavedState.bActive = true;

		// DebugMode 7 is intentionally a full-screen secondary color reference.
		// Disable every portal-only aperture/depth/scissor gate so the resulting
		// image isolates the full transformed FSceneViewFamily + TSR + PreExposure
		// path. The running TSR producer remains untouched and keeps ownership of
		// SecondaryPreExposure / portal.PreExposureRebase.
		SetInt(TEXT("portal.StencilCompositionBypassShaderAperture"), 0);
		SetInt(TEXT("portal.StencilGatedComposition"), 0);
		SetInt(TEXT("portal.BoundedMainPassScissor"), 0);
		SetInt(TEXT("portal.MainDepthPropagation"), 0);
		SetInt(TEXT("portal.DepthAwareComposition"), 0);
		SetInt(TEXT("portal.SecondaryDepthRemap"), 0);
		SetInt(TEXT("portal.CompositionDebugMode"), 7);

		UE_LOG(LogTemp, Display,
			TEXT("PortalVisualParityReference: START. Full-screen transformed secondary HDR reference is active. This does not move the player or change the persistent secondary ViewState."));
		DumpState();
	}

	void StopReference()
	{
		if (!GSavedState.bActive)
		{
			UE_LOG(LogTemp, Display, TEXT("PortalVisualParityReference: not active."));
			return;
		}

		SetInt(TEXT("portal.CompositionDebugMode"), GSavedState.CompositionDebugMode);
		SetInt(TEXT("portal.StencilGatedComposition"), GSavedState.StencilGatedComposition);
		SetInt(TEXT("portal.StencilCompositionBypassShaderAperture"), GSavedState.StencilBypass);
		SetInt(TEXT("portal.BoundedMainPassScissor"), GSavedState.BoundedMainPassScissor);
		SetInt(TEXT("portal.MainDepthPropagation"), GSavedState.MainDepthPropagation);
		SetInt(TEXT("portal.DepthAwareComposition"), GSavedState.DepthAwareComposition);
		SetInt(TEXT("portal.SecondaryDepthRemap"), GSavedState.SecondaryDepthRemap);
		GSavedState.bActive = false;

		UE_LOG(LogTemp, Display, TEXT("PortalVisualParityReference: STOP. Previous portal composition controls restored."));
		DumpState();
	}

	FAutoConsoleCommand GStartVisualParityReferenceCommand(
		TEXT("portal.StartVisualParityReference"),
		TEXT("STEP 1B.14A: replace main BeforeDOF SceneColor with the full-screen transformed secondary post-TSR HDR reference, after accepted PreExposure rebase. Disables stencil/depth/scissor gates temporarily."),
		FConsoleCommandDelegate::CreateStatic(&StartReference));

	FAutoConsoleCommand GStopVisualParityReferenceCommand(
		TEXT("portal.StopVisualParityReference"),
		TEXT("STEP 1B.14A: restore the portal composition controls saved by portal.StartVisualParityReference."),
		FConsoleCommandDelegate::CreateStatic(&StopReference));

	FAutoConsoleCommand GDumpVisualParityReferenceCommand(
		TEXT("portal.DumpVisualParityReference"),
		TEXT("STEP 1B.14A: print the current visual-parity isolation state and relevant portal CVars."),
		FConsoleCommandDelegate::CreateStatic(&DumpState));
}
