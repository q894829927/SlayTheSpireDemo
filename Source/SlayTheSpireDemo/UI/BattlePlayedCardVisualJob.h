#pragma once
#include "../Presentation/PresentationCardVisualTypes.h"
#include "../Presentation/PresentationG9Types.h"
#include "Components/SlateWrapperTypes.h"
#include "BattlePlayedCardVisualJob.generated.h"
class UBattleCardWidget;
class UBattleHUDViewModel;

UENUM()
enum class ECardVisualPhase : uint8 { Prepared, EnteringPlayArea, AtPlayArea, DestinationTail };

USTRUCT()
struct FPreparedCardDestinationVisual
{
	GENERATED_BODY()
	UPROPERTY() FDetachedCardDestinationToken Token;
	UPROPERTY() FVector2D StartCenter = FVector2D::ZeroVector;
	UPROPERTY() FVector2D EndCenter = FVector2D::ZeroVector;
	UPROPERTY() float EndScale = 0.72f;
	UPROPERTY() float EndOpacity = 0;
	UPROPERTY() float Duration = 0.5f;
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
	UPROPERTY() bool bDetachedDestination = false;
	UPROPERTY() bool bDetachedArrival = false;
	UPROPERTY() bool bArrivalDestinationCommitted = false;
	UPROPERTY() FDetachedCardArrivalToken ArrivalToken;
	UPROPERTY() FPreparedCardDestinationVisual ArrivalDestination;
	UPROPERTY() FName SourcePresentationId;
	UPROPERTY() FPreparedCardDestinationVisual PreparedDestination;
	UPROPERTY() FDetachedCardDestinationToken LastDestinationPreparationToken;
	UPROPERTY() FVector2D DetachedStartCenter = FVector2D::ZeroVector;
	FCardPlayVisualOrigin Origin;
	UPROPERTY() FVector2D DesiredSize = FVector2D::ZeroVector;
	UPROPERTY() FVector2D DestinationCenter = FVector2D::ZeroVector;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() float Duration = 0.5f;
	UPROPERTY() float EndScale = 1;
	UPROPERTY() float EndOpacity = 1;
};
