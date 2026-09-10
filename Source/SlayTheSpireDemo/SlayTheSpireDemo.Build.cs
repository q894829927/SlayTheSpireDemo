// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlayTheSpireDemo : ModuleRules
{
	public SlayTheSpireDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });

		// Native HUD preview mouse routing returns Slate FReply. Keep this engine
		// dependency private; no Slate types cross the Gameplay/public boundary.
		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore" });
		
		// Gameplay still exposes no Slate types; this private SlateCore dependency is
		// limited to the Native HUD's FReply input boundary.
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
