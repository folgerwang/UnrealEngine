
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libPhonon : ModuleRules
{
    public libPhonon(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string LibraryPath = Target.UEThirdPartySourceDirectory + "libPhonon/phonon_api/";
        string BinaryPath = "$(EngineDir)/Binaries/ThirdParty/Phonon/";

        PublicIncludePaths.Add(LibraryPath + "include");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicLibraryPaths.Add(LibraryPath + "/lib/Win64");
            PublicAdditionalLibraries.Add("phonon.lib");

            string DllName = "phonon.dll";

            // 64 bit only libraries for TAN support:
            string TrueAudioNextDllName = "tanrt64.dll";
            string GPUUtilitiesDllName = "GPUUtilities.dll";

            PublicDelayLoadDLLs.Add(DllName);
            PublicDelayLoadDLLs.Add(TrueAudioNextDllName);
            PublicDelayLoadDLLs.Add(GPUUtilitiesDllName);

            BinaryPath += "Win64/";

            RuntimeDependencies.Add(BinaryPath + DllName);
            RuntimeDependencies.Add(BinaryPath + TrueAudioNextDllName);
            RuntimeDependencies.Add(BinaryPath + GPUUtilitiesDllName);
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            PublicLibraryPaths.Add(LibraryPath + "/lib/Win32");
            PublicAdditionalLibraries.Add("phonon.lib");

            string DllName = "phonon.dll";

            PublicDelayLoadDLLs.Add(DllName);

            BinaryPath += "Win32/";

            RuntimeDependencies.Add(BinaryPath + DllName);
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicLibraryPaths.Add(LibraryPath + "/lib/Android");
            PublicAdditionalLibraries.Add("phonon");
        }
    }
}

