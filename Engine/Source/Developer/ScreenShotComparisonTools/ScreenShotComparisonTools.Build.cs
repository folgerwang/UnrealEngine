// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparisonTools : ModuleRules
{
	public ScreenShotComparisonTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AutomationMessages",
                "EditorStyle",
				"ImageWrapper",
				"Json",
				"JsonUtilities",
				"Slate",
                "UnrealEdMessages",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "MessagingCommon",
            }
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ScreenShotComparisonTools/Private"
			}
		);

		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
