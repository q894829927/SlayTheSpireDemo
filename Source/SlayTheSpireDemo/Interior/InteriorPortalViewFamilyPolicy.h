#pragma once

#include "ShowFlags.h"

namespace InteriorPortalRendering
{

/** Additional families bypass UGameViewportClient::Draw. Resolve the same game
 * quality/show-flag overrides before applying this producer's HDR/TSR policy. */
inline FEngineShowFlags BuildAdditionalViewShowFlags(const FEngineShowFlags& ViewportFlags)
{
	FEngineShowFlags Flags = ViewportFlags;
	EngineShowFlagOverride(ESFIM_Game, VMI_Lit, Flags, false);
	// The receiver applies motion blur/DOF once, after portal HDR composition.
	Flags.SetMotionBlur(false);
	Flags.SetDepthOfField(false);
	Flags.SetTemporalAA(true);
	Flags.SetScreenPercentage(true);
	return Flags;
}

}
