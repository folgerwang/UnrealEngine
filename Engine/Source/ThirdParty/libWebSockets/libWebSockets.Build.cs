// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class libWebSockets : ModuleRules
{
	public libWebSockets(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string WebsocketPath = Path.Combine(Target.UEThirdPartySourceDirectory, "libWebSockets", "libwebsockets");
		string PlatformSubdir = Target.Platform.ToString();

        bool bUseDebugBuild = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
        string ConfigurationSubdir = bUseDebugBuild ? "Debug" : "Release";
        switch (Target.Platform)
		{
		case UnrealTargetPlatform.HTML5:
			return;

		case UnrealTargetPlatform.Win64:
		case UnrealTargetPlatform.Win32:
			PlatformSubdir = Path.Combine(PlatformSubdir, "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            PublicAdditionalLibraries.Add("websockets_static.lib");
            break;

		case UnrealTargetPlatform.Mac:
        case UnrealTargetPlatform.IOS:
            PublicAdditionalShadowFiles.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), ConfigurationSubdir, "libwebsockets.a"));
            PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), ConfigurationSubdir, "libwebsockets.a"));
            break;

		case UnrealTargetPlatform.PS4:
			PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), ConfigurationSubdir, "libwebsockets.a"));
			break;

		case UnrealTargetPlatform.Switch:
			PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), ConfigurationSubdir, "libwebsockets.a"));
			break;

		case UnrealTargetPlatform.Android:
		    PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir, "ARMv7"));
			PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), "ARMv7", ConfigurationSubdir));
		    PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir, "ARM64"));
			PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), "ARM64", ConfigurationSubdir));
		    PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir, "x86"));
			PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), "x86", ConfigurationSubdir));
		    PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir, "x64"));
			PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), "x64", ConfigurationSubdir));
			PublicAdditionalLibraries.Add("websockets");
			break;
        default:
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				PlatformSubdir = "Linux/" + Target.Architecture;
				PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir, ConfigurationSubdir, "libwebsockets.a"));
				break;
			}
			return;
		}

		if(Target.Platform != UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir, ConfigurationSubdir));
		}
		PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir));

		if (Target.Platform != UnrealTargetPlatform.Switch)
		{
			PublicDependencyModuleNames.Add("OpenSSL");
		}
	}
}
