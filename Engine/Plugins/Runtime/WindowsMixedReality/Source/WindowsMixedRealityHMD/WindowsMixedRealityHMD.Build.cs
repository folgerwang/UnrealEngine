// Copyright (c) Microsoft Corporation. All rights reserved.

using System;
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class WindowsMixedRealityHMD : ModuleRules
    {
        private string ModulePath
		{
			get { return ModuleDirectory; }
		}
	 
		private string ThirdPartyPath
		{
			get { return Path.GetFullPath( Path.Combine( ModulePath, "../ThirdParty" ) ); }
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
			// Explicitly load lib path since name conflicts with an existing lib in the DX11 dependency.
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "d3d11.lib"));
				
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include"));
        }

        public WindowsMixedRealityHMD(ReadOnlyTargetRules Target) : base(Target)
        {
            bEnableExceptions = true;

            if (Target.Platform == UnrealTargetPlatform.Win32 ||
                Target.Platform == UnrealTargetPlatform.Win64)
            {
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"Core",
						"CoreUObject",
						"Engine",
						"InputCore",
						"RHI",
						"RenderCore",
						"Renderer",
						"ShaderCore",
						"HeadMountedDisplay",
						"D3D11RHI",
						"Slate",
						"SlateCore",
						"UtilityShaders",
						"Projects",
                    }
					);

				if (Target.bBuildEditor == true)
				{
					PrivateDependencyModuleNames.Add("UnrealEd");
				}

                LoadMixedReality(Target);

                PrivateIncludePaths.AddRange(
                    new string[]
                    {
                    "WindowsMixedRealityHMD/Private",
                    "../../../../Source/Runtime/Windows/D3D11RHI/Private",
                    "../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
                    "../../../../Source/Runtime/Renderer/Private",
                    });

                bFasterWithoutUnity = true;

                PCHUsage = PCHUsageMode.NoSharedPCHs;
                PrivatePCHHeaderFile = "Private/WindowsMixedRealityPrecompiled.h";
            }
        }
    }
}
