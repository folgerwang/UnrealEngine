// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OodleHandlerComponent : ModuleRules
{
    public OodleHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
    {
		// @todo oodle: Clean this up with the compression format?


		// this needs to match the version in Oodle.Build.cs
		string OodleVersion = "255";

		ShortName = "OodleHC";

        BinariesSubFolder = "NotForLicensees";
		
		PrivateIncludePaths.Add("OodleHandlerComponent/Private");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"PacketHandler",
				"Core",
				"CoreUObject",
				"Engine",
                "Analytics"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Projects",
			});

		string PlatformName = Target.Platform.ToString();
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			PlatformName = "win";

			// this is needed to hunt down the DLL in the binaries directory for running unstaged
			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Projects",
			});
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PlatformName = "Linux";
		}

		// Check the NotForLicensees folder first
		string OodleNotForLicenseesLibDir = System.IO.Path.Combine(ModuleDirectory, "..", "ThirdParty", "NotForLicensees",
			"Oodle", OodleVersion, PlatformName, "lib");

		bool bHaveOodleSDK = false;
		if (OodleNotForLicenseesLibDir.Length > 0)
		{
			try
			{
				bHaveOodleSDK = System.IO.Directory.Exists( OodleNotForLicenseesLibDir );
			}
			catch ( System.Exception )
			{
			}
        }

		if ( bHaveOodleSDK )
		{
	        AddEngineThirdPartyPrivateStaticDependencies(Target, "Oodle");
	        PublicIncludePathModuleNames.Add("Oodle");
			PublicDefinitions.Add( "HAS_OODLE_SDK=1" );
		}
		else
		{
			PublicDefinitions.Add( "HAS_OODLE_SDK=0" );
		}
    }
}