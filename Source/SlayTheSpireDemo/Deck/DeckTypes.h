#pragma once

#include "CoreMinimal.h"
#include "DeckTypes.generated.h"

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDeckCardToken
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deck")
	int32 RuntimeId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deck")
	FName DebugName = NAME_None;

	bool IsValid() const
	{
		return RuntimeId > 0;
	}
};
