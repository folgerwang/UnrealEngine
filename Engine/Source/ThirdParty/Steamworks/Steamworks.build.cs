// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class Steamworks : ModuleRules
{
	public Steamworks(ReadOnlyTargetRules Target) : base(Target)
	{
		/** Mark the current version of the Steam SDK */
		string SteamVersion = "v139";
		Type = ModuleType.External;

		PublicDefinitions.Add("STEAM_SDK_VER=TEXT(\"1.39\")");
		PublicDefinitions.Add("STEAM_SDK_VER_PATH=TEXT(\"Steam" + SteamVersion + "\")");

		string SdkBase = Target.UEThirdPartySourceDirectory + "Steamworks/Steam" + SteamVersion + "/sdk";
		if (!Directory.Exists(SdkBase))
		{
			string Err = string.Format("steamworks SDK not found in {0}", SdkBase);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}

		PublicIncludePaths.Add(SdkBase + "/public");

		string LibraryPath = SdkBase + "/redistributable_bin/";
		string LibraryName = "steam_api";

		if(Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
			PublicDelayLoadDLLs.Add(LibraryName + ".dll");

			string SteamBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/Steamworks/Steam{0}/Win32/", SteamVersion);
			RuntimeDependencies.Add(SteamBinariesDir + "steam_api.dll");

			if(Target.Type == TargetType.Server)
			{
				RuntimeDependencies.Add(SteamBinariesDir + "steamclient.dll");
				RuntimeDependencies.Add(SteamBinariesDir + "tier0_s.dll");
				RuntimeDependencies.Add(SteamBinariesDir + "vstdlib_s.dll");
			}
		}
		else if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(LibraryPath + "win64");
			PublicAdditionalLibraries.Add(LibraryName + "64.lib");
			PublicDelayLoadDLLs.Add(LibraryName + "64.dll");

			string SteamBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/Steamworks/Steam{0}/Win64/", SteamVersion);
			RuntimeDependencies.Add(SteamBinariesDir + LibraryName + "64.dll");

			if(Target.Type == TargetType.Server)
			{
				RuntimeDependencies.Add(SteamBinariesDir + "steamclient64.dll");
				RuntimeDependencies.Add(SteamBinariesDir + "tier0_s64.dll");
				RuntimeDependencies.Add(SteamBinariesDir + "vstdlib_s64.dll");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string SteamBinariesPath = String.Format(Target.UEThirdPartyBinariesDirectory + "Steamworks/Steam{0}/Mac/", SteamVersion);
			LibraryPath = SteamBinariesPath + "libsteam_api.dylib";
			PublicDelayLoadDLLs.Add(LibraryPath);
			RuntimeDependencies.Add(LibraryPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				LibraryPath += "linux64";
				PublicLibraryPaths.Add(LibraryPath);
				PublicAdditionalLibraries.Add(LibraryName);
			}
			else
			{
				LibraryPath += "linux64/libsteam_api.so";
				PublicDelayLoadDLLs.Add(LibraryPath);
			}
			string SteamBinariesPath = String.Format(Target.UEThirdPartyBinariesDirectory + "Steamworks/Steam{0}/{1}", SteamVersion, Target.Architecture);
			PrivateRuntimeLibraryPaths.Add(SteamBinariesPath);
			PublicAdditionalLibraries.Add(SteamBinariesPath + "/libsteam_api.so");
			RuntimeDependencies.Add(SteamBinariesPath + "/libsteam_api.so");
		}
	}
}
