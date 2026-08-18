using UnrealBuildTool;
using System.IO;

public class SlayTheSpireDemoTests : ModuleRules
{
	public SlayTheSpireDemoTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SlayTheSpireDemo"
		});

		// Runtime headers currently live under the Runtime module root rather than Public/.
		// Keep that legacy access private to the test module instead of widening Runtime's
		// public include surface solely for Automation.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "SlayTheSpireDemo", "Tests"));
	}
}
