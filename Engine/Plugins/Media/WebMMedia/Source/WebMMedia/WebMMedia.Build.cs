// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


namespace UnrealBuildTool.Rules
{
	public class WebMMedia : ModuleRules
	{
		public WebMMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"WebMMediaFactory",
					"Core",
					"Engine",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"ShaderCore",
					"UtilityShaders",
					"libOpus",
					//"UEOgg",
					//"Vorbis",
					"LibVpx",
					"LibWebM",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"WebMMedia/Private",
					"WebMMedia/Private/Player",
				});
		}
	}
}
