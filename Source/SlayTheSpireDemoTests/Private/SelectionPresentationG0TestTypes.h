#pragma once

#include "CoreMinimal.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDReconciledWidget.h"
#include "SelectionPresentationG0TestTypes.generated.h"

class UBattleHUDViewModel;
class UHorizontalBox;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG0CardProbe : public UBattleCardWidget
{
	GENERATED_BODY()

protected:
	// Identity/ownership tests do not render the card face. Skip the production
	// Designer binding contract so no fake Text/Image/Button hierarchy is needed.
	virtual void NativeOnInitialized() override {}
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API USelectionPresentationG0HUDProbe : public UBattleHUDReconciledWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void ConfigureFormalHandForTesting(
		UBattleHUDViewModel* InViewModel,
		UHorizontalBox* InHand);
	void RefreshFormalHandForTesting() { RefreshHand(); }
	int32 GetFormalHandChildCountForTesting() const;
	UBattleCardWidget* FindFormalHandCardForTesting(int32 RuntimeId) const;
	virtual UWorld* GetWorld() const override;

protected:
	// Tests exercise G0 Hand reconciliation/ownership only and deliberately avoid
	// the full production Designer binding contract.
	virtual void NativeOnInitialized() override {}

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
