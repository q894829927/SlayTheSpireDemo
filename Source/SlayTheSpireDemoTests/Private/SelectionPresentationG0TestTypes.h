#pragma once

#include "CoreMinimal.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDReconciledWidget.h"
#include "SelectionPresentationG0TestTypes.generated.h"

class UHorizontalBox;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG0CardProbe : public UBattleCardWidget
{
	GENERATED_BODY()

protected:
	// The identity test does not render a card face. Skip the production Designer
	// binding contract so no fake Text/Image/Button hierarchy is needed.
	virtual void NativeOnInitialized() override {}
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG0HUDProbe : public UBattleHUDReconciledWidget
{
	GENERATED_BODY()

public:
	void ConfigureFormalHandForTesting();
	void RefreshFormalHandForTesting() { RefreshHand(); }
	int32 GetFormalHandChildCountForTesting() const;
	UBattleCardWidget* FindFormalHandCardForTesting(int32 RuntimeId) const;

protected:
	// The test exercises only G0 Hand reconciliation/ownership and deliberately
	// avoids the full production Designer binding contract.
	virtual void NativeOnInitialized() override {}
};
