// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AndroidDeviceProfileSelectorRuntime : ModuleRules
	{
        public AndroidDeviceProfileSelectorRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidDPSRT";

			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
				}
				);

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
					"AndroidDeviceProfileSelector",
				}
				);
		}
	}
}
