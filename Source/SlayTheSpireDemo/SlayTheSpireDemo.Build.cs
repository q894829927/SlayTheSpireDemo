// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlayTheSpireDemo : ModuleRules
{
	public SlayTheSpireDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
		
		// Slate/SlateCore are intentionally not direct gameplay dependencies here.
		// Phase 6UI-A1 uses UMG public types; concrete visual layout remains in Widget Blueprints.
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
