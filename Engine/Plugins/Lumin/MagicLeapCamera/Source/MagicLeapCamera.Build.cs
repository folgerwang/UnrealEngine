// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapCamera : ModuleRules
	{
		public MagicLeapCamera( ReadOnlyTargetRules Target ) : base(Target)
		{
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));            

            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LuminRuntimeSettings",
					"MLSDK",
					"MagicLeap",
					"HeadMountedDisplay",
				}
			);

			// This is not ideal but needs to be done in order to expose the private MagicLeapHMD headers to this module.
			PrivateIncludePaths.Add(Path.Combine(
				new string[] 
				{
					ModuleDirectory,
					"..", "..",
					"MagicLeap", "Source", "MagicLeap", "Private"
                })
			);
        }
	}
}
