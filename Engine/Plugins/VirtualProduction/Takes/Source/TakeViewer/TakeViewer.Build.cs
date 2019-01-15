// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeViewer : ModuleRules
{
	public TakeViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"TakeViewer/Private"
			}
		);
	}
}
