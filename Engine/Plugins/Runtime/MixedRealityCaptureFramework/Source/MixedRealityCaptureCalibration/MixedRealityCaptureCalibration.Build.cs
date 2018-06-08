// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MixedRealityCaptureCalibration : ModuleRules
{
	public MixedRealityCaptureCalibration(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
                "MixedRealityCaptureCalibration/Private"
            }
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "MixedRealityCaptureFramework",
				"MediaAssets",
				"InputCore",
                "OpenCVHelper",
                "OpenCV",
                "HeadMountedDisplay"
            }
        );
	}
}
