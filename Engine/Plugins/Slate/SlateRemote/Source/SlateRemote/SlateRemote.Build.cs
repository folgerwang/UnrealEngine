// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SlateRemote : ModuleRules
	{
		public SlateRemote(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Networking",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"InputCore",
					"Slate",
					"SlateCore",
					"Sockets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"Settings",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"SlateRemote/Private",
					"SlateRemote/Private/Server",
					"SlateRemote/Private/Shared",
				});

			// Only valid in non-shipping builds
			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDefinitions.Add("WITH_SLATE_REMOTE_SERVER=1");
			}
			else
			{
				PrivateDefinitions.Add("WITH_SLATE_REMOTE_SERVER=0");
			}
		}
	}
}
