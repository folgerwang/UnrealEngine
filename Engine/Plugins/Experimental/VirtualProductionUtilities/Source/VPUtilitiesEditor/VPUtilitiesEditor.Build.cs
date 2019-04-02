// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VPUtilitiesEditor : ModuleRules
{
	public VPUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"VPUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorStyle",
				"Engine",
				"LevelEditor",
				"Settings",
				"Slate",
				"SlateCore",
				"TimeManagement",
				"UMG",
				"UMGEditor",
				"UnrealEd",
				"VPBookmark",
				"VREditor",
				"WorkspaceMenuStructure",
			}
		);
	}
}
