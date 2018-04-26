// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class QuadricMeshReduction : ModuleRules
{
	public QuadricMeshReduction(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("RenderCore");
		PrivateDependencyModuleNames.Add("RawMesh");
        PrivateDependencyModuleNames.Add("MeshDescription");

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );
    }
}
