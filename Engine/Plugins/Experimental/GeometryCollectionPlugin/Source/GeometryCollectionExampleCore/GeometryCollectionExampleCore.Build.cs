// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCollectionExampleCore : ModuleRules
	{
        public GeometryCollectionExampleCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("GeometryCollectionExampleCore/Private");
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "GeometryCollectionCore",
                    "GeometryCollectionSimulationCore",
					"ChaosSolvers",
                    "Chaos",
                    "FieldSystemCore",
                    "FieldSystemSimulationCore"
                }
                );
		}
	}
}
