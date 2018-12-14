// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FieldSystemSimulationCore : ModuleRules
	{
        public FieldSystemSimulationCore(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ChaosSolvers",
					"Chaos",
					"FieldSystemCore"
                }
            );
        }
    }
}
