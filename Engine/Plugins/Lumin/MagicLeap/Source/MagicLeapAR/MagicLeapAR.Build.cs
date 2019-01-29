// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapAR : ModuleRules
	{
		public MagicLeapAR(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateIncludePaths.AddRange(
				new string[] {
					"../../../../Source/Runtime/Renderer/Private",
					// ... add other private include paths required here ...
				}
			);
			// This is not ideal but needs to be done in order to expose the private MagicLeapHMD header to this module.
			//PrivateIncludePaths.Add(Path.Combine(new string[] { ModuleDirectory, "../../../../Lumin/MagicLeap", "Private" }));

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"LuminRuntimeSettings",
					"MagicLeap",
					"MLSDK",
					"HeadMountedDisplay",
					"AugmentedReality"
				}
			);

			// This is not ideal but needs to be done in order to expose the private MagicLeapHMD header to this module.
			PrivateIncludePaths.Add(Path.Combine(new string[] { ModuleDirectory, "..", "MagicLeap", "Private" }));
		}
	}
}
