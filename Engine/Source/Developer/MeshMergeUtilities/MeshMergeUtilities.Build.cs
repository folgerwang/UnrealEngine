// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshMergeUtilities : ModuleRules
{
	public MeshMergeUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "Renderer",
                "RHI",
                "Landscape",
                "UnrealEd",
                "MaterialUtilities",     
                "SlateCore",
                "Slate",
                "StaticMeshEditor",
                "SkeletalMeshEditor",
                "MaterialBaking",
                "MeshUtilitiesCommon",
            }
		);
        
        PublicDependencyModuleNames.AddRange(
			new string [] {
                "RawMesh",
                "MeshDescription",
                "MeshDescriptionOperations"
            }
		);

        PublicIncludePathModuleNames.AddRange(
          new string[] {
               "HierarchicalLODUtilities",
               "MeshUtilities",
               "MeshReductionInterface",
          }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "HierarchicalLODUtilities",
                "MeshUtilities",
                "MeshReductionInterface",
                "MaterialBaking",
            }
       );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "HierarchicalLODUtilities",
                "MeshUtilities",
                "MeshReductionInterface",
            }
        );
    }
}
