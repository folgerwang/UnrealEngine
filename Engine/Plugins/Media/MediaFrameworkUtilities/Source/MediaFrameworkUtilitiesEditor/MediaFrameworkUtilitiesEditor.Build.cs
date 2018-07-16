// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaFrameworkUtilitiesEditor : ModuleRules
	{
		public MediaFrameworkUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"Core",
					"CoreUObject",
					"EditorStyle",
					"Engine",
					"InputCore",
					"MaterialEditor",
					"MediaAssets",
					"MediaFrameworkUtilities",
					"MediaIOCore",
					"MediaUtils",
					"PlacementMode",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
					"WorkspaceMenuStructure",
				});
		}
	}
}
