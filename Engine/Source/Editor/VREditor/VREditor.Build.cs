// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VREditor : ModuleRules
	{
        public VREditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.Add(ModuleDirectory);

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "AppFramework",
				    "Core",
				    "CoreUObject",
					"ApplicationCore",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
				    "UnrealEd",
					"UMG",
					"LevelEditor",
					"HeadMountedDisplay",
					"Analytics",
                    "LevelSequence",
                    "Sequencer",
                    "Projects"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"ViewportInteraction"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"PlacementMode"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
                    "PlacementMode"
                }
			);

            PrivateIncludePaths.AddRange(
                new string[] {
                    "Editor/VREditor/UI",
                    "Editor/VREditor/Teleporter",
                }
            );

		}
	}
}