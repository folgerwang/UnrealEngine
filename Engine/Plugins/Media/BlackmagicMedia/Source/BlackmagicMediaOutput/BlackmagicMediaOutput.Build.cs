// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlackmagicMediaOutput : ModuleRules
	{
		public BlackmagicMediaOutput(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"BlackmagicMedia"
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"BlackmagicMediaOutput/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Blackmagic",
                    "Core",
					"CoreUObject",
					"Engine",
                    "MediaIOCore",
					"MovieSceneCapture",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
                    "TimeManagement"
                }
            );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
		}
	}
}
