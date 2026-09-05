#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidget.h"
#include "CFVCardMetadataTestTypes.generated.h"

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UCFVCardMetadataProbeWidget : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	bool InvokeIsNativeCardSnapshotValid(const FPresentationCardSnapshot& Snapshot) const
	{
		return IsNativeCardSnapshotValid(Snapshot);
	}

	bool InvokeDoesNativeCardViewMatchSnapshot(
		const FBattleHUDCardView& View,
		const FPresentationCardSnapshot& Snapshot) const
	{
		return DoesNativeCardViewMatchSnapshot(View, Snapshot);
	}
};
