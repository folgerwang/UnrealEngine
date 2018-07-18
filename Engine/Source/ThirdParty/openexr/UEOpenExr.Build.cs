// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOpenExr : ModuleRules
{
    public UEOpenExr(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac || Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
            string LibDir = Target.UEThirdPartySourceDirectory + "openexr/Deploy/lib/";
			string Platform = "";
			if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                    Platform = "x64";
                    LibDir += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
                    Platform = "Win32";
                    LibDir += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
                    Platform = "Mac";
                    bDebug = false;
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
                    Platform = "Linux";
                    bDebug = false;
            }
            LibDir = LibDir + "/" + Platform;
            LibDir = LibDir + "/Static" + (bDebug ? "Debug" : "Release");
            PublicLibraryPaths.Add(LibDir);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						"Half.lib",
						"Iex.lib",
						"IlmImf.lib",
						"IlmThread.lib",
						"Imath.lib",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/libHalf.a",
						LibDir + "/libIex.a",
						LibDir + "/libIlmImf.a",
						LibDir + "/libIlmThread.a",
						LibDir + "/libImath.a",
					}
				);
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64"))
			{
				string LibArchDir = LibDir + "/" + Target.Architecture;
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibArchDir + "/libHalf.a",
						LibArchDir + "/libIex.a",
						LibArchDir + "/libIlmImf.a",
						LibArchDir + "/libIlmThread.a",
						LibArchDir + "/libImath.a",
					}
				);
			}

            PublicSystemIncludePaths.AddRange(
                new string[] {
                    Target.UEThirdPartySourceDirectory + "openexr/Deploy/include",
			    }
            );
        }
    }
}

