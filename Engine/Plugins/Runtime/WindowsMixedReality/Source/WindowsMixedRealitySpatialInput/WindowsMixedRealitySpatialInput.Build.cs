// Copyright (c) Microsoft Corporation. All rights reserved.

using System;
using System.IO;
using UnrealBuildTool;
using Microsoft.Win32;

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
            int releaseId = 0;
            string releaseIdString = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion", "ReleaseId", "") as String;
            if (!String.IsNullOrEmpty(releaseIdString))
            {
                releaseId = Convert.ToInt32(releaseIdString);
            }
            bool bAllowWindowsMixedReality = (releaseId >= 1803);

            if (bAllowWindowsMixedReality)
            {
                string LibrariesPath = Path.Combine(ThirdPartyPath, "Lib", "x64");

                if (Target.Platform == UnrealTargetPlatform.Win32)
                {
                    LibrariesPath = Path.Combine(ThirdPartyPath, "Lib", "x86");
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win32/HolographicStreamerDesktop.dll");
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win32/Microsoft.Perception.Simulation.dll");
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win32/PerceptionSimulationManager.dll");
                }
                else if (Target.Platform == UnrealTargetPlatform.Win64)
                {
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/HolographicStreamerDesktop.dll");
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/Microsoft.Perception.Simulation.dll");
                    RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/PerceptionSimulationManager.dll");
                }

                PublicLibraryPaths.Add(LibrariesPath);
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "MixedRealityInterop.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "HolographicStreamerDesktop.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "Microsoft.Perception.Simulation.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "PerceptionSimulationManager.lib"));

                // Win10 support
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "onecore.lib"));

                PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include"));
            }
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