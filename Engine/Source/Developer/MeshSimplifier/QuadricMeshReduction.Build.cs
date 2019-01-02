// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
        PrivateDependencyModuleNames.Add("MeshDescription");
        PrivateDependencyModuleNames.Add("MeshDescriptionOperations");
        PrivateDependencyModuleNames.Add("MeshUtilitiesCommon");

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );
    }
}
