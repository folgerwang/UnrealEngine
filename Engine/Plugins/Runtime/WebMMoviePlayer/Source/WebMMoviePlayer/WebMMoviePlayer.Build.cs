// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class WebMMoviePlayer : ModuleRules
	{
		public WebMMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Engine",
					"MoviePlayer",
					"RenderCore",
					"RHI",
					"SlateCore",
					"Slate",
					"MediaUtils",
					"WebMMedia",
				});
		}
	}
}
