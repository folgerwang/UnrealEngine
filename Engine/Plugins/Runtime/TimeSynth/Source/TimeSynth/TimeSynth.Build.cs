// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TimeSynth : ModuleRules
	{
        public TimeSynth(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
                    "UMG",
                    "Slate",
                    "SlateCore",
                    "InputCore",
                    "Projects"
                }
            );
		}
	}
}