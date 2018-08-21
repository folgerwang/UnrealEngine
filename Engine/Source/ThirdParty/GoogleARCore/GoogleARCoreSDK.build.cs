// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GoogleARCoreSDK : ModuleRules
{
	public GoogleARCoreSDK(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

		string ARCoreSDKDir = Target.UEThirdPartySourceDirectory + "GoogleARCore/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					ARCoreSDKDir + "include/",
				}
			);

		string ARCoreSDKBaseLibPath = ARCoreSDKDir + "lib/";
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string ARCoreSDKArmLibPath = ARCoreSDKBaseLibPath + "armeabi-v7a/";
			string ARCoreSDKArm64LibPath = ARCoreSDKBaseLibPath + "arm64-v8a/";

			// toolchain will filter properly
			PublicLibraryPaths.Add(ARCoreSDKArmLibPath);
			PublicLibraryPaths.Add(ARCoreSDKArm64LibPath);

			PublicAdditionalLibraries.Add("arcore_sdk_c");
		}
		else if(Target.Platform == UnrealTargetPlatform.IOS)
		{
			string ARCoreSDKiOSLibPath = ARCoreSDKBaseLibPath + "ios/";
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGTMSessionFetcher.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGoogleToolboxForMac.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libProtobuf.a");

			PublicAdditionalLibraries.Add("sqlite3");
			PublicAdditionalLibraries.Add("z");

			PublicAdditionalFrameworks.Add(new UEBuildFramework("ARKit"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("AVFoundation"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreGraphics"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreImage"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreMotion"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreVideo"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("Foundation"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("ImageIO"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("QuartzCore"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("Security"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("UIKit"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("VideoToolbox"));

			PublicAdditionalFrameworks.Add(new UEBuildFramework("ARCore", "lib/ios/ARCore.embeddedframework.zip", "ARCore.framework/Resources/ARCoreResources.bundle"));
		}
	}
}
