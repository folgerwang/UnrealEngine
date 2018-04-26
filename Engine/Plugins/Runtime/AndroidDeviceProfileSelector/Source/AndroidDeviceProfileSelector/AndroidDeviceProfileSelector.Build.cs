// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AndroidDeviceProfileSelector : ModuleRules
	{
        public AndroidDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidDPS";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				    "Core",
				    "CoreUObject",
				    "Engine",
				}
				);
		}
	}
}
