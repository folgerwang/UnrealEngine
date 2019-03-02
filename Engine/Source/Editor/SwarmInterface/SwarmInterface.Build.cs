// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

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

		// Copy the AgentInterface DLLs to the same output directory as the editor DLL.
		RuntimeDependencies.Add("$(BinaryOutputDir)/AgentInterface.dll", "$(EngineDir)/Binaries/DotNET/AgentInterface.dll", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(BinaryOutputDir)/AgentInterface.pdb", "$(EngineDir)/Binaries/DotNET/AgentInterface.pdb", StagedFileType.DebugNonUFS);
	}
}
