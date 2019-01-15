// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebRTCProxy : ModuleRules
{
	public WebRTCProxy(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			});

        var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/include/Win64/VS2017"));
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/include/Win64/VS2017/third_party/jsoncpp/source/include"));

        PublicLibraryPaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/lib/Win64/VS2017/release"));

        PublicAdditionalLibraries.Add("json.lib");
        PublicAdditionalLibraries.Add("webrtc.lib");
        PublicAdditionalLibraries.Add("webrtc_opus.lib");
        PublicAdditionalLibraries.Add("audio_decoder_opus.lib");
        PublicAdditionalLibraries.Add("Msdmo.lib");
        PublicAdditionalLibraries.Add("Dmoguids.lib");
        PublicAdditionalLibraries.Add("wmcodecdspuuid.lib");
        PublicAdditionalLibraries.Add("Secur32.lib");

        bEnableExceptions = true;
    }
}
