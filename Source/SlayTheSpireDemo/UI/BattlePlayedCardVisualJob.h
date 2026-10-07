#pragma once
#include "../Presentation/PresentationCardReducer.h"
#include "../Presentation/PresentationG9Types.h"
#include "Components/SlateWrapperTypes.h"
#include "BattlePlayedCardVisualJob.generated.h"
class UBattleCardWidget;
class UBattleHUDViewModel;

UENUM()
enum class ECardVisualPhase : uint8 { Prepared, EnteringPlayArea, AtPlayArea, DestinationTail };

USTRUCT()
struct FDetachedCardVisualToken
{
	GENERATED_BODY()
	UPROPERTY() FPlayedCardPresentationLifecycleToken Lifecycle;
	UPROPERTY() int64 LocalVisualGeneration = 0;
	bool IsValid() const { return Lifecycle.IsValid() && LocalVisualGeneration > 0; }
	bool operator==(const FDetachedCardVisualToken& Other) const
	{ return Lifecycle == Other.Lifecycle && LocalVisualGeneration == Other.LocalVisualGeneration; }
};

USTRUCT()
struct FNativePlayedCardVisualJob
{
	GENERATED_BODY()
	UPROPERTY() FDetachedCardVisualToken Token;
	UPROPERTY() FPresentationPlaybackToken BlockingToken;
	UPROPERTY() TObjectPtr<UBattleCardWidget> Widget;
	UPROPERTY() TWeakObjectPtr<UBattleCardWidget> SourceHandWidget;
	UPROPERTY() TWeakObjectPtr<UBattleHUDViewModel> ViewModel;
	UPROPERTY() uint64 SurfaceGeneration = 0;
	UPROPERTY() ESlateVisibility SourceVisibility = ESlateVisibility::Visible;
	UPROPERTY() ECardVisualPhase Phase = ECardVisualPhase::Prepared;
	FCardPlayVisualOrigin Origin;
	UPROPERTY() FVector2D DesiredSize = FVector2D::ZeroVector;
	UPROPERTY() FVector2D DestinationCenter = FVector2D::ZeroVector;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() float Duration = 0.5f;
	UPROPERTY() float EndScale = 1;
	UPROPERTY() float EndOpacity = 1;
};
