// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OpenXRHMD : ModuleRules
	{
		public OpenXRHMD(ReadOnlyTargetRules Target) : base(Target)
        {
            var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            PrivateIncludePaths.AddRange(
				new string[] {
					"OpenXRHMD/Private",
                    EngineDir + "/Source/Runtime/Renderer/Private",
                    EngineDir + "/Source/ThirdParty/OpenXR/include",
					// ... add other private include paths required here ...
				}
				);

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
					"RenderCore",
                    "HeadMountedDisplay",
                    "UtilityShaders",
                    "Slate",
                    "SlateCore",
                    "OpenXR"
                }
				);

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenXR");

            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });

                // Required for some private headers needed for the rendering support.
                PrivateIncludePaths.AddRange(
                    new string[] {
                            Path.Combine(EngineDir, @"Source\Runtime\Windows\D3D11RHI\Private"),
                            Path.Combine(EngineDir, @"Source\Runtime\Windows\D3D11RHI\Private\Windows")
                                });

                AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
            }
        }
	}
}
