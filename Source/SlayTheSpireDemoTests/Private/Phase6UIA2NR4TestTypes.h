#pragma once

#include "CoreMinimal.h"
#include "UI/BattleCardWidget.h"
#include "Phase6UIA2NR4TestTypes.generated.h"

class UButton;
class UImage;
class UTextBlock;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR4CardProbe : public UBattleCardWidget
{
	GENERATED_BODY()

public:
	void ConfigureSurfaces(
		UButton* InButton,
		UTextBlock* InName,
		UTextBlock* InCost,
		UTextBlock* InDescription,
		UTextBlock* InType,
		UImage* InArt);

	void InvokeCardClickForTesting();
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR4RequestSink : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleCardRequested(int32 RuntimeId);

	int32 CallCount = 0;
	int32 LastRuntimeId = INDEX_NONE;
};
