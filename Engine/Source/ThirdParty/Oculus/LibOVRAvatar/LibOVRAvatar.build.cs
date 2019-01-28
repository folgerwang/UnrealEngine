// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LibOVRAvatar : ModuleRules
{
	public LibOVRAvatar(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;		
		
		string OculusThirdPartyDirectory = Target.UEThirdPartySourceDirectory + "Oculus/LibOVRAvatar/LibOVRAvatar";

		bool isLibrarySupported = false;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(OculusThirdPartyDirectory + "/lib/win64/libovravatar.lib");
			isLibrarySupported = true;
		}
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(OculusThirdPartyDirectory + "/lib/armeabi-v7a/libovravatarloader.so");
            string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "LibOVRAvatar_APL.xml"));
            isLibrarySupported = true;
        }
        else
		{
			System.Console.WriteLine("Oculus Avatar SDK not supported for this platform");
		}
		
		if (isLibrarySupported)
		{
			PublicIncludePaths.Add(Path.Combine( OculusThirdPartyDirectory, "include" ));
		}
	}
}
