// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class DigitalHuman : ModuleRules
{
	public DigitalHuman(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "A2FCommon"});

		PrivateDependencyModuleNames.AddRange(new string[] { "ACERuntime", "UMG" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "ACECore" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "HTTP", "Json", "JsonUtilities" });
		
		//Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "TextToFace" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "Chatbot" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "WebInterface" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
