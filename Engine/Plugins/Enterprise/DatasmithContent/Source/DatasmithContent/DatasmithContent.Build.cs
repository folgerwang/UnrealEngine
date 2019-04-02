// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithContent : ModuleRules
	{
		public DatasmithContent(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"Engine",
                    "RenderCore",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Landscape",
					"LevelSequence",
					"MeshDescription",
                    "Projects",
                }
			);
		}
	}
}