// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MixedRealityCaptureFramework : ModuleRules
{
	public MixedRealityCaptureFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"MixedRealityCaptureFramework/Private"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MediaAssets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Media",
				"HeadMountedDisplay",
				"InputCore",
                "MediaUtils",
				"RenderCore",
                "OpenCVHelper",
                "OpenCV",
				"OpenCVLensDistortion",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
