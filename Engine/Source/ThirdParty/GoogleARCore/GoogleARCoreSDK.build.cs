// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
			string ARCoreSDKx86LibPath = ARCoreSDKBaseLibPath + "x86/";

			// toolchain will filter properly
			PublicLibraryPaths.Add(ARCoreSDKArmLibPath);
			PublicLibraryPaths.Add(ARCoreSDKArm64LibPath);
			PublicLibraryPaths.Add(ARCoreSDKx86LibPath);

			PublicAdditionalLibraries.Add("arcore_sdk_c");
		}
		else if(Target.Platform == UnrealTargetPlatform.IOS)
		{
			string ARCoreSDKiOSLibPath = ARCoreSDKBaseLibPath + "ios/";
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGTMSessionFetcher.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libGoogleToolboxForMac.a");
			PublicAdditionalLibraries.Add(ARCoreSDKiOSLibPath + "libProtobuf.a");

			PublicAdditionalLibraries.Add("c++");
			PublicAdditionalLibraries.Add("sqlite3");
			PublicAdditionalLibraries.Add("z");

			PublicAdditionalFrameworks.Add(new Framework("ARKit"));
			PublicAdditionalFrameworks.Add(new Framework("AVFoundation"));
			PublicAdditionalFrameworks.Add(new Framework("CoreGraphics"));
			PublicAdditionalFrameworks.Add(new Framework("CoreImage"));
			PublicAdditionalFrameworks.Add(new Framework("CoreMotion"));
			PublicAdditionalFrameworks.Add(new Framework("CoreMedia"));
			PublicAdditionalFrameworks.Add(new Framework("CoreVideo"));
			PublicAdditionalFrameworks.Add(new Framework("Foundation"));
			PublicAdditionalFrameworks.Add(new Framework("ImageIO"));
			PublicAdditionalFrameworks.Add(new Framework("QuartzCore"));
			PublicAdditionalFrameworks.Add(new Framework("Security"));
			PublicAdditionalFrameworks.Add(new Framework("UIKit"));
			PublicAdditionalFrameworks.Add(new Framework("VideoToolbox"));

			PublicAdditionalFrameworks.Add(new Framework("ARCore", "lib/ios/ARCore.embeddedframework.zip", "ARCore.framework/Resources/ARCoreResources.bundle"));
		}
	}
}
