// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class MixedRealityInteropLibrary : ModuleRules
{
	public MixedRealityInteropLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty"));

		if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(Path.Combine(ThirdPartyPath, "Lib", "x86"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(Path.Combine(ThirdPartyPath, "Lib", "x64"));
		}

		PublicAdditionalLibraries.Add("MixedRealityInterop.lib");
		// Delay-load the DLL, so we can load it from the right place first
		PublicDelayLoadDLLs.Add("MixedRealityInterop.dll");
        RuntimeDependencies.Add(PluginDirectory + "/Binaries/ThirdParty/MixedRealityInteropLibrary/" + Target.Platform.ToString() + "/MixedRealityInterop.dll");
    }
}
