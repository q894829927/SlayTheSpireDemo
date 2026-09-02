#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "BattleImmediatePreviewTextBlock.generated.h"

class UBattleHUDViewModel;
class UOverlay;

/**
 * Native-only transient A3 surface. It observes the ViewModel DTO/formatting only
 * and never queries Gameplay or shares the A2 committed damage-number surface.
 */
UCLASS(Transient)
class SLAYTHESPIREDEMO_API UBattleImmediatePreviewTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	void AttachToPreview(UBattleHUDViewModel* InViewModel, UOverlay* InHostOverlay);
	void DetachFromPreview();

protected:
	virtual void BeginDestroy() override;

private:
	UFUNCTION()
	void HandlePreviewChanged();

	TWeakObjectPtr<UBattleHUDViewModel> BoundViewModel;
	TWeakObjectPtr<UOverlay> HostOverlay;
};
