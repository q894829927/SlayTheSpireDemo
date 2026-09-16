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
 *
 * FScreenToPortalMapping is also used as an immutable transport container for
 * production analytic ray/plane geometry. bAnalyticRayPlane distinguishes the
 * two layouts explicitly so the historical inverse-homography diagnostics can
 * remain intact while production composition stops depending on H^-1.
 */
namespace InteriorPortalProjectiveAperture
{
	struct FScreenToPortalMapping
	{
		FVector4f Row0 = FVector4f(1, 0, 0, 0);
		FVector4f Row1 = FVector4f(0, 1, 0, 0);
		FVector4f Row2 = FVector4f(0, 0, 1, 0);
		/** clipZ(u,v)=ClipZRow.x*u+ClipZRow.y*v+ClipZRow.z before perspective divide. */
		FVector4f ClipZRow = FVector4f(0, 0, 0, 0);
		float DeterminantQuality = 0.0f;
		bool bValid = false;
		bool bAnalyticRayPlane = false;
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
	 * If OutMapping already carries analytic geometry, this call is the existing
	 * full-fidelity producer applying its cosmetic SurfaceVisualBias. Preserve the
	 * logical plane/basis and update only that signed bias instead of overwriting
	 * the analytic representation with another inverse homography.
	 */
	inline bool BuildScreenToPortalMapping(
		const FTransform& ApertureFrame,
		double HalfWidth,
		double HalfHeight,
		const FMatrix& ViewProjectionMatrix,
		FScreenToPortalMapping& OutMapping)
	{
		if (OutMapping.bAnalyticRayPlane && OutMapping.bValid)
		{
			const FVector LogicalCenter(OutMapping.Row0.X, OutMapping.Row0.Y, OutMapping.Row0.Z);
			const FVector Normal(OutMapping.Row1.X, OutMapping.Row1.Y, OutMapping.Row1.Z);
			const double SurfaceBias = FVector::DotProduct(
				ApertureFrame.GetLocation() - LogicalCenter, Normal);
			if (!FMath::IsFinite(SurfaceBias))
			{
				return false;
			}
			OutMapping.Row2.W = float(SurfaceBias);
			return true;
		}

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
		OutMapping.ClipZRow = FVector4f(
			float(Du.Z), float(Dv.Z), float(ClipCenter.Z), 0.0f);

		OutMapping.bValid = IsFiniteRow(OutMapping.Row0)
			&& IsFiniteRow(OutMapping.Row1)
			&& IsFiniteRow(OutMapping.Row2)
			&& IsFiniteRow(OutMapping.ClipZRow);
		return OutMapping.bValid;
	}

	/**
	 * Production aperture geometry for analytic per-pixel ray/plane ownership.
	 * No screen-space matrix inversion is performed, so the representation stays
	 * well-defined as the portal approaches an edge-on projection.
	 *
	 * Packed layout consumed by production composition/depth shaders:
	 *   Row0.xyz = logical portal center, Row0.w = 1 / HalfWidth
	 *   Row1.xyz = logical portal normal(+X), Row1.w = 1 / HalfHeight
	 *   Row2.xyz = logical portal width axis(+Y), Row2.w = cosmetic surface bias
	 *   ClipZRow.xyz = logical portal height axis(+Z)
	 */
	inline bool BuildAnalyticRayPlaneGeometry(
		const FTransform& ApertureFrame,
		double HalfWidth,
		double HalfHeight,
		double SurfaceVisualBias,
		FScreenToPortalMapping& OutGeometry)
	{
		OutGeometry = FScreenToPortalMapping();
		if (HalfWidth <= UE_SMALL_NUMBER || HalfHeight <= UE_SMALL_NUMBER
			|| !FMath::IsFinite(SurfaceVisualBias))
		{
			return false;
		}

		const FVector Center = ApertureFrame.GetLocation();
		const FVector Normal = ApertureFrame.GetUnitAxis(EAxis::X);
		const FVector AxisY = ApertureFrame.GetUnitAxis(EAxis::Y);
		const FVector AxisZ = ApertureFrame.GetUnitAxis(EAxis::Z);
		OutGeometry.Row0 = FVector4f(FVector3f(Center), float(1.0 / HalfWidth));
		OutGeometry.Row1 = FVector4f(FVector3f(Normal), float(1.0 / HalfHeight));
		OutGeometry.Row2 = FVector4f(FVector3f(AxisY), float(SurfaceVisualBias));
		OutGeometry.ClipZRow = FVector4f(FVector3f(AxisZ), 0.0f);
		OutGeometry.DeterminantQuality = 1.0f;
		OutGeometry.bValid = IsFiniteRow(OutGeometry.Row0)
			&& IsFiniteRow(OutGeometry.Row1)
			&& IsFiniteRow(OutGeometry.Row2)
			&& IsFiniteRow(OutGeometry.ClipZRow);
		OutGeometry.bAnalyticRayPlane = OutGeometry.bValid;
		return OutGeometry.bValid;
	}
}
