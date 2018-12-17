// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCollectionEngine : ModuleRules
	{
        public GeometryCollectionEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("Runtime/Experimental/GeometryCollectionEngine/Private");
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
                    "Chaos",
					"ChaosSolvers",
                    "PhysX",
                    "APEX",
                    "FieldSystemCore",
                    "FieldSystemEngine",
                    "GeometryCollectionCore", 
                    "GeometryCollectionSimulationCore",
	                "ChaosSolverEngine"
                }
                );

			if(Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
        }
	}
}
