// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libcurl : ModuleRules
{
	public libcurl(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_LIBCURL=1");

		string LinuxLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7_48_0/";
		string WinLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/curl-7.55.1/";
		string AndroidLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = LinuxLibCurlPath + "include" + platform;
			string LibraryPath = LinuxLibCurlPath + "lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryPath + "/libcurl.a");

			PrivateDependencyModuleNames.Add("SSL");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			// toolchain will filter properly
			PublicIncludePaths.Add(AndroidLibCurlPath + "include/Android/ARMv7");
			PublicLibraryPaths.Add(AndroidLibCurlPath + "lib/Android/ARMv7");
			PublicIncludePaths.Add(AndroidLibCurlPath + "include/Android/ARM64");
			PublicLibraryPaths.Add(AndroidLibCurlPath + "lib/Android/ARM64");
			PublicIncludePaths.Add(AndroidLibCurlPath + "include/Android/x86");
			PublicLibraryPaths.Add(AndroidLibCurlPath + "lib/Android/x86");
			PublicIncludePaths.Add(AndroidLibCurlPath + "include/Android/x64");
			PublicLibraryPaths.Add(AndroidLibCurlPath + "lib/Android/x64");

			PublicAdditionalLibraries.Add("curl");
//			PublicAdditionalLibraries.Add("crypto");
//			PublicAdditionalLibraries.Add("ssl");
//			PublicAdditionalLibraries.Add("dl");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(WinLibCurlPath + "include/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicLibraryPaths.Add(WinLibCurlPath + "lib/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			PublicAdditionalLibraries.Add("libcurl_a.lib");
			PublicDefinitions.Add("CURL_STATICLIB=1");

			// Our build requires OpenSSL and zlib, so ensure thye're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"OpenSSL",
				"zlib"
			});
		}
	}
}
