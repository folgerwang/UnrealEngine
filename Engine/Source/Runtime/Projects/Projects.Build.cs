// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Projects : ModuleRules
	{
		public Projects(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Json",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/Projects/Private",
				}
			);

			PrivateDefinitions.Add(string.Format("UBT_TARGET_ENABLED_PLUGINS=TEXT(\"{0}\")", string.Join("\", TEXT(\"", Target.EnablePlugins)));
			PrivateDefinitions.Add(string.Format("UBT_TARGET_DISABLED_PLUGINS=TEXT(\"{0}\")", string.Join("\", TEXT(\"", Target.DisablePlugins)));

			if (Target.bIncludePluginsForTargetPlatforms)
			{
				PublicDefinitions.Add("LOAD_PLUGINS_FOR_TARGET_PLATFORMS=1");
			}
			else
			{
				PublicDefinitions.Add("LOAD_PLUGINS_FOR_TARGET_PLATFORMS=0");
			}
		}
	}
}
