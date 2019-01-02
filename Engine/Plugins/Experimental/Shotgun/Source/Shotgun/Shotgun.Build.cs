// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Shotgun : ModuleRules
	{
		public Shotgun(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"EditorStyle",
					"Engine",
					"LevelEditor",
					"PythonScriptPlugin",
					"Settings",
					"Slate",
					"SlateCore",
				}
			);
		}
	}
}