// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaIOEditor : ModuleRules
	{
		public MediaIOEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"MediaIOCore",
					"Slate",
					"SlateCore",
					"TimeManagement",
					"UnrealEd",
				});
		}
	}
}
