// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ComposureLayersEditor : ModuleRules
{
	public ComposureLayersEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"SceneOutliner",

                "WorkspaceMenuStructure",
				"Composure",
				"ClassViewer",
				"DetailCustomizations",
				"PropertyEditor",
				"BlueprintMaterialTextureNodes",
				"AppFramework",
				"LevelEditor",
				"ContentBrowser",
				"MediaIOCore",
				"RHI",
				"RenderCore"
            }
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"UnrealEd",
			}
		);
	}
}
