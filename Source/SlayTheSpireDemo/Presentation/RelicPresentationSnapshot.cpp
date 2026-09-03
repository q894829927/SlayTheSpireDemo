#include "RelicPresentationSnapshot.h"

#include "../Battle/BattleReadSnapshot.h"
#include "../Relics/RelicData.h"
#include "../UI/BattleHUDTypes.h"

bool RelicPresentationSnapshot::TryFreeze(
	const TArray<FRelicReadView>& Relics,
	TArray<FBattleHUDRelicView>& OutRelics)
{
	OutRelics.Reset();
	OutRelics.Reserve(Relics.Num());
	TSet<FName> SeenRelicIds;

	for (const FRelicReadView& Source : Relics)
	{
		const URelicData* Definition = Source.Definition.Get();
		if (Source.RelicId.IsNone()
			|| Source.RuntimeSequence == 0
			|| Source.RuntimeSequence > static_cast<uint64>(MAX_int64)
			|| Source.Counter < 0
			|| !IsValid(Definition)
			|| SeenRelicIds.Contains(Source.RelicId)
			|| (Definition->bShowCounter && Definition->CounterDisplayMax <= 0))
		{
			OutRelics.Reset();
			return false;
		}

		SeenRelicIds.Add(Source.RelicId);

		FBattleHUDRelicView Frozen;
		Frozen.RelicId = Source.RelicId;
		Frozen.RuntimeSequence = static_cast<int64>(Source.RuntimeSequence);
		Frozen.DisplayName = Definition->DisplayName.IsEmpty()
			? FText::FromName(Source.RelicId)
			: Definition->DisplayName;
		Frozen.Description = Definition->Description;
		Frozen.bShowCounter = Definition->bShowCounter;
		Frozen.Counter = Source.Counter;
		Frozen.CounterMax = Definition->bShowCounter
			? Definition->CounterDisplayMax
			: 0;
		Frozen.Icon = Definition->Icon;
		OutRelics.Add(MoveTemp(Frozen));
	}

	OutRelics.Sort(
		[](const FBattleHUDRelicView& A, const FBattleHUDRelicView& B)
		{
			return A.RuntimeSequence < B.RuntimeSequence;
		}
	);

	for (int32 Index = 1; Index < OutRelics.Num(); ++Index)
	{
		if (OutRelics[Index - 1].RuntimeSequence >= OutRelics[Index].RuntimeSequence)
		{
			OutRelics.Reset();
			return false;
		}
	}

	return true;
}
