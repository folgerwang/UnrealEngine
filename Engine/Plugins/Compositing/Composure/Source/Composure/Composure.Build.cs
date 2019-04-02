// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Composure : ModuleRules
	{
		public Composure(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
                    "../../../../Source/Runtime/Engine/",
					"Composure/Private/"
				}
				);
            
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "CoreUObject",
                    "Engine",
					"MovieScene",
					"MovieSceneTracks",
					"TimeManagement",
					"CinematicCamera",
                    "MediaIOCore",
					"OpenColorIO",
                }
				);

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "RHI",

					// Removed dependency, until the MediaFrameworkUtilities plugin is available for all platforms
                    //"MediaFrameworkUtilities",

                    "MediaAssets",
					"MovieSceneCapture",
					"ImageWriteQueue",
                }
                );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.AddRange(
					new string[]
                    {
                        "UnrealEd",
                        "Slate",
                        "SlateCore",
						"EditorStyle"
                    }
					);
            }
        }
    }
}
