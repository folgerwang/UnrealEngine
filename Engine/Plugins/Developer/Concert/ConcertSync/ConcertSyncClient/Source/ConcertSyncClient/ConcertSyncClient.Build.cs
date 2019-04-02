// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSyncClient : ModuleRules
	{
		public ConcertSyncClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Concert",
                }
            );
       
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ConcertTransport",
					"ConcertSyncCore",
					"ConcertUICore",
					"AssetRegistry",
					"HeadMountedDisplay",
					"InputCore",
					"MovieScene",
					"LevelSequence",
					"RenderCore",
					"TargetPlatform",
					"TimeManagement",
					"SlateCore",
					"Slate",
					"SourceControl",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"DirectoryWatcher",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"DirectoryWatcher",
					}
				);

				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"EditorStyle",
						"EngineSettings",
						"Sequencer",
						"UnrealEd",
						"ViewportInteraction",
						"LevelEditor",
						"VREditor",
					}
				);
			}
		}
	}
}
