// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


namespace UnrealBuildTool.Rules
{
	public class WebMMedia : ModuleRules
	{
		public WebMMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"WebMMediaFactory",
					"Core",
					"Engine",
					"RenderCore",
					"RHI",
					"ShaderCore",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Media",
					"MediaUtils",
					"UtilityShaders",
					"libOpus",
					"UEOgg",
					"Vorbis",
					"LibVpx",
					"LibWebM",
				});
		}
	}
}
