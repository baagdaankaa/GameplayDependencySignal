using UnrealBuildTool;

public class DependencySignalEditor : ModuleRules
{
    public DependencySignalEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "BlueprintGraph",
                "CoreUObject",
                "DependencySignalRuntime",
                "Engine",
                "GameplayTags",
                "KismetCompiler",
                "UnrealEd",
            }
        );
    }
}
