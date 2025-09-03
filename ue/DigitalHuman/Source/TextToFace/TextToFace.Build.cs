using UnrealBuildTool;

public class TextToFace : ModuleRules
{
    public TextToFace(ReadOnlyTargetRules Target) : base(Target)
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
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );
        
        PrivateDependencyModuleNames.AddRange(new string[] {
            "HTTP", "Json", "JsonUtilities",
            "UMG",              // 如果只在主模块放 UI，可移回主模块；留着也不冲突
            "ACERuntime"        // NVIDIA ACE Runtime
        });
    }
}