// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioMixer : ModuleRules
	{
		public AudioMixer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.Add("TargetPlatform");

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AudioMixer/Private",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus",
					"UELibSampleRate"
					);

			// TODO test this for HTML5 !
			//if (Target.Platform == UnrealTargetPlatform.HTML5)
			//{
			//	AddEngineThirdPartyPrivateStaticDependencies(Target,
			//		"UEOgg",
			//		"Vorbis",
			//		"VorbisFile"
			//		);
			//}

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"libOpus"
					);
				PublicFrameworks.AddRange(new string[] { "AVFoundation", "CoreVideo", "CoreMedia" });
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile"
					);
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus"
					);
			}

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"libOpus"
					);
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				string LibSndFilePath = Target.UEThirdPartyBinariesDirectory + "libsndfile/";
				LibSndFilePath += Target.Platform == UnrealTargetPlatform.Win32
					? "Win32"
					: "Win64";
					
				PublicAdditionalLibraries.Add("libsndfile-1.lib");
				PublicDelayLoadDLLs.Add("libsndfile-1.dll");
				PublicIncludePathModuleNames.Add("UELibSampleRate");
				PublicLibraryPaths.Add(LibSndFilePath);
			}
		}
	}
}
