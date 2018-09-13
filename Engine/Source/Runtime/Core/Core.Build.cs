// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Core : ModuleRules
{
	public Core(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "Private/CorePrivatePCH.h";

		SharedPCHHeaderFile = "Public/CoreSharedPCH.h";

		PrivateDependencyModuleNames.Add("BuildSettings");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DerivedDataCache/Public",
				"Runtime/SynthBenchmark/Public",
				"Runtime/Core/Private",
				"Runtime/Core/Private/Misc",
                "Runtime/Core/Private/Internationalization",
				"Runtime/Core/Private/Internationalization/Cultures",
				"Runtime/Engine/Public",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"DerivedDataCache",
                "InputDevice",
                "Analytics",
				"RHI"
			}
			);

		if (Target.bBuildEditor == true)
		{
			DynamicallyLoadedModuleNames.Add("SourceCodeAccess");

			PrivateIncludePathModuleNames.Add("DirectoryWatcher");
			DynamicallyLoadedModuleNames.Add("DirectoryWatcher");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib");

			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"IntelTBB",
				"IntelVTune"
				);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"zlib",
				"PLCrashReporter",
				"rd_route"
				);
			PublicFrameworks.AddRange(new string[] { "Cocoa", "Carbon", "IOKit", "Security" });
			
			if (Target.bBuildEditor == true)
			{
				PublicAdditionalLibraries.Add("/System/Library/PrivateFrameworks/MultitouchSupport.framework/Versions/Current/MultitouchSupport");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib"
				);
			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation", "AudioToolbox", "AVFoundation", "GameKit", "StoreKit", "CoreVideo", "CoreMedia", "CoreGraphics", "GameController", "SystemConfiguration", "DeviceCheck" });
			if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PublicFrameworks.AddRange(new string[] { "CoreMotion", "AdSupport", "WebKit" });
                AddEngineThirdPartyPrivateStaticDependencies(Target,
                    "PLCrashReporter"
                    );
			}

			PrivateIncludePathModuleNames.Add("ApplicationCore");

			bool bSupportAdvertising = Target.Platform == UnrealTargetPlatform.IOS;
			if (bSupportAdvertising)
			{
				PublicFrameworks.AddRange(new string[] { "iAD" });
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"cxademangle",
				"zlib"
				);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
			PublicIncludePaths.Add(string.Format("Runtime/Core/Public/{0}", Target.Platform.ToString()));
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib",
				"jemalloc"
                );

			// Core uses dlopen()
			PublicAdditionalLibraries.Add("dl");
        }
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
            PrivateDependencyModuleNames.Add("HTML5JS");
            PrivateDependencyModuleNames.Add("MapPakDownloader");
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            PublicAdditionalLibraries.Add("SceRtc_stub_weak"); //ORBIS SDK rtc.h, used in PS4Time.cpp
        }

		if ( Target.bCompileICU == true )
        {
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ICU");
        }
        PublicDefinitions.Add("UE_ENABLE_ICU=" + (Target.bCompileICU ? "1" : "0")); // Enable/disable (=1/=0) ICU usage in the codebase. NOTE: This flag is for use while integrating ICU and will be removed afterward.

        // If we're compiling with the engine, then add Core's engine dependencies
		if (Target.bCompileAgainstEngine == true)
		{
			if (!Target.bBuildRequiresCookedData)
			{
				DynamicallyLoadedModuleNames.AddRange(new string[] { "DerivedDataCache" });
			}
		}

		
		// On Windows platform, VSPerfExternalProfiler.cpp needs access to "VSPerf.h".  This header is included with Visual Studio, but it's not in a standard include path.
		if( Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 )
		{
			var VisualStudioVersionNumber = "11.0";
			var SubFolderName = ( Target.Platform == UnrealTargetPlatform.Win64 ) ? "x64/PerfSDK" : "PerfSDK";

			string PerfIncludeDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), String.Format("Microsoft Visual Studio {0}/Team Tools/Performance Tools/{1}", VisualStudioVersionNumber, SubFolderName));

			if (File.Exists(Path.Combine(PerfIncludeDirectory, "VSPerf.h")))
			{
				PrivateIncludePaths.Add(PerfIncludeDirectory);
				PublicDefinitions.Add("WITH_VS_PERF_PROFILER=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_VS_PERF_PROFILER=0");
			}
		}

		WhitelistRestrictedFolders.Add("Private/NoRedist");

        if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            PublicDefinitions.Add("WITH_DIRECTXMATH=1");
        }
        else if ((Target.Platform == UnrealTargetPlatform.Win64) ||
                (Target.Platform == UnrealTargetPlatform.Win32))
        {
			// To enable this requires Win8 SDK
            PublicDefinitions.Add("WITH_DIRECTXMATH=0");  // Enable to test on Win64/32.

            //PublicDependencyModuleNames.AddRange(  // Enable to test on Win64/32.
			//    new string[] {
			//    "DirectXMath"
            //});
        }
        else
        {
            PublicDefinitions.Add("WITH_DIRECTXMATH=0");
        }

        // Decide if validating memory allocator (aka MallocStomp) can be used on the current platform.
        // Run-time validation must be enabled through '-stompmalloc' command line argument.

        bool bWithMallocStomp = false;
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            switch (Target.Platform)
            {
                case UnrealTargetPlatform.Mac:
                case UnrealTargetPlatform.Linux:
                //case UnrealTargetPlatform.Win32: // 32-bit windows can technically be supported, but will likely run out of virtual memory space quickly
                //case UnrealTargetPlatform.XboxOne: // XboxOne could be supported, as it's similar enough to Win64
                case UnrealTargetPlatform.Win64:
                    bWithMallocStomp = true;
                break;
            }
        }

        PublicDefinitions.Add("WITH_MALLOC_STOMP=" + (bWithMallocStomp ? "1" : "0"));

    }
}
