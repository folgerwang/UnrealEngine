// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioPlatformConfiguration : ModuleRules
	{
		public AudioPlatformConfiguration(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
                    "CoreUObject"
                }
			);

            PrivateIncludePathModuleNames.Add("Engine");

            if (Target.Type == TargetType.Editor && Target.Platform != UnrealTargetPlatform.Linux)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target,
                    "UELibSampleRate"
                    );
            }
        }
	}
}
