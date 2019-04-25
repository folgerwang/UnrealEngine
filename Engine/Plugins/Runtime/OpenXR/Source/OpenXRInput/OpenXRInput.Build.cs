// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OpenXRInput : ModuleRules
    {
        public OpenXRInput(ReadOnlyTargetRules Target) : base(Target)
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
					"ApplicationCore",
                    "Engine",
                    "InputDevice",
                    "InputCore",
                    "HeadMountedDisplay",
                    "OpenXRHMD"
                }
                );

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenXR");
        }
    }
}
