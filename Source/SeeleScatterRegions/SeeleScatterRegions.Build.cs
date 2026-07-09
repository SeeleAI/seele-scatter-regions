using UnrealBuildTool;

public class SeeleScatterRegions : ModuleRules
{
	public SeeleScatterRegions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json"
			}
		);
	}
}
