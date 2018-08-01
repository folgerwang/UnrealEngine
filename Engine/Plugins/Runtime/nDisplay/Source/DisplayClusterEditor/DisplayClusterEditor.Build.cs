// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterEditor : ModuleRules
{
	public DisplayClusterEditor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange( new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd"
		});

		PrivateDependencyModuleNames.AddRange( new string[] {
			"DisplayCluster"
		});

        PrivateIncludePathModuleNames.AddRange( new string[] {
			"Settings",
			"DisplayCluster"
		});
	}
}
