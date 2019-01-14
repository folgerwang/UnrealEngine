// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    //MeshBuilder module is a editor module
	public class MeshBuilder : ModuleRules
	{
		public MeshBuilder(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "MeshDescription",
                    "RenderCore",
                    "MeshDescriptionOperations",
                    "MeshReductionInterface",
                    "RawMesh",
                    "MeshUtilitiesCommon",
                }
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTriStrip");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "ForsythTriOptimizer");
	        AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTessLib");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "QuadricMeshReduction");
        }
	}
}
