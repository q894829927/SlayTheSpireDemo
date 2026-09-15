#pragma once

#include "CoreMinimal.h"

/**
 * Projective aperture mapping used by the main-view portal compositor.
 *
 * Portal-local coordinates are normalized so local +Y/HalfWidth is u=+1 and
 * local +Z/HalfHeight is v=+1. The authored elliptical aperture is therefore
 * exactly u^2 + v^2 <= 1. A perspective projection maps the portal plane to
 * screen space with a 3x3 homography; the inverse rows below map normalized
 * constrained-view UV back to homogeneous portal-local (u,v,w).
 */
namespace InteriorPortalProjectiveAperture
{
	struct FScreenToPortalMapping
	{
		FVector4f Row0 = FVector4f(1, 0, 0, 0);
		FVector4f Row1 = FVector4f(0, 1, 0, 0);
		FVector4f Row2 = FVector4f(0, 0, 1, 0);
		float DeterminantQuality = 0.0f;
		bool bValid = false;
	};

	inline bool IsFiniteRow(const FVector4f& Row)
	{
		return FMath::IsFinite(Row.X)
			&& FMath::IsFinite(Row.Y)
			&& FMath::IsFinite(Row.Z)
			&& FMath::IsFinite(Row.W);
	}

	/**
	 * Build screen-UV -> normalized portal-local inverse homography.
	 *
	 * Screen UV uses the same normalized constrained-view convention as
	 * FPortalScreenBounds: X left->right, Y top->bottom, both [0,1].
	 *
	 * The normalized determinant guards the exact edge-on/camera-in-plane
	 * singularity without imposing an arbitrary world-space angle threshold.
	 */
	inline bool BuildScreenToPortalMapping(
		const FTransform& ApertureFrame,
		double HalfWidth,
		double HalfHeight,
		const FMatrix& ViewProjectionMatrix,
		FScreenToPortalMapping& OutMapping)
	{
		OutMapping = FScreenToPortalMapping();
		if (HalfWidth <= UE_SMALL_NUMBER || HalfHeight <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const FVector Center = ApertureFrame.GetLocation();
		const FVector WidthPoint = Center + ApertureFrame.GetUnitAxis(EAxis::Y) * HalfWidth;
		const FVector HeightPoint = Center + ApertureFrame.GetUnitAxis(EAxis::Z) * HalfHeight;

		const FVector4 ClipCenter = ViewProjectionMatrix.TransformFVector4(FVector4(Center, 1.0));
		const FVector4 ClipWidth = ViewProjectionMatrix.TransformFVector4(FVector4(WidthPoint, 1.0));
		const FVector4 ClipHeight = ViewProjectionMatrix.TransformFVector4(FVector4(HeightPoint, 1.0));
		const FVector4 Du = ClipWidth - ClipCenter;
		const FVector4 Dv = ClipHeight - ClipCenter;

		// H maps [u v 1]^T to homogeneous normalized screen UV:
		// Xh = 0.5 * (clip.x + clip.w)
		// Yh = 0.5 * (clip.w - clip.y)
		// Wh = clip.w
		const double A = 0.5 * (double(Du.X) + double(Du.W));
		const double B = 0.5 * (double(Dv.X) + double(Dv.W));
		const double C = 0.5 * (double(ClipCenter.X) + double(ClipCenter.W));
		const double D = 0.5 * (double(Du.W) - double(Du.Y));
		const double E = 0.5 * (double(Dv.W) - double(Dv.Y));
		const double F = 0.5 * (double(ClipCenter.W) - double(ClipCenter.Y));
		const double G = double(Du.W);
		const double H = double(Dv.W);
		const double I = double(ClipCenter.W);

		const double Determinant =
			A * (E * I - F * H)
			- B * (D * I - F * G)
			+ C * (D * H - E * G);

		double MaxCoefficient = 1.0;
		for (const double Value : {A, B, C, D, E, F, G, H, I})
		{
			MaxCoefficient = FMath::Max(MaxCoefficient, FMath::Abs(Value));
		}
		const double DeterminantQuality = FMath::Abs(Determinant)
			/ (MaxCoefficient * MaxCoefficient * MaxCoefficient);
		OutMapping.DeterminantQuality = float(DeterminantQuality);

		constexpr double MinDeterminantQuality = 1.0e-8;
		if (!FMath::IsFinite(Determinant)
			|| !FMath::IsFinite(DeterminantQuality)
			|| DeterminantQuality <= MinDeterminantQuality)
		{
			return false;
		}

		const double InvDeterminant = 1.0 / Determinant;
		OutMapping.Row0 = FVector4f(
			float((E * I - F * H) * InvDeterminant),
			float((C * H - B * I) * InvDeterminant),
			float((B * F - C * E) * InvDeterminant),
			0.0f);
		OutMapping.Row1 = FVector4f(
			float((F * G - D * I) * InvDeterminant),
			float((A * I - C * G) * InvDeterminant),
			float((C * D - A * F) * InvDeterminant),
			0.0f);
		OutMapping.Row2 = FVector4f(
			float((D * H - E * G) * InvDeterminant),
			float((B * G - A * H) * InvDeterminant),
			float((A * E - B * D) * InvDeterminant),
			0.0f);

		OutMapping.bValid = IsFiniteRow(OutMapping.Row0)
			&& IsFiniteRow(OutMapping.Row1)
			&& IsFiniteRow(OutMapping.Row2);
		return OutMapping.bValid;
	}
}
