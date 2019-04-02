// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
    public GameProjectGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"HardwareTargeting",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
				"DesktopPlatform",
				"LauncherPlatform",
				"MainFrame",
				"AddContentDialog",
				"HardwareTargeting",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"ApplicationCore",
				"AppFramework",
				"ClassViewer",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"InputCore",
				"Projects",
				"RenderCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"SourceControl",
 				"TargetPlatform",
				"UnrealEd",
				"DesktopPlatform",
				"LauncherPlatform",
				"HardwareTargeting",
				"AddContentDialog",
                "AudioMixer"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "Documentation",
                "MainFrame",
            }
		);

		if(Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
