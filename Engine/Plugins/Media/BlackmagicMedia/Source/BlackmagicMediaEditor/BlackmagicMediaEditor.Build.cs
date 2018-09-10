// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlackmagicMediaEditor : ModuleRules
	{
		public BlackmagicMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"BlackmagicMedia",
					"BlackmagicMediaOutput",
					"Core",
					"CoreUObject",
					"MediaAssets",
					"Projects",
					"PropertyEditor",
					"Settings",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
				});

			PrivateIncludePaths.Add("BlackmagicMediaEditor/Private");
		}
	}
}
