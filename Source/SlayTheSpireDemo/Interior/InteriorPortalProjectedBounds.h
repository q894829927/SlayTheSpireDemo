#pragma once

#include "CoreMinimal.h"

namespace InteriorPortalProjectedBounds
{
	constexpr int32 DefaultOverscanPixels = 4;
	constexpr int32 TargetAlignmentPixels = 32;
	constexpr int32 PingPongBufferCount = 2;

	inline int32 PingPongSlotForLevel(const int32 Level)
	{
		return Level >= 0 ? (Level % PingPongBufferCount) : INDEX_NONE;
	}

	inline bool ExpandAndClampRect(
		const FIntRect& SourceRect,
		const FIntRect& ParentViewRect,
		const int32 PaddingPixels,
		FIntRect& OutRect)
	{
		OutRect = FIntRect(0, 0, 0, 0);
		if (SourceRect.Width() <= 0 || SourceRect.Height() <= 0
			|| ParentViewRect.Width() <= 0 || ParentViewRect.Height() <= 0)
		{
			return false;
		}

		const int32 Padding = FMath::Max(PaddingPixels, 0);
		OutRect.Min.X = FMath::Clamp(SourceRect.Min.X - Padding, ParentViewRect.Min.X, ParentViewRect.Max.X);
		OutRect.Min.Y = FMath::Clamp(SourceRect.Min.Y - Padding, ParentViewRect.Min.Y, ParentViewRect.Max.Y);
		OutRect.Max.X = FMath::Clamp(SourceRect.Max.X + Padding, ParentViewRect.Min.X, ParentViewRect.Max.X);
		OutRect.Max.Y = FMath::Clamp(SourceRect.Max.Y + Padding, ParentViewRect.Min.Y, ParentViewRect.Max.Y);
		return OutRect.Width() > 0 && OutRect.Height() > 0;
	}

	inline bool PixelRectToNormalizedBounds(
		const FIntRect& CropRect,
		const FIntRect& ParentViewRect,
		FVector4f& OutBounds)
	{
		OutBounds = FVector4f(0, 0, 1, 1);
		if (ParentViewRect.Width() <= 0 || ParentViewRect.Height() <= 0
			|| CropRect.Width() <= 0 || CropRect.Height() <= 0
			|| CropRect.Min.X < ParentViewRect.Min.X || CropRect.Min.Y < ParentViewRect.Min.Y
			|| CropRect.Max.X > ParentViewRect.Max.X || CropRect.Max.Y > ParentViewRect.Max.Y)
		{
			return false;
		}

		const float InvWidth = 1.0f / float(ParentViewRect.Width());
		const float InvHeight = 1.0f / float(ParentViewRect.Height());
		OutBounds = FVector4f(
			float(CropRect.Min.X - ParentViewRect.Min.X) * InvWidth,
			float(CropRect.Min.Y - ParentViewRect.Min.Y) * InvHeight,
			float(CropRect.Max.X - ParentViewRect.Min.X) * InvWidth,
			float(CropRect.Max.Y - ParentViewRect.Min.Y) * InvHeight);
		return OutBounds.Z > OutBounds.X && OutBounds.W > OutBounds.Y;
	}

	/**
	 * Build a projection whose full NDC [-1,+1] viewport represents CropRect
	 * inside ParentViewRect. UE matrices transform row vectors, so the clip-space
	 * crop transform is post-multiplied onto ParentProjection.
	 */
	inline bool BuildCroppedProjection(
		const FMatrix& ParentProjection,
		const FIntRect& ParentViewRect,
		const FIntRect& CropRect,
		FMatrix& OutProjection)
	{
		OutProjection = ParentProjection;
		if (ParentViewRect.Width() <= 0 || ParentViewRect.Height() <= 0
			|| CropRect.Width() <= 0 || CropRect.Height() <= 0
			|| CropRect.Min.X < ParentViewRect.Min.X || CropRect.Min.Y < ParentViewRect.Min.Y
			|| CropRect.Max.X > ParentViewRect.Max.X || CropRect.Max.Y > ParentViewRect.Max.Y)
		{
			return false;
		}

		const double InvParentWidth = 1.0 / double(ParentViewRect.Width());
		const double InvParentHeight = 1.0 / double(ParentViewRect.Height());
		const double MinU = double(CropRect.Min.X - ParentViewRect.Min.X) * InvParentWidth;
		const double MaxU = double(CropRect.Max.X - ParentViewRect.Min.X) * InvParentWidth;
		const double MinV = double(CropRect.Min.Y - ParentViewRect.Min.Y) * InvParentHeight;
		const double MaxV = double(CropRect.Max.Y - ParentViewRect.Min.Y) * InvParentHeight;

		const double MinNdcX = MinU * 2.0 - 1.0;
		const double MaxNdcX = MaxU * 2.0 - 1.0;
		const double MinNdcY = 1.0 - MaxV * 2.0;
		const double MaxNdcY = 1.0 - MinV * 2.0;
		const double WidthNdc = MaxNdcX - MinNdcX;
		const double HeightNdc = MaxNdcY - MinNdcY;
		if (WidthNdc <= UE_SMALL_NUMBER || HeightNdc <= UE_SMALL_NUMBER)
		{
			return false;
		}

		FMatrix ClipCrop = FMatrix::Identity;
		ClipCrop.M[0][0] = 2.0 / WidthNdc;
		ClipCrop.M[1][1] = 2.0 / HeightNdc;
		ClipCrop.M[3][0] = -(MaxNdcX + MinNdcX) / WidthNdc;
		ClipCrop.M[3][1] = -(MaxNdcY + MinNdcY) / HeightNdc;
		OutProjection = ParentProjection * ClipCrop;
		return true;
	}

