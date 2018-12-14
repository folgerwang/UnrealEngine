// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

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

			List<string> EnabledPluginStrings = new List<string>();
			foreach(string EnablePlugin in Target.EnablePlugins)
			{
				EnabledPluginStrings.Add(String.Format("TEXT(\"{0}\")", EnablePlugin));
			}
			PrivateDefinitions.Add(String.Format("UBT_TARGET_ENABLED_PLUGINS={0}", String.Join(", ", EnabledPluginStrings)));

			List<string> DisabledPluginStrings = new List<string>();
			foreach(string DisablePlugin in Target.DisablePlugins)
			{
				DisabledPluginStrings.Add(String.Format("TEXT(\"{0}\")", DisablePlugin));
			}
			PrivateDefinitions.Add(String.Format("UBT_TARGET_DISABLED_PLUGINS={0}", String.Join(", ", DisabledPluginStrings)));

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
