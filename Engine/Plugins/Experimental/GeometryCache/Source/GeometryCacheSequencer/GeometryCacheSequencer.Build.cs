// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCacheSequencer : ModuleRules
	{
        public GeometryCacheSequencer(ReadOnlyTargetRules Target) : base(Target)
        {
            OptimizeCode = CodeOptimization.Never;

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Sequencer",
                }
            );


            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AssetTools",
                    "Core",
                    "CoreUObject",
					"EditorStyle",
					"Engine",
					"MovieScene",
					"MovieSceneTools",
					"MovieSceneTracks",
					"RHI",
					"Slate",
					"SlateCore",
                    "TimeManagement",
					"UnrealEd",
                    "GeometryCacheTracks",
                    "GeometryCache"
                }
			);
        }
	}
}
