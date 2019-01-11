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
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"LevelEditor",
				"Slate",
				"SlateCore",
				"UMG",
				"UMGEditor",
				"UnrealEd",
				"VPBookmark",
				"VREditor",
			}
		);
	}
}
