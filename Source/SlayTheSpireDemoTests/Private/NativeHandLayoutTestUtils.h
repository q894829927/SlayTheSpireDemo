#pragma once

#include "UI/BattleHandFanPanel.h"
#include "Widgets/SWidget.h"

namespace NativeHandLayoutTest
{
	using FArrangedCards = TArray<TPair<UWidget*, FGeometry>>;
	inline FArrangedCards Arrange(UBattleHandFanPanel* Panel, const FVector2D& Size)
	{
		Panel->TakeWidget();
		FArrangedCards Children;
		Panel->GetArrangedCardGeometries(FGeometry::MakeRoot(Size, FSlateLayoutTransform()), Children);
		return Children;
	}
	inline const FGeometry* Find(const FArrangedCards& Children, UWidget* Card)
	{
		for (int32 Index = 0; Index < Children.Num(); ++Index)
			if (Children[Index].Key == Card) return &Children[Index].Value;
		return nullptr;
	}
	inline FVector2D Position(const FGeometry& Geometry)
	{
		return FVector2D(Geometry.GetAccumulatedLayoutTransform().GetTranslation());
	}
}
