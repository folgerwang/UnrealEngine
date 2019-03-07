// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class VivoxCoreSDK : ModuleRules
	{
		public VivoxCoreSDK(ReadOnlyTargetRules Target) : base(Target)
		{
			Type = ModuleType.External;

			string VivoxSDKPath = ModuleDirectory;
			string PlatformSubdir = Target.Platform.ToString();
			string VivoxLibPath = Path.Combine(VivoxSDKPath, "Lib", PlatformSubdir);
			string VivoxIncludePath = Path.Combine(VivoxSDKPath, "Include");
			string VivoxBinPath = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Vivox", PlatformSubdir);
			PublicIncludePaths.Add(VivoxIncludePath);

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk_x64.lib");
				PublicDelayLoadDLLs.Add("ortp_x64.dll");
				PublicDelayLoadDLLs.Add("vivoxsdk_x64.dll");
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "ortp_x64.dll"));
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "vivoxsdk_x64.dll"));
			}
			else if(Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk.lib");
				PublicDelayLoadDLLs.Add("ortp.dll");
				PublicDelayLoadDLLs.Add("vivoxsdk.dll");
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "ortp.dll"));
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "vivoxsdk.dll"));
			}
			else if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk.lib");
			}
			else if (Target.Platform == UnrealTargetPlatform.PS4)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk");
				PublicAdditionalLibraries.Add("SceSha1");
				PublicAdditionalLibraries.Add("SceAudioIn_stub_weak");
				PublicAdditionalLibraries.Add("SceHmac");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicDelayLoadDLLs.Add(Path.Combine(VivoxBinPath, "libortp.dylib"));
				PublicDelayLoadDLLs.Add(Path.Combine(VivoxBinPath, "libvivoxsdk.dylib"));
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "libortp.dylib"));
				RuntimeDependencies.Add(Path.Combine(VivoxBinPath, "libvivoxsdk.dylib"));
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk");
				PublicFrameworks.Add("CFNetwork");
			}
			else if (Target.Platform == UnrealTargetPlatform.Switch)
			{
				PublicLibraryPaths.Add(VivoxLibPath);
				PublicAdditionalLibraries.Add("vivoxsdk");
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{ 
				PublicLibraryPaths.Add(Path.Combine(VivoxLibPath, "armeabi-v7a"));
				PublicLibraryPaths.Add(Path.Combine(VivoxLibPath, "arm64-v8a"));

				PublicAdditionalLibraries.Add("vivox-sdk");

				string PluginPath = Utils.MakePathRelativeTo(VivoxSDKPath, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "VivoxCoreSDK_UPL.xml"));
			}
		}
	}
}
