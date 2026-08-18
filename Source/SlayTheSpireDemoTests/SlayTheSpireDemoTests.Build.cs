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
		// The migrated tests intentionally keep their existing ../Actions, ../Status, ...
		// include shapes. Anchor those private legacy paths at one real Runtime subdirectory
		// so "../<Area>" resolves inside SlayTheSpireDemo without widening Runtime's public
		// include surface solely for Automation.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "SlayTheSpireDemo", "Actions"));
	}
}
