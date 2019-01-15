// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertFrontend : ModuleRules
	{
		public ConcertFrontend(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Slate",
					"EditorStyle",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"PropertyEditor",
					"SlateCore",
					"InputCore",
					"Projects",
                    "DesktopPlatform",
					"SourceControl",
					"MessageLog",
                    "Concert",
					"ConcertTransport",
                    "ConcertSyncClient",
                    "ConcertUICore",
                    "ConcertSyncCore",
					"UndoHistory",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
				        "WorkspaceMenuStructure",
					}
				);
			}
		}
	}
}
