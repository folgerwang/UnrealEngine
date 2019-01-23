// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using Tools.DotNETCommon;

namespace UnrealBuildTool.Rules
{
    public class PixelStreaming : ModuleRules
    {
        private void AddWebRTCProxy()
        {
            string PixelStreamingProgramsDirectory = "./Programs/PixelStreaming";
            string WebRTCProxyDir = PixelStreamingProgramsDirectory + "/WebRTCProxy/bin";

            if (!Directory.Exists(WebRTCProxyDir))
            {
                Log.TraceInformation(string.Format("WebRTC Proxy path '{0}' does not exist", WebRTCProxyDir));
                return;
            }

            List<string> DependenciesToAdd = new List<string>();
            DependenciesToAdd.AddRange(Directory.GetFiles(WebRTCProxyDir, "WebRTCProxy.exe"));
            DependenciesToAdd.AddRange(Directory.GetFiles(WebRTCProxyDir, "WebRTCProxy.pdb"));
            DependenciesToAdd.AddRange(Directory.GetFiles(WebRTCProxyDir, "*.bat"));
            DependenciesToAdd.AddRange(Directory.GetFiles(WebRTCProxyDir, "*.ps1"));
            
            foreach(string Dependency in DependenciesToAdd)
            {
                RuntimeDependencies.Add(Dependency, StagedFileType.NonUFS);
            }
        }

        private void AddSignallingServer()
        {
            string PixelStreamingProgramsDirectory = "./Programs/PixelStreaming";
            string SignallingServerDir = new DirectoryInfo(PixelStreamingProgramsDirectory + "/WebServers/SignallingWebServer").FullName;

            if (!Directory.Exists(SignallingServerDir))
            {
                Log.TraceInformation(string.Format("Signalling Server path '{0}' does not exist", SignallingServerDir));
                return;
            }

            List<string> DependenciesToAdd = new List<string>();
            DependenciesToAdd.AddRange(Directory.GetFiles(SignallingServerDir, "*.*", SearchOption.AllDirectories));

            string NodeModulesDirPath = new DirectoryInfo(SignallingServerDir + "/node_modules").FullName;
            string LogsDirPath = new DirectoryInfo(SignallingServerDir + "/logs").FullName;
            foreach (string Dependency in DependenciesToAdd)
            {
                if (!Dependency.StartsWith(NodeModulesDirPath) &&
                    !Dependency.StartsWith(LogsDirPath))
                {
                    RuntimeDependencies.Add(Dependency, StagedFileType.NonUFS);
                }
            }
        }

        private void AddMatchmakingServer()
        {
			string PixelStreamingProgramsDirectory = "./Programs/PixelStreaming";
            string MatchmakingServerDir = new DirectoryInfo(PixelStreamingProgramsDirectory + "/WebServers/Matchmaker").FullName;

            if (!Directory.Exists(MatchmakingServerDir))
            {
                Log.TraceInformation(string.Format("Matchmaking Server path '{0}' does not exist", MatchmakingServerDir));
                return;
            }

            List<string> DependenciesToAdd = new List<string>();
            DependenciesToAdd.AddRange(Directory.GetFiles(MatchmakingServerDir, "*.*", SearchOption.AllDirectories));

            string NodeModulesDirPath = new DirectoryInfo(MatchmakingServerDir + "/node_modules").FullName;
            string LogsDirPath = new DirectoryInfo(MatchmakingServerDir + "/logs").FullName;
            foreach (string Dependency in DependenciesToAdd)
            {
                if (!Dependency.StartsWith(NodeModulesDirPath) &&
                    !Dependency.StartsWith(LogsDirPath))
                {
                    RuntimeDependencies.Add(Dependency, StagedFileType.NonUFS);
                }
            }
        }

		private void AddWebRTCServers()
        {
            string webRTCRevision = "23789";
            string webRTCRevisionDirectory = "./ThirdParty/WebRTC/rev." + webRTCRevision;
			string webRTCProgramsDirectory = System.IO.Path.Combine(webRTCRevisionDirectory, "programs/Win64/VS2017/release");

            List<string> DependenciesToAdd = new List<string>();
            DependenciesToAdd.AddRange(Directory.GetFiles(webRTCProgramsDirectory, "*.exe"));
            DependenciesToAdd.AddRange(Directory.GetFiles(webRTCProgramsDirectory, "*.pdb"));
            DependenciesToAdd.AddRange(Directory.GetFiles(webRTCProgramsDirectory, "*.bat"));
            DependenciesToAdd.AddRange(Directory.GetFiles(webRTCProgramsDirectory, "*.ps1"));

            foreach (string Dependency in DependenciesToAdd)
            {
                RuntimeDependencies.Add(Dependency, StagedFileType.NonUFS);
            }
        }

        public PixelStreaming(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.Add(ModuleDirectory);
            PrivateIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../ThirdParty"));

			// NOTE: General rule is not to access the private folder of another module,
			// but to use the ISubmixBufferListener interface, we  need to include some private headers
            PrivateIncludePaths.Add(System.IO.Path.Combine(Directory.GetCurrentDirectory(), "./Runtime/AudioMixer/Private"));

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
					"ApplicationCore",
					"Core",
                    "CoreUObject",
                    "Engine",
					"InputCore",
                    "InputDevice",
					"Json",
					"RenderCore",
                    "AnimGraphRuntime",
                    "RHI",
					"Slate",
					"SlateCore",
					"Sockets",
					"Networking"
				}
			);

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Slate",
                    "SlateCore",
                    "AudioMixer",
					"Json"
                }
			);

            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });
				PrivateIncludePaths.AddRange(
					new string[] {
					"../../../../Source/Runtime/Windows/D3D11RHI/Private",
					"../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
					});
				AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
			}

			AddWebRTCProxy();
            AddSignallingServer();
            AddMatchmakingServer();
            AddWebRTCServers();

        }
    }
}
