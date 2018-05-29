// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class MagicLeapEmulator : ModuleRules
    {
        public MagicLeapEmulator(ReadOnlyTargetRules Target)
        : base(Target)
        {

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "HeadMountedDisplay",
					"LuminRuntimeSettings",
					"MLSDK",
				}
			);

            
            PublicIncludePaths.AddRange(
                new string[]
                {
                }
            );

            if (Target.Type == TargetType.Editor)
            {
                PrivateDependencyModuleNames.Add("Settings");
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
        }
    }
}