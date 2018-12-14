// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaCompositingEditor : ModuleRules
	{
		public MediaCompositingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorStyle",
					"EditorWidgets",
					"Engine",
					"ImgMedia",
					"InputCore",
					"MediaAssets",
					"MediaCompositing",
					"MediaUtils",
					"MovieScene",
					"MovieSceneTools",
					"MovieSceneTracks",
					"RHI",
					"Sequencer",
					"SequenceRecorder",
					"Slate",
					"SlateCore",
					"UnrealEd",
					"TimeManagement"
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MediaCompositingEditor/Private",
					"MediaCompositingEditor/Private/Sequencer",
					"MediaCompositingEditor/Private/Shared",
				});
		}
	}
}
