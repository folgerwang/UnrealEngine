// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOgg : ModuleRules
{
	public UEOgg(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string OggPath = Target.UEThirdPartySourceDirectory + "Ogg/libogg-1.2.2/";

		PublicSystemIncludePaths.Add(OggPath + "include");

		string OggLibPath = OggPath + "lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			OggLibPath += "Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg_64.lib");

			PublicDelayLoadDLLs.Add("libogg_64.dll");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Ogg/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libogg_64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			OggLibPath += "Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg.lib");

			PublicDelayLoadDLLs.Add("libogg.dll");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Ogg/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libogg.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = Target.UEThirdPartyBinariesDirectory + "Ogg/Mac/libogg.dylib";
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			string OpimizationSuffix = "";
			if (Target.bCompileForSize)
			{
				OpimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OpimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OpimizationSuffix = "_O3";
				}
			}
			PublicAdditionalLibraries.Add(OggLibPath + "HTML5/libogg" + OpimizationSuffix + ".bc");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			// Filtered in the toolchain.
			PublicLibraryPaths.Add(OggLibPath + "Android/ARMv7");
			PublicLibraryPaths.Add(OggLibPath + "Android/ARM64");
			PublicLibraryPaths.Add(OggLibPath + "Android/x86");
			PublicLibraryPaths.Add(OggLibPath + "Android/x64");

			PublicAdditionalLibraries.Add("ogg");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(OggLibPath + "Linux/" + Target.Architecture + "/libogg.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(OggLibPath + "Linux/" + Target.Architecture + "/libogg_fPIC.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicLibraryPaths.Add(OggLibPath + "XboxOne/VS" + VersionName.ToString());
				PublicAdditionalLibraries.Add("libogg_static.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalLibraries.Add(OggLibPath + "ios" + "/libogg.a");
        }
		else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            PublicAdditionalLibraries.Add(OggLibPath + "ORBIS_Release" + "/libogg-1.2.2_PS4_Static.a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Switch)
        {
            PublicAdditionalLibraries.Add(OggLibPath + "NX64" + "/Ogg_Switch_Static.a");
        }
    }
}
