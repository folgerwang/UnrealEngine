// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveCodingServer : ModuleRules
{
	public LiveCodingServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Distorm");
		PrivateDependencyModuleNames.Add("LiveCoding");

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add("Developer/Windows/LiveCoding/Private");
			PrivateIncludePaths.Add("Developer/Windows/LiveCoding/Private/External");

			string DiaSdkDir = Target.WindowsPlatform.DiaSdkDir;
			if(DiaSdkDir == null)
			{
				throw new System.Exception("Unable to find DIA SDK directory");
			}

			PrivateIncludePaths.Add(Path.Combine(DiaSdkDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(DiaSdkDir, "lib", "amd64", "diaguids.lib"));
			RuntimeDependencies.Add("$(TargetOutputDir)/msdia140.dll", Path.Combine(DiaSdkDir, "bin", "amd64", "msdia140.dll"));
		}

		// Allow precompiling when generating project files so we can get intellisense
		if(!Target.bGenerateProjectFiles)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
