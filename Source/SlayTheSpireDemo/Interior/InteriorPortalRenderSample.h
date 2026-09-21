#pragma once

#include "CoreMinimal.h"
#include "SceneView.h"

namespace InteriorPortalRendering
{
	// SceneCapture families are also non-additional. Only the viewport's explicit
	// main-family flag identifies the player exposure/composition owner.
	inline bool IsPlayerMainView(const FSceneViewFamily& Family, const FSceneView& View)
	{
		return Family.bIsMainViewFamily && !Family.bAdditionalViewFamily
			&& !View.bIsSceneCapture && View.IsPrimarySceneView();
	}

	/** One submission's color metadata. Written and consumed only on the render
	 * thread, in the same order as the associated extraction and composition.
	 * Never populated from the previous frame's game-thread diagnostic readback. */
	struct FColorSample
	{
		explicit FColorSample(uint64 InSubmission) : Submission(InSubmission) {}
		const uint64 Submission;
		float PreExposure = 0.0f;

		bool TryGetExposureScale(float MainPreExposure, float& OutScale) const
		{
			if (!FMath::IsFinite(PreExposure) || PreExposure <= 0.0f
				|| !FMath::IsFinite(MainPreExposure) || MainPreExposure <= 0.0f)
			{
				return false;
			}
			OutScale = MainPreExposure / PreExposure;
			return FMath::IsFinite(OutScale) && OutScale > 0.0f;
		}
	};
}
