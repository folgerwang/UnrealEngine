// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapAnalytics : ModuleRules
	{
		public MagicLeapAnalytics( ReadOnlyTargetRules Target ) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"Json",
					"LuminRuntimeSettings",
					"MLSDK"
				}
			);

            PublicIncludePathModuleNames.Add("Analytics");
        }
	}
}
