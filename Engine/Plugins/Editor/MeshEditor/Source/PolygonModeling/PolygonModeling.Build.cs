// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PolygonModeling : ModuleRules
	{
        public PolygonModeling(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
					"Slate",
					"Engine",
					"InputCore",
					"UnrealEd",
					"EditableMesh",
					"MeshEditor",
                    "MeshDescription",
					"SlateCore",
					"ViewportInteraction"
				}
			);
		}
	}
}