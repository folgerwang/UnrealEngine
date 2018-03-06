// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AssetScriptingUtilitiesEditor : ModuleRules
	{
		public AssetScriptingUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"AssetScriptingUtilitiesEditor/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "RawMesh",
                    "UnrealEd"
				}
			);
		}
	}
}
