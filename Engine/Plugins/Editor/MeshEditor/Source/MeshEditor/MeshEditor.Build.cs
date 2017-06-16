// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshEditor : ModuleRules
	{
        public MeshEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"RenderCore",
					"EditableMesh",
					"ViewportInteraction",
                    "VREditor",
					"Projects"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
                    "ContentBrowser",
					"LevelEditor",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
                    "ContentBrowser",
					"LevelEditor",
				}
			);
		}
	}
}