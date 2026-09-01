#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AssetRegistryInterface.h"
#include "Modules/ModuleManager.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDPresenter.h"
#include "UI/BattleHUDWidget.h"
#include "UI/BattleStatusWidget.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

namespace Phase6UIA2NR13Test
{
	const FName ProductionMapPackage(TEXT("/Game/SlayTheSpireDemo/Maps/L_BattleTest"));
	const FName PresenterPackage(TEXT("/Game/SlayTheSpireDemo/Blueprints/Battle/BP_BattleHUDPresenter"));
	const FName NativeHUDPackage(TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native"));
	const FName LegacyHUDPackage(TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD"));
	const FName LegacyCardPackage(TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard"));
	const FName LegacyStatusPackage(TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleStatus"));

	UClass* LoadWidgetClass(const TCHAR* ClassPath, UClass* RequiredBaseClass)
	{
		return StaticLoadClass(RequiredBaseClass, nullptr, ClassPath);
	}

	TSet<FName> GatherHardProductionDependencies(IAssetRegistry& AssetRegistry)
	{
		UE::AssetRegistry::FDependencyQuery HardQuery(
			UE::AssetRegistry::EDependencyQuery::Hard);
		TSet<FName> Visited;
		TArray<FName> Pending{ProductionMapPackage};

		while (!Pending.IsEmpty())
		{
			const FName PackageName = Pending.Pop(EAllowShrinking::No);
			if (Visited.Contains(PackageName))
			{
				continue;
			}

			Visited.Add(PackageName);
			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(
				PackageName,
				Dependencies,
				UE::AssetRegistry::EDependencyCategory::Package,
				HardQuery);
			for (const FName Dependency : Dependencies)
			{
				if (Dependency.ToString().StartsWith(TEXT("/Game/SlayTheSpireDemo"))
					&& !Visited.Contains(Dependency))
				{
					Pending.Add(Dependency);
				}
			}
		}

		return Visited;
	}
}

using namespace Phase6UIA2NR13Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeProductionAssetReferencesTest,
	"SlayTheSpireDemo.Phase6UIA2N.R13.AssetReferences.NativeProductionClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeProductionAssetReferencesTest::RunTest(const FString& Parameters)
{
	UClass* NativeHUDClass = LoadWidgetClass(
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"),
		UBattleHUDWidget::StaticClass());
	UClass* NativeCardClass = LoadWidgetClass(
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard_Native.WBP_BattleCard_Native_C"),
		UBattleCardWidget::StaticClass());
	UClass* NativeStatusClass = LoadWidgetClass(
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleStatus_Native.WBP_BattleStatus_Native_C"),
		UBattleStatusWidget::StaticClass());
	UClass* PresenterClass = StaticLoadClass(
		ABattleHUDPresenter::StaticClass(),
		nullptr,
		TEXT("/Game/SlayTheSpireDemo/Blueprints/Battle/BP_BattleHUDPresenter.BP_BattleHUDPresenter_C"));

	if (!TestNotNull(TEXT("Native HUD class loads"), NativeHUDClass)
		|| !TestNotNull(TEXT("Native Card class loads"), NativeCardClass)
		|| !TestNotNull(TEXT("Native Status class loads"), NativeStatusClass)
		|| !TestNotNull(TEXT("Presenter Blueprint class loads"), PresenterClass))
	{
		return false;
	}

	const ABattleHUDPresenter* PresenterCDO = PresenterClass->GetDefaultObject<ABattleHUDPresenter>();
	const UBattleHUDWidget* NativeHUDCDO = NativeHUDClass->GetDefaultObject<UBattleHUDWidget>();
	TestNotNull(TEXT("Presenter CDO exists"), PresenterCDO);
	TestNotNull(TEXT("Native HUD CDO exists"), NativeHUDCDO);
	if (!PresenterCDO || !NativeHUDCDO)
	{
		return false;
	}

	TestEqual(
		TEXT("Presenter default WidgetClass is Native"),
		PresenterCDO->WidgetClass.Get(),
		NativeHUDClass);
	TestEqual(
		TEXT("Native HUD CardWidgetClass is Native"),
		NativeHUDCDO->CardWidgetClass.Get(),
		NativeCardClass);
	TestEqual(
		TEXT("Native HUD StatusWidgetClass is Native"),
		NativeHUDCDO->StatusWidgetClass.Get(),
		NativeStatusClass);

	UPackage* MapPackage = LoadPackage(nullptr, *ProductionMapPackage.ToString(), LOAD_None);
	UWorld* ProductionWorld = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	if (!TestNotNull(TEXT("Production map loads"), ProductionWorld))
	{
		return false;
	}

	int32 PresenterCount = 0;
	for (AActor* Actor : ProductionWorld->PersistentLevel->Actors)
	{
		if (const ABattleHUDPresenter* Presenter = Cast<ABattleHUDPresenter>(Actor))
		{
			++PresenterCount;
			TestEqual(
				TEXT("Production map Presenter instance uses Native HUD"),
				Presenter->WidgetClass.Get(),
				NativeHUDClass);
		}
	}
	TestEqual(TEXT("Production map has exactly one Presenter"), PresenterCount, 1);

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.ScanPathsSynchronous({TEXT("/Game/SlayTheSpireDemo")}, true);
	const TSet<FName> ProductionDependencies = GatherHardProductionDependencies(AssetRegistry);
	const TSet<FName> LegacyPackages{LegacyHUDPackage, LegacyCardPackage, LegacyStatusPackage};

	int32 LegacyDependencyCount = 0;
	for (const FName LegacyPackage : LegacyPackages)
	{
		if (ProductionDependencies.Contains(LegacyPackage))
		{
			++LegacyDependencyCount;
			AddError(FString::Printf(
				TEXT("Production hard dependency closure contains Legacy asset %s"),
				*LegacyPackage.ToString()));
		}
	}
	TestEqual(TEXT("Production runtime Legacy HUD/Card/Status dependency count"), LegacyDependencyCount, 0);

	UE::AssetRegistry::FDependencyQuery HardQuery(UE::AssetRegistry::EDependencyQuery::Hard);
	TArray<FName> NativeHUDDependencies;
	AssetRegistry.GetDependencies(
		NativeHUDPackage,
		NativeHUDDependencies,
		UE::AssetRegistry::EDependencyCategory::Package,
		HardQuery);
	int32 NativeHUDDirectLegacyDependencyCount = 0;
	for (const FName Dependency : NativeHUDDependencies)
	{
		NativeHUDDirectLegacyDependencyCount += LegacyPackages.Contains(Dependency) ? 1 : 0;
	}
	TestEqual(
		TEXT("Native HUD direct Legacy Card/Status dependency count"),
		NativeHUDDirectLegacyDependencyCount,
		0);

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
