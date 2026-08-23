using UnrealBuildTool;
using System.IO;

public class SlayTheSpireDemoTests : ModuleRules
{
	public SlayTheSpireDemoTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"SlayTheSpireDemo"
		});

		// Runtime headers currently live under the Runtime module root rather than Public/.
		// Keep both paths private to this Editor-only test module: the Runtime root supports
		// module-root-relative includes in reflected test headers, while the real Actions/
		// subdirectory anchors the migrated tests' existing ../Actions, ../Status, ...
		// include shapes. Runtime's public include surface is not widened for Automation.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "SlayTheSpireDemo"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "SlayTheSpireDemo", "Actions"));
	}
}
