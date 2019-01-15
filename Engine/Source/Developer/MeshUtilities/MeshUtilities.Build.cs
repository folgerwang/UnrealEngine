// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshUtilities : ModuleRules
{
	public MeshUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"MaterialUtilities",

			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RawMesh",
				"RenderCore", // For FPackedNormal
				"SlateCore",
				"Slate",
				"MaterialUtilities",
				"MeshBoneReduction",
				"UnrealEd",
				"RHI",
				"HierarchicalLODUtilities",
				"Landscape",
				"LevelEditor",
				"AnimationBlueprintEditor",
				"AnimationEditor",
				"SkeletalMeshEditor",
				"SkeletonEditor",
				"PropertyEditor",
				"EditorStyle",
                "GraphColor",
                "MeshBuilder",
                "MeshUtilitiesCommon",
                "MeshDescription",
                "MeshDescriptionOperations"
            }
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "MeshMergeUtilities"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
          new string[] {
                "MeshMergeUtilities",
                "MaterialBaking",
          }
      );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "MeshMergeUtilities",
                "MaterialBaking",
            }
        );

        AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTriStrip");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "ForsythTriOptimizer");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "QuadricMeshReduction");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTessLib");

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");
		}

        // EMBREE
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree2140/Win64/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicLibraryPaths.Add(SDKDir + "lib");
            PublicAdditionalLibraries.Add("embree.2.14.0.lib");
            RuntimeDependencies.Add("$(TargetOutputDir)/embree.2.14.0.dll", SDKDir + "lib/embree.2.14.0.dll");
            RuntimeDependencies.Add("$(TargetOutputDir)/tbb.dll", SDKDir + "lib/tbb.dll");
			RuntimeDependencies.Add("$(TargetOutputDir)/tbbmalloc.dll", SDKDir + "lib/tbbmalloc.dll");
			PublicDefinitions.Add("USE_EMBREE=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree2140/MacOSX/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicAdditionalLibraries.Add(SDKDir + "lib/libembree.2.14.0.dylib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/libtbb.dylib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/libtbbmalloc.dylib");
			RuntimeDependencies.Add("$(TargetOutputDir)/libembree.2.14.0.dylib", SDKDir + "lib/libembree.2.14.0.dylib");
			RuntimeDependencies.Add("$(TargetOutputDir)/libtbb.dylib", SDKDir + "lib/libtbb.dylib");
			RuntimeDependencies.Add("$(TargetOutputDir)/libtbbmalloc.dylib", SDKDir + "lib/libtbbmalloc.dylib");
            PublicDefinitions.Add("USE_EMBREE=1");
        }
        else
        {
            PublicDefinitions.Add("USE_EMBREE=0");
        }
	}
}
