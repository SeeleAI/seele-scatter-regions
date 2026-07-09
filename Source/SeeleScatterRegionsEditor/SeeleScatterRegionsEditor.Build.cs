using UnrealBuildTool;

public class SeeleScatterRegionsEditor : ModuleRules
{
	public SeeleScatterRegionsEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"Json",
				"SeeleScatterRegions"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"UnrealEd",
				"Landscape",
				"EditorSubsystem"
			}
		);
	}
}
