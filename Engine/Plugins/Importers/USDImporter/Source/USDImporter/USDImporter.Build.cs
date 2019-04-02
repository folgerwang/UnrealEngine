// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDImporter : ModuleRules
	{
		public USDImporter(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
				}
				);

		
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"InputCore",
					"SlateCore",
                    "PropertyEditor",
					"Slate",
                    "EditorStyle",
                    "RawMesh",
                    "GeometryCache",
					"MeshDescription",
					"MeshUtilities",
                    "PythonScriptPlugin",
                    "RenderCore",
                    "RHI",
                    "MessageLog",
					"JsonUtilities",
                }
				);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MeshDescription"
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateDependencyModuleNames.Add("UnrealUSDWrapper");

				foreach (string FilePath in Directory.EnumerateFiles(Path.Combine(ModuleDirectory, "../../Binaries/Win64/"), "*.dll", SearchOption.AllDirectories))
                {
                    RuntimeDependencies.Add(FilePath);
                }
            }
            else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
			{
				PrivateDependencyModuleNames.Add("UnrealUSDWrapper");

				// link directly to runtime libs on Linux, as this also puts them into rpath
				string RuntimeLibraryPath = Path.Combine(ModuleDirectory, "../../Binaries", Target.Platform.ToString(), Target.Architecture.ToString());
				PrivateRuntimeLibraryPaths.Add(RuntimeLibraryPath);

				foreach (string FilePath in Directory.EnumerateFiles(RuntimeLibraryPath, "*.so*", SearchOption.AllDirectories))
                {
                    RuntimeDependencies.Add(FilePath);
                }
            }
		}
	}
}
