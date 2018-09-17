// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class BlackmagicMedia : ModuleRules
	{
		public BlackmagicMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Blackmagic",
                    "Core",
					"CoreUObject",
					"Engine",
                    "MediaIOCore",
                    "MediaUtils",
					"Projects",
                    "TimeManagement",
                });

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"BlackmagicMedia/Private",
					"BlackmagicMedia/Private/Blackmagic",
					"BlackmagicMedia/Private/Assets",
					"BlackmagicMedia/Private/Player",
					"BlackmagicMedia/Private/Shared",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});
		}
	}
}
