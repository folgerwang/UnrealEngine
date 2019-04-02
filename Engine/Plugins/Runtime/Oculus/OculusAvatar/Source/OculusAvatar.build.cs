// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class OculusAvatar : ModuleRules
{
	public OculusAvatar(ReadOnlyTargetRules Target) : base(Target)
	{
        string BaseDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, ".."));

        PublicIncludePaths.AddRange(
            new string[] {
                BaseDirectory + "/Source/Public",
                BaseDirectory + "/../OculusVR/Source/OculusHMD/Private"
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "RenderCore",
                "HeadMountedDisplay"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "OculusHMD"
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
                "OVRPlugin"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] 
            {
                "OVRPlugin",
                "OculusHMD",
                "LibOVRAvatar"
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDelayLoadDLLs.Add("libovravatar.dll");
            PublicDelayLoadDLLs.Add("OVRPlugin.dll");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OVRPlugin.dll");
        }
        else if (Target.Platform != UnrealTargetPlatform.Android)
        {
			PrecompileForTargets = PrecompileTargetsType.None;
		}
    }
}
