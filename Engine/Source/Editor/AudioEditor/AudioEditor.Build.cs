// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioEditor : ModuleRules
{
	public AudioEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/AudioEditor/Private",
				"Editor/AudioEditor/Private/Factories",
				"Editor/AudioEditor/Private/AssetTypeActions"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AudioMixer"
			});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Engine",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"RenderCore",
				"LevelEditor",
				"Landscape",
				"PropertyEditor",
				"DetailCustomizations",
				"ClassViewer",
				"GraphEditor",
				"ContentBrowser",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools"
			});

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("WITH_SNDFILE_IO=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_SNDFILE_IO=0");
		}
	}
}
