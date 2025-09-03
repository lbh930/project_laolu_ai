using UnrealBuildTool;
public class Chatbot : ModuleRules
{
    public Chatbot(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new[] {
            "HTTP", "Json", "JsonUtilities", "Slate", "SlateCore"
        });
    }
}
