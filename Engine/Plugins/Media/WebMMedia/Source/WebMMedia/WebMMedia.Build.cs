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
					"Media",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"ShaderCore",
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
