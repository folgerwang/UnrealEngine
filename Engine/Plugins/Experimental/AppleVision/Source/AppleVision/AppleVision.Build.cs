// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleVision : ModuleRules
	{
		public AppleVision(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.AddRange(
                new string[] {
                    // ... add public include paths required here ...
                }
                );


            PrivateIncludePaths.AddRange(
                new string[] {
                    "AppleVision/Public",
                    "AppleVision/Private"
                    // ... add other private include paths required here ...
                }
                );


            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Engine",
                    "AppleImageUtils"
                    // ... add other public dependencies that you statically link with here ...
                }
                );

    		if (Target.Platform == UnrealTargetPlatform.IOS)
    		{
                PublicFrameworks.AddRange(
                    new string[]
                    {
                        "CoreImage",
                        "Vision"
                        // ... add other public dependencies that you statically link with here ...
                    }
                    );
            }

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
    				"CoreUObject"
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}
