// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SequenceRecorderSections : ModuleRules
	{
		public SequenceRecorderSections(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					"Editor/SequenceRecorderSections/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/SequenceRecorderSections/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "MovieScene",
                    "MovieSceneTracks",
					"SequenceRecorder",
					"TimeManagement"
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);
		}
	}
}
