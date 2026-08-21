#pragma once

#include "CoreMinimal.h"
#include "../Status/StatusMutationTypes.h"

class ABattleManager;
class ACombatant;
class UStatusInstance;
struct FPresentationRecordWriter;

namespace StatusPresentation
{
	SLAYTHESPIREDEMO_API FText FreezeDescription(const UStatusInstance* Instance);

	SLAYTHESPIREDEMO_API bool AppendCommittedChange(
		const FPresentationRecordWriter& Writer,
		ABattleManager* Battle,
		ACombatant* Source,
		ACombatant* Target,
		const FStatusMutationResult& Mutation,
		EStatusChangeReason Reason,
		const FText& DescriptionBefore,
		const FText& DescriptionAfter
	);
}
