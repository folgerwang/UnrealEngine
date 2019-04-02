// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SwarmInterface : ModuleRules
{
	public SwarmInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/SwarmInterface/Public"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePathModuleNames.Add("MessagingCommon");
			// the modules below are only needed for the UMB usability check
			PublicDependencyModuleNames.Add("Sockets");
			PublicDependencyModuleNames.Add("Networking");
		}

		// Copy the AgentInterface DLL to the same output directory as the editor DLL.
		RuntimeDependencies.Add("$(BinaryOutputDir)/AgentInterface.dll", "$(EngineDir)/Binaries/DotNET/AgentInterface.dll", StagedFileType.NonUFS);

		// Also copy the PDB, if it exists
		if(File.Exists(Path.Combine(EngineDirectory, "Binaries", "DotNET", "AgentInterface.pdb")))
		{
			RuntimeDependencies.Add("$(BinaryOutputDir)/AgentInterface.pdb", "$(EngineDir)/Binaries/DotNET/AgentInterface.pdb", StagedFileType.DebugNonUFS);
		}
	}
}
