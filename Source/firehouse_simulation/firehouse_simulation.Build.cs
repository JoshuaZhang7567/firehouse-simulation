using UnrealBuildTool;

public class firehouse_simulation : ModuleRules
{
	public firehouse_simulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "PhysicsCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}