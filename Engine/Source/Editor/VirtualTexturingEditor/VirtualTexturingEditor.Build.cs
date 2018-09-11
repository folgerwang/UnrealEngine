// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualTexturingEditor : ModuleRules
{
	public VirtualTexturingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Editor/VirtualTexturingEditor/Private");

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "MainFrame",
                "DesktopPlatform",
                "ContentBrowser",
                "AssetTools"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "AppFramework",
                "AssetRegistry",
                "Core",
                "CoreUObject",
                "ContentBrowser",
                "DesktopPlatform",
                "DesktopWidgets",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
                "PropertyEditor"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "DesktopPlatform",
                "MainFrame",
                "UnrealEd",
            }
        );
    }
}