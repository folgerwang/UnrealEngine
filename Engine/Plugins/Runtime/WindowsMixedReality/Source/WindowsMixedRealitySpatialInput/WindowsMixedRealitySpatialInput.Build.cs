// Copyright (c) Microsoft Corporation. All rights reserved.

using System;
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class WindowsMixedRealitySpatialInput : ModuleRules
	{
		private string ModulePath
		{
			get { return ModuleDirectory; }
		}
	 
		private string ThirdPartyPath
		{
			get { return Path.GetFullPath( Path.Combine( ModulePath, "../ThirdParty/" ) ); }
		}
		
		private void LoadMixedReality(ReadOnlyTargetRules Target)
        {
            string LibrariesPath = Path.Combine(ThirdPartyPath, "Lib", "x64");

            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                LibrariesPath = Path.Combine(ThirdPartyPath, "Lib", "x86");
            }

            PublicLibraryPaths.Add(LibrariesPath);
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "MixedRealityInterop.lib"));

            // Win10 support
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "onecore.lib"));

            PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include"));
        }
		
        public WindowsMixedRealitySpatialInput(ReadOnlyTargetRules Target) : base(Target)
        {	
            PrivateIncludePathModuleNames.AddRange(
                new string[]
				{
					"TargetPlatform",
                    "InputDevice",
					"HeadMountedDisplay",
					"WindowsMixedRealityHMD"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "HeadMountedDisplay",
                    "WindowsMixedRealityHMD"
				});

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }

            if (Target.Platform == UnrealTargetPlatform.Win32 ||
                Target.Platform == UnrealTargetPlatform.Win64)
            {
				LoadMixedReality(Target);
				
                bFasterWithoutUnity = true;
                bEnableExceptions = true;

                PCHUsage = PCHUsageMode.NoSharedPCHs;
                PrivatePCHHeaderFile = "Private/WindowsMixedRealitySpatialInput.h";
            }
        }
    }
}