	inline FIntPoint ComputeAlignedTargetSize(
		const FIntRect& CropRect,
		const FIntRect& ParentViewRect,
		const FIntPoint& ParentRenderSize)
	{
		if (CropRect.Width() <= 0 || CropRect.Height() <= 0
			|| ParentViewRect.Width() <= 0 || ParentViewRect.Height() <= 0
			|| ParentRenderSize.X <= 0 || ParentRenderSize.Y <= 0)
		{
			return FIntPoint::ZeroValue;
		}

		const double ScaleX = double(ParentRenderSize.X) / double(ParentViewRect.Width());
		const double ScaleY = double(ParentRenderSize.Y) / double(ParentViewRect.Height());
		const int32 RawWidth = FMath::Max(1, FMath::CeilToInt(double(CropRect.Width()) * ScaleX));
		const int32 RawHeight = FMath::Max(1, FMath::CeilToInt(double(CropRect.Height()) * ScaleY));
		const int32 Alignment = TargetAlignmentPixels;
		const int32 AlignedWidth = FMath::Min(
			ParentRenderSize.X,
			FMath::Max(FMath::Min(Alignment, ParentRenderSize.X),
				FMath::DivideAndRoundUp(RawWidth, Alignment) * Alignment));
		const int32 AlignedHeight = FMath::Min(
			ParentRenderSize.Y,
			FMath::Max(FMath::Min(Alignment, ParentRenderSize.Y),
				FMath::DivideAndRoundUp(RawHeight, Alignment) * Alignment));
		return FIntPoint(AlignedWidth, AlignedHeight);
	}

	/** Map a rectangle between equal-normalized-coordinate texture extents. */
	inline bool ScaleRectBetweenExtents(
		const FIntRect& SourceRect,
		const FIntPoint& SourceExtent,
		const FIntPoint& DestinationExtent,
		FIntRect& OutRect)
	{
		OutRect = FIntRect(0, 0, 0, 0);
		if (SourceExtent.X <= 0 || SourceExtent.Y <= 0
			|| DestinationExtent.X <= 0 || DestinationExtent.Y <= 0
			|| SourceRect.Width() <= 0 || SourceRect.Height() <= 0
			|| SourceRect.Min.X < 0 || SourceRect.Min.Y < 0
			|| SourceRect.Max.X > SourceExtent.X || SourceRect.Max.Y > SourceExtent.Y)
		{
			return false;
		}

		const double ScaleX = double(DestinationExtent.X) / double(SourceExtent.X);
		const double ScaleY = double(DestinationExtent.Y) / double(SourceExtent.Y);
		OutRect.Min.X = FMath::Clamp(FMath::FloorToInt(double(SourceRect.Min.X) * ScaleX), 0, DestinationExtent.X);
		OutRect.Min.Y = FMath::Clamp(FMath::FloorToInt(double(SourceRect.Min.Y) * ScaleY), 0, DestinationExtent.Y);
		OutRect.Max.X = FMath::Clamp(FMath::CeilToInt(double(SourceRect.Max.X) * ScaleX), 0, DestinationExtent.X);
		OutRect.Max.Y = FMath::Clamp(FMath::CeilToInt(double(SourceRect.Max.Y) * ScaleY), 0, DestinationExtent.Y);
		return OutRect.Width() > 0 && OutRect.Height() > 0;
	}
}
