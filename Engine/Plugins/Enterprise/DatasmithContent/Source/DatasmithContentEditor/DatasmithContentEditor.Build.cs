// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithContentEditor : ModuleRules
	{
		public DatasmithContentEditor(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithContent",
					"EditorStyle",
					"Engine",
					"Projects",
					"UnrealEd",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"SlateCore",
					"Slate",
				}
			);
		}
	}
}