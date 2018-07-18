// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimecodeSynchronizerEditor : ModuleRules
{
	public TimecodeSynchronizerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("TimecodeSynchronizerEditor/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MediaAssets",
				"TimecodeSynchronizer",
			});
			
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"EditorStyle",
				"InputCore",
				"SlateCore",
				"Slate",
				"UnrealEd",
			});
	}
}
