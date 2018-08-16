// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class EditorScriptingUtilities : ModuleRules
	{
		public EditorScriptingUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AssetRegistry",
					"Core",
					"CoreUObject",
					"Engine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"EditorStyle",
					"MainFrame",
					"Slate",
					"SlateCore",
                    "MeshDescription",
					"UnrealEd",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"PythonScriptPlugin",
				}
			);
		}
	}
}
