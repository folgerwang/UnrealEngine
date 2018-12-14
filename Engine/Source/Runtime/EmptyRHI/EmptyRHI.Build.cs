// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EmptyRHI : ModuleRules
{	
	public EmptyRHI(ReadOnlyTargetRules Target) : base(Target)
	{
//		BinariesSubFolder = "Empty";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore"
			}
			);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
