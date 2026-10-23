// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gesith : ModuleRules
{
	public Gesith(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Gesith",
			"Gesith/Castle",
			"Gesith/Variant_Platforming",
			"Gesith/Variant_Platforming/Animation",
			"Gesith/Variant_Combat",
			"Gesith/Variant_Combat/AI",
			"Gesith/Variant_Combat/Animation",
			"Gesith/Variant_Combat/Gameplay",
			"Gesith/Variant_Combat/Interfaces",
			"Gesith/Variant_Combat/UI",
			"Gesith/Variant_SideScrolling",
			"Gesith/Variant_SideScrolling/AI",
			"Gesith/Variant_SideScrolling/Gameplay",
			"Gesith/Variant_SideScrolling/Interfaces",
			"Gesith/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
