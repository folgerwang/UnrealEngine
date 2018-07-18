// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapEyeTracker : ModuleRules
	{
		public MagicLeapEyeTracker(ReadOnlyTargetRules Target)
				: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"EyeTracker"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
					"MagicLeap",
					"MLSDK"
				}
			);

			// This is not ideal but needs to be done in order to expose the private MagicLeapHMD header to this module.
			PrivateIncludePaths.Add(Path.Combine(new string[] { ModuleDirectory, "..", "MagicLeap", "Private" }));
			//PublicIncludePaths.AddRange(new string[] { "Private", "Public"});
		}
	}
}